#include <errno.h>
#include <iface.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

static char* err_str = NULL; // there's no real point in providing a library function to free this once a program is done - it's a global anyway

char* iface_err_str(void) {
	return err_str;
}

static int __emit_err(char* fmt, ...) {
	if (err_str) {
		free(err_str);
	}

	va_list args;
	va_start(args, fmt);

	if (vasprintf(&err_str, fmt, args)) {}

	va_end(args);

	return -1;
}

int iface_list(iface_t** ifaces_ref, size_t* iface_count_ref) {
	// check references for NULL-pointers

	if (!ifaces_ref) {
		return __emit_err("'ifaces_ref' argument points to NULL");
	}

	if (!iface_count_ref) {
		return __emit_err("'iface_count_ref' argument points to NULL");
	}

	// get linked list of interfaces

	struct ifaddrs* ifaddr;

	if (getifaddrs(&ifaddr) < 0) {
		return __emit_err("getifaddrs: %s", strerror(errno));
	}

	// create list

	size_t iface_count = 0;
	iface_t* ifaces = NULL;

	// traverse through interfaces linked list

	for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}

		sa_family_t const family = ifa->ifa_addr->sa_family;

		if (family != AF_LINK) { // we're looking for link interfaces here
			continue;
		}

		// get interface type

		struct sockaddr_dl* const sdl = (void*) ifa->ifa_addr;
		iface_kind_t kind = IFACE_KIND_OTHER;

		if (sdl->sdl_type == IFT_LOOP) {
			kind = IFACE_KIND_LOOP;
		}

		else if (sdl->sdl_type == IFT_USB) {
			kind = IFACE_KIND_USB;
		}

		else if (sdl->sdl_type == IFT_ETHER) {
			kind = IFACE_KIND_ETH;
		}

		else {
			fprintf(stderr, "TODO iface_list: unknown interface type for %s: %d\n", ifa->ifa_name, sdl->sdl_type);
		}

		// create interface object

		ifaces = realloc(ifaces, ++iface_count * sizeof *ifaces);

		iface_t* const iface = &ifaces[iface_count - 1];
		memset(iface, 0, sizeof *iface);

		iface->kind = kind;

		// copy name
		// we could directly point 'iface->name' to 'iface->ifr.ifr_name', but that's unstable as a future call to 'realloc' may shift us and make this pointer bad

		strncpy(iface->ifr.ifr_name, ifa->ifa_name, MIN(sizeof iface->ifr.ifr_name, sizeof ifa->ifa_name));
		iface->name = strdup(iface->ifr.ifr_name);

		iface->ifr.ifr_addr.sa_family = AF_INET;
		iface->dgram_sock = socket(iface->ifr.ifr_addr.sa_family, SOCK_DGRAM, 0);

		if (iface->dgram_sock < 0) {
			__emit_err("failed to create datagram socket for interface %s: %s", iface->name, strerror(errno));

			iface_free(ifaces, iface_count);
			return -1;
		}
	}

	// set our references

	*ifaces_ref = ifaces;
	*iface_count_ref = iface_count;

	return 0;
}

void iface_free(iface_t* ifaces, size_t iface_count) {
	for (size_t i = 0; i < iface_count; i++) {
		iface_t* const iface = &ifaces[i];

		if (iface->name) {
			free(iface->name);
		}

		if (iface->dgram_sock) {
			close(iface->dgram_sock);
		}
	}

	free(ifaces);
}
