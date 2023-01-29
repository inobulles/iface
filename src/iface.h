#pragma once

#include <net/if.h>
#include <stddef.h>

// enums

typedef enum {
	IFACE_KIND_OTHER = 0,
	IFACE_KIND_LOOP  = 1,
	IFACE_KIND_ETH   = 2,
	IFACE_KIND_USB   = 3,
} iface_kind_t;

// this is not especially necessary, but it does make the API more cohesive
// it is important however that these follow the same order as the BSD definitions, so we can't simply redefine them

typedef enum {
	#define MAP(name) IFACE_##name = IFF_##name,

	MAP(UP)
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
} iface_flag_t;

// structs

typedef struct {
	iface_kind_t kind;
	char* name;

	int dgram_sock;
	struct ifreq ifr;

	// 'AF_INET/6' interfaces

	iface_flag_t flags;
	char* ip;
	char* netmask;
} iface_t;

// functions

char* iface_err_str(void);

int iface_list(iface_t** ifaces_ref, size_t* count_ref);
void iface_free(iface_t* ifaces, size_t iface_count);

int iface_get_flags(iface_t* iface);
int iface_get_ip(iface_t* iface);
int iface_get_netmask(iface_t* iface);
