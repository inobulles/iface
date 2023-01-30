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

// globals

static char* err_str = NULL; // there's no real point in providing a library function to free this once a program is done - it's a global anyway

// error handling

char* iface_err_str(void) {
	char* const rv = err_str;
	err_str = NULL;

	return rv;
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

// stringification functions

char* iface_str_kind(iface_kind_t kind) {
	if (kind == IFACE_KIND_LOOP) return "loop";
	if (kind == IFACE_KIND_ETH ) return "eth";
	if (kind == IFACE_KIND_USB ) return "usb";

	return "other";
}

char* iface_str_family(iface_family_t family) {
	if (family == IFACE_FAMILY_UNSPEC) return "unspecified";
	if (family == IFACE_FAMILY_UNIX  ) return "UNIX";
	if (family == IFACE_FAMILY_IPV4  ) return "IPv4";
	if (family == IFACE_FAMILY_IPV6  ) return "IPv6";

	return "other";
}

// interface object handling

iface_t* iface_new(char* name) {
	iface_t* const iface = calloc(1, sizeof *iface);

	iface->name = strdup(name);
	iface->family = IFACE_FAMILY_UNSPEC;

	strncpy(iface->ifr.ifr_name, name, sizeof iface->ifr.ifr_name);

	return iface;
}

int iface_upgrade_family(iface_t* iface, iface_family_t family) {
	if (iface->family == family) {
		return 0;
	}

	iface->family = family;

	// we use the 'in6_ifr' member for IPv6, 'ifr' for anything else

	if (family == IFACE_FAMILY_IPV6) {
		iface->in6_ifr.ifr_addr.sin6_family = (sa_family_t) family;
	}

	else {
		iface->ifr.ifr_addr.sa_family = (sa_family_t) family;
	}

	// create datagram socket

	int const dgram_sock = socket((sa_family_t) family, SOCK_DGRAM, 0);

	if (dgram_sock < 0) {
		return __emit_err("failed to create '%s' datagram socket: %s", iface_str_family(family), strerror(errno));
;
	}

	// if we already had one, close it
	// XXX this isn't super ideal because that means we're gonna be opening/closing a lot of sockets if we progressively upgrade the interface's family, but w/e

	if (iface->dgram_sock > 0) {
		close(iface->dgram_sock);
	}

	iface->dgram_sock = dgram_sock;

	return 0;
}

void iface_free(iface_t* iface) {
	if (iface->name) {
		free(iface->name);
	}

	if (iface->dgram_sock > 0) {
		close(iface->dgram_sock);
	}

	if (iface->ip) {
		free(iface->ip);
	}

	if (iface->netmask) {
		free(iface->netmask);
	}

	for (size_t i = 0; i < iface->ipv6_count; i++) {
		free(iface->ipv6s[i]);
	}

	if (iface->ipv6s) {
		free(iface->ipv6s);
	}

	free(iface->name);
}

// interface listing

int iface_list(iface_t*** ifaces_ref, size_t* iface_count_ref) {
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
	iface_t** ifaces = NULL;

	// traverse through interfaces linked list

	for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}

		// small explanations on families:
		// 'AF_LINK' family interfaces are interfaces from the POV of the link-layer
		// 'AF_INET/6' family interfaces are interfaces from the POV of the IP-layer

		sa_family_t const af = ifa->ifa_addr->sa_family;
		iface_family_t const family = af;

		// if interface with same name already exits, just add the following information to it rather than creating a new entry

		iface_t* iface;

		for (size_t i = 0; i < iface_count; i++) {
			iface = ifaces[i];

			if (!strcmp(iface->name, ifa->ifa_name)) {
				goto found;
			}
		}

		// interface not found
		// create it

		iface = iface_new(ifa->ifa_name);

		ifaces = realloc(ifaces, ++iface_count * sizeof *ifaces);
		ifaces[iface_count - 1] = iface;

		// copy name
		// we could directly point 'iface->name' to 'iface->ifr.ifr_name', but that's unstable as a future call to 'realloc' may shift us and make this pointer bad

		strncpy(iface->ifr.ifr_name, ifa->ifa_name, MIN(sizeof iface->ifr.ifr_name, sizeof ifa->ifa_name));
		iface->name = strdup(iface->ifr.ifr_name);

found:

		// link-layer interfaces will tell us the underlying kind of interface we're dealing with

		if (af == AF_LINK) {
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

		// (attempt to) upgrade the interface family if IPv4/6 interface

		else if (family == IFACE_FAMILY_IPV4 || family == IFACE_FAMILY_IPV6) {
			if (iface_upgrade_family(iface, family) < 0) {
				goto err;
			}

			// if IPv6, add IP address
			// with IPv6, we can have multiple IP's per interface

			if (family == IFACE_FAMILY_IPV6) {
				struct sockaddr_in6 const* const ip = (void*) ifa->ifa_addr;
				char buf[40];

				if (!inet_ntop(AF_INET6, &ip->sin6_addr, buf, sizeof buf)) {
					__emit_err("inet_ntop(AF_INET6, '%s'): %s", iface->name, strerror(errno));
					goto err;
				}

				iface->ipv6s = realloc(iface->ipv6s, ++iface->ipv6_count * sizeof *iface->ipv6s);
				iface->ipv6s[iface->ipv6_count - 1] = strdup(buf);
			}
		}

		// error otherwise

		else {
			__emit_err("unknown interface family '%s' (%d)", iface_str_family(family), family);
			goto err;
		}
	}

	// set our references

	*ifaces_ref = ifaces;
	*iface_count_ref = iface_count;

	return 0;

err:

	iface_list_free(ifaces, iface_count);
	return -1;
}

