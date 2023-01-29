#include <arpa/inet.h>
#include <errno.h>
#include <iface.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioccom.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/types.h>
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

		// small explanations on families:
		// 'AF_LINK' family interfaces are interfaces from the POV of the link-layer
		// 'AF_INET/6' family interfaces are interfaces from the POV of the IP-layer

		sa_family_t const family = ifa->ifa_addr->sa_family;

		// if interface with same name already exits, just add the following information to it rather than creating a new entry

		iface_t* iface;

		for (size_t i = 0; i < iface_count; i++) {
			iface = &ifaces[i];

			if (!strcmp(iface->name, ifa->ifa_name)) {
				goto found;
			}
		}

		// interface not found
		// create it

		ifaces = realloc(ifaces, ++iface_count * sizeof *ifaces);

		iface = &ifaces[iface_count - 1];
		memset(iface, 0, sizeof *iface);

		iface->ifr.ifr_addr.sa_family = family;

		// copy name
		// we could directly point 'iface->name' to 'iface->ifr.ifr_name', but that's unstable as a future call to 'realloc' may shift us and make this pointer bad

		strncpy(iface->ifr.ifr_name, ifa->ifa_name, MIN(sizeof iface->ifr.ifr_name, sizeof ifa->ifa_name));
		iface->name = strdup(iface->ifr.ifr_name);

found:

		// link-layer interfaces will tell us the kind of interface we're dealing with

		if (family == AF_LINK) {
			struct sockaddr_dl* const sdl = (void*) ifa->ifa_addr;
			iface->kind = IFACE_KIND_OTHER;

			if (sdl->sdl_type == IFT_LOOP) {
				iface->kind = IFACE_KIND_LOOP;
			}

			else if (sdl->sdl_type == IFT_USB) {
				iface->kind = IFACE_KIND_USB;
			}

			else if (sdl->sdl_type == IFT_ETHER) {
				iface->kind = IFACE_KIND_ETH;
			}

			else {
				fprintf(stderr, "TODO iface_list: unknown interface type for %s: %d\n", ifa->ifa_name, sdl->sdl_type);
			}
		}

		// IP-layer interfaces will allow us to perform operations on them through a datagram socket

		else if (family == AF_INET || family == AF_INET6) {
			// don't open a second socket if we already have one
			// TODO what if we want both IPv4 & IPv6 on a socket? should we force a choice to be made?

			if (!iface->dgram_sock) {
				iface->dgram_sock = socket(family, SOCK_DGRAM, 0);

				if (iface->dgram_sock < 0) {
					return __emit_err("failed to create datagram socket for interface %s: %s", iface->name, strerror(errno));
				}
			}
		}

		else {
			__emit_err("unknown address family %d", family);
			goto err;
		}
	}

	// set our references

	*ifaces_ref = ifaces;
	*iface_count_ref = iface_count;

	return 0;

err:

	iface_free(ifaces, iface_count);
	return -1;
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

		// 'AF_INET/6' interfaces

		if (iface->ip) {
			free(iface->ip);
		}

		if (iface->netmask) {
			free(iface->netmask);
		}
	}

	free(ifaces);
}

int iface_get_flags(iface_t* iface) {
	// make private copy of 'ifr' because 'SIOCGIFLAGS' scribbles on its union portion

	struct ifreq ifr;
	memcpy(&ifr, &iface->ifr, sizeof ifr);

	// actually get flags

	if (ioctl(iface->dgram_sock, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCGIFFLAGS): %s", iface->name, strerror(errno));
	}

	iface->flags = ifr.ifr_flags;

	return 0;
}

int iface_get_ip(iface_t* iface) {
	if (ioctl(iface->dgram_sock, SIOCGIFADDR, &iface->ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCGIFADDR): %s", iface->name, strerror(errno));
	}

	struct sockaddr_in* const ip = (void*) &iface->ifr.ifr_addr;
	iface->ip = strdup(inet_ntoa(ip->sin_addr));

	if (!iface->ip) {
		return __emit_err("inet_ntoa('%s'): %s", iface->name, strerror(errno));
	}

	return 0;
}

int iface_get_netmask(iface_t* iface) {
	if (ioctl(iface->dgram_sock, SIOCGIFNETMASK, &iface->ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCGIFNETMASK): %s", iface->name, strerror(errno));
	}

	struct sockaddr_in* const netmask = (void*) &iface->ifr.ifr_addr;
	iface->netmask = strdup(inet_ntoa(netmask->sin_addr));

	if (!iface->netmask) {
		return __emit_err("inet_ntoa('%s'): %s", iface->name, strerror(errno));
	}

	return 0;
}
