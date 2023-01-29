#include <err.h>
#include <iface.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef int (*action_t) (opts_t* opts);;

static int do_stat(opts_t* opts) {
	for (size_t i = 0; i < opts->iface_count; i++) {
		iface_t* const iface = &opts->ifaces[i];
		char* ip = "n/a";

		if (!iface_get_ip(iface)) {
			ip = iface->ip;
		}

		iface_get_flags(iface);

		printf("%s\t%s\t%s\n", iface->name, iface->flags & IFACE_UP ? "ONLINE" : "OFFLINE", ip);
	}

	return 0;
}

int main(int argc, char* argv[]) {
	action_t action = do_stat;
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

	// TODO parse commands

	// list interfaces
	// we do this regardless of the command, as even if we're not listing interfaces, we still need to be able to search through them by name

	if (iface_list(&opts.ifaces, &opts.iface_count) < 0) {
		errx(EXIT_FAILURE, "failed to list interfaces: %s", iface_err_str());
	}

	// take action

	int rv = action(&opts);

	// no need to free interfaces

	return rv < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