void iface_list_free(iface_t** ifaces, size_t iface_count) {
	for (size_t i = 0; i < iface_count; i++) {
		iface_free(ifaces[i]);
	}

	free(ifaces);
}

// getters

int iface_get_flags(iface_t* iface) {
	// make private copy of 'ifr' because 'SIOCGIFLAGS' scribbles on its union portion

	struct ifreq ifr;
	memcpy(&ifr, &iface->ifr, sizeof ifr);

	// actually get flags

	if (ioctl(iface->dgram_sock, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCGIFFLAGS): %s", iface->name, strerror(errno));
	}

	iface->flags = ifr.ifr_flags;
	iface->ifr.ifr_flags = iface->flags;

	return 0;
}

static int __iface_get_ipv4(iface_t* iface) {
	if (ioctl(iface->dgram_sock, SIOCGIFADDR, &iface->ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCGIFADDR): %s", iface->name, strerror(errno));
	}

	struct sockaddr_in const* const ip = (void*) &iface->ifr.ifr_addr;
	char* buf = inet_ntoa(ip->sin_addr);

	if (!buf) {
		return __emit_err("inet_ntoa('%s'): %s", iface->name, strerror(errno));
	}

	if (iface->ip) {
		free(iface->ip);
	}

	iface->ip = strdup(buf);

	return 0;
}

static int __iface_get_ipv6(iface_t* iface) {
	// 'SIOCGIFADDR_IN6' is deprecated and returns an 'EADDRNOTAVAIL' error each time (cf. 'sys/netinet6/in6.c')
	// besides, we should steer away from ioctl's as they have some limitations (such as not being able to convey multiple addresses on one interface)

	if (iface->ipv6_count <= 0) {
		return __emit_err("no IPv6 addresses");
	}

	iface->ip = iface->ipv6s[iface->ipv6_count - 1];

	return 0;
}

int iface_get_ip(iface_t* iface) {
	if (iface->family == IFACE_FAMILY_IPV4) {
		return __iface_get_ipv4(iface);
	}

	if (iface->family == IFACE_FAMILY_IPV6) {
		return __iface_get_ipv6(iface);
	}

	return __emit_err("unsupported family '%s' for getting IP address", iface_str_family(iface->family));
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

// setters

int iface_set_flags(iface_t* iface) {
	iface->ifr.ifr_flags = iface->flags;

	if (ioctl(iface->dgram_sock, SIOCSIFFLAGS, (caddr_t) &iface->ifr) < 0) {
		return __emit_err("ioctl('%s', SIOCSIFFLAGS): %s", iface->name, strerror(errno));
	}

	return 0;
}

// interface creation

int iface_create(iface_t* iface) {
	struct ifreq ifr = { 0 };
	strncpy(ifr.ifr_name, iface->name, sizeof ifr.ifr_name);

#ifdef SIOCIFCREATE2
	unsigned long const req = SIOCIFCREATE2;
	char const* const req_str = "SIOCIFCREATE2";
#else
	unsigned long const req = SIOCIFCREATE;
	char const* const req_str = "SIOCIFCREATE";
#endif

	if (!ioctl(iface->dgram_sock, req, &ifr)) {
		memcpy(&iface->ifr, &ifr, sizeof ifr);
		return 0;
	}

	if (errno == EEXIST) {
		return __emit_err("ioctl('%s', %s): Interface already exists", iface->name, req_str, strerror(errno));
	}

	return __emit_err("ioctl('%s', %s): %s", iface->name, req_str, strerror(errno));
}
