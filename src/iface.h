#pragma once

#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <stddef.h>
#include <sys/socket.h>

// the underlying type of the interface

typedef enum {
	IFACE_KIND_OTHER = 0,
	IFACE_KIND_LOOP  = 1,
	IFACE_KIND_ETH   = 2,
	IFACE_KIND_USB   = 3,
} iface_kind_t;

// this is not especially necessary, but it does make the API more cohesive
// it is important however that these follow the same order as the BSD definitions, so we can't simply redefine them

typedef enum {
#define MAP(name) IFACE_FLAG_##name = IFF_##name,

	MAP(BROADCAST)
	MAP(DEBUG)
	MAP(LOOPBACK)
	MAP(POINTOPOINT)
	MAP(KNOWSEPOCH)
	MAP(DRV_RUNNING)
	MAP(NOARP)
	MAP(PROMISC)
	MAP(ALLMULTI)
	MAP(DRV_OACTIVE)
	MAP(SIMPLEX)
	MAP(LINK0)
	MAP(LINK1)
	MAP(LINK2)
	MAP(ALTPHYS)
	MAP(MULTICAST)
	MAP(CANTCONFIG)
	MAP(PPROMISC)
	MAP(MONITOR)
	MAP(STATICARP)
	MAP(DYING)
	MAP(RENAMING)
	MAP(NOGROUP)

#undef MAP

	IFACE_FLAG_ONLINE = IFF_UP,
} iface_flag_t;

// similar to 'sa_family_t'
// many BSD-specific address families (e.g. 'AF_LINK' for link-layer interfaces) don't concern us, so just include the POSIX ones
// some address families have been renamed for discoverability (e.g. 'AF_INET' -> 'IFACE_FAMILY_IPV4')
// the order of these families have some importance, although we don't *really* have control over it
// basically, we prefer families with higher numbers (so e.g. IPv6 would be preferred to IPv4)

typedef enum {
#define MAP(name) IFACE_FAMILY_##name = AF_##name,

	MAP(UNSPEC) // explicitly unspecified
	MAP(UNIX)   // local to host (pipes, portals)

#undef MAP

	IFACE_FAMILY_IPV4 = AF_INET,
	IFACE_FAMILY_IPV6 = AF_INET6,
} iface_family_t;

// structs

typedef struct {
	char* name;

	iface_kind_t kind;
	iface_family_t family;

	int dgram_sock;

	union {
		struct     ifreq     ifr;
		struct in6_ifreq in6_ifr;
	};

	iface_flag_t flags;

	// IPv4

	char* ip;
	char* netmask;

	// IPv6

	size_t ipv6_count;
	char** ipv6s;
} iface_t;

// functions

char* iface_err_str(void);

char* iface_str_kind(iface_kind_t kind);
char* iface_str_family(iface_family_t family);

iface_t* iface_new(char* name);
int iface_upgrade_family(iface_t* iface, iface_family_t family);
void iface_free(iface_t* iface);

int iface_list(iface_t*** ifaces_ref, size_t* count_ref);
void iface_list_free(iface_t** ifaces, size_t iface_count);

int iface_get_flags(iface_t* iface);
int iface_get_ip(iface_t* iface);
int iface_get_netmask(iface_t* iface);

int iface_set_flags(iface_t* iface);

int iface_create(iface_t* iface);
int iface_destroy(iface_t* iface);
