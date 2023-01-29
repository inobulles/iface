#include <err.h>
#include <iface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
	fprintf(stderr,
		"usage: %1$s [-v]\n",
	getprogname());

	exit(EXIT_FAILURE);
}

typedef struct {
	iface_t* ifaces;
	size_t iface_count;
} opts_t;

static char* strkind(iface_kind_t kind) {
	if (kind == IFACE_KIND_LOOP) return "loop";
	if (kind == IFACE_KIND_ETH ) return "eth";
	if (kind == IFACE_KIND_USB ) return "usb";

	return "other";
}

static int do_stat(opts_t* opts) {
	for (size_t i = 0; i < opts->iface_count; i++) {
		iface_t* const iface = &opts->ifaces[i];
		char* ip = "n/a";

		if (!iface_get_ip(iface)) {
			ip = iface->ip;
		}

		iface_get_flags(iface);

		printf("%s (%s)\t%s\t%s\n", iface->name, strkind(iface->kind), iface->flags & IFACE_UP ? "ONLINE" : "OFFLINE", ip);
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
		return do_stat(&opts) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	// parse instructions

	while (argc --> 0) {
		char* const instr = *argv++;
		int rv = EXIT_FAILURE; // I'm a pessimist

		if (!strcmp(instr, "list")) {
			rv = do_stat(&opts);
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
