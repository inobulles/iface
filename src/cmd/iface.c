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

int main(int argc, char* argv[]) {
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

	return EXIT_SUCCESS;
}
