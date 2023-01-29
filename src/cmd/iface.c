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
	(void) opts;
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

	// populate interface list
	// we do this regardless of the command, as even if we're not listing interfaces, we still need to be able to search through them by name

	// take action

	int rv = action(&opts);

	return rv < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
