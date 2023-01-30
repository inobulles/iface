#include <arpa/inet.h>
#include <err.h>
#include <iface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
	fprintf(stderr,
		"usage: %1$s [list]\n"
		"       %1$s disable interface\n"
		"       %1$s enable interface\n",
	getprogname());

	exit(EXIT_FAILURE);
}

typedef struct {
	iface_t** ifaces;
	size_t iface_count;
} opts_t;

static iface_t* search_iface(opts_t* opts, char const* name) {
	if (!name) {
		usage();
	}

	for (size_t i = 0; i < opts->iface_count; i++) {
		iface_t* const iface = opts->ifaces[i];

		if (!strcmp(iface->name, name)) {
			return iface;
		}
	}

	errx(EXIT_FAILURE, "interface '%s' not found", name);
}

// instructions
// TODO I'd like this to be in a separate file

static int do_create(opts_t* opts, int* argc_ref, char*** argv_ref) {
	char* const name = ((*argc_ref)--, *((*argv_ref)++));
	(void) opts;

	iface_t* const iface = iface_new(name);

	if (!iface) {
		errx(EXIT_FAILURE, "iface_new('%s'): %s", name, iface_err_str());
	}

	if (iface_upgrade_family(iface, IFACE_FAMILY_IPV6) < 0) {
		errx(EXIT_FAILURE, "iface_upgrade_family('%s', IFACE_FAMILY_IPV6): %s", name, iface_err_str());
	}

	if (iface_create(iface) < 0) {
		errx(EXIT_FAILURE, "iface_create('%s'): %s", name, iface_err_str());
	}

	iface_free(iface);

	return 0;
}

static int do_disable(opts_t* opts, int* argc_ref, char*** argv_ref) {
	char* const name = ((*argc_ref)--, *((*argv_ref)++));
	iface_t* const iface = search_iface(opts, name);

	if (iface_get_flags(iface) < 0) {
		errx(EXIT_FAILURE, "iface_get_flags('%s'): %s", iface->name, iface_err_str());
	}

	iface->flags &= ~IFACE_FLAG_ONLINE;

	if (iface_set_flags(iface) < 0) {
		errx(EXIT_FAILURE, "iface_set_flags('%s'): %s", iface->name, iface_err_str());
	}

	return 0;
}

static int do_enable(opts_t* opts, int* argc_ref, char*** argv_ref) {
	char* const name = ((*argc_ref)--, *((*argv_ref)++));
	iface_t* const iface = search_iface(opts, name);

	if (iface_get_flags(iface) < 0) {
		errx(EXIT_FAILURE, "iface_get_flags('%s'): %s", iface->name, iface_err_str());
	}

	iface->flags |= IFACE_FLAG_ONLINE;

	if (iface_set_flags(iface) < 0) {
		errx(EXIT_FAILURE, "iface_set_flags('%s'): %s", iface->name, iface_err_str());
	}

	return 0;
}

static int do_list(opts_t* opts) {
	for (size_t i = 0; i < opts->iface_count; i++) {
		iface_t* const iface = opts->ifaces[i];
		char* ip = "n/a";

		if (!iface_get_ip(iface)) {
			ip = iface->ip;
		}

		iface_get_flags(iface);

		printf("%s (%s)\t%s\t%s (%s)\n",
			iface->name, iface_str_kind(iface->kind),
			iface->flags & IFACE_FLAG_ONLINE ? "ONLINE" : "OFFLINE",
			ip, iface_str_family(iface->family)
		);
	}

	return 0;
}

int main(int argc, char* argv[]) {
	opts_t opts = { 0 };

	// get options

	int c;

	while ((c = getopt(argc, argv, "")) >= 0) {
		if (0) {}

		else {
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	// list interfaces
	// we do this regardless of the instruction, as even if we're not listing interfaces, we still need to be able to search through them by name

	if (iface_list(&opts.ifaces, &opts.iface_count) < 0) {
		errx(EXIT_FAILURE, "failed to list interfaces: %s", iface_err_str());
	}

	// if no instructions are passed, assume we just want to list interfaces

	if (argc <= 0) {
		return do_list(&opts) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	// parse instructions

	while (argc --> 0) {
		char* const instr = *argv++;
		int rv = EXIT_FAILURE; // I'm a pessimist

		if (!strcmp(instr, "create")) {
			rv = do_create(&opts, &argc, &argv);
		}

		else if (!strcmp(instr, "disable")) {
			rv = do_disable(&opts, &argc, &argv);
		}

		else if (!strcmp(instr, "enable")) {
			rv = do_enable(&opts, &argc, &argv);
		}

		else if (!strcmp(instr, "list")) {
			rv = do_list(&opts);
		}

		else {
			usage();
		}

		// stop here if there was an error in the execution of an instruction

		if (rv != EXIT_SUCCESS) {
			return rv;
		}
	}

	// no need to free interfaces

	return EXIT_SUCCESS;
}
