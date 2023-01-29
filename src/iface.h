#pragma once

#include <net/if.h>
#include <stddef.h>

// types

typedef enum {
	IFACE_KIND_OTHER = 0,
	IFACE_KIND_LOOP  = 1,
	IFACE_KIND_ETH   = 2,
	IFACE_KIND_USB   = 3,
} iface_kind_t;

typedef struct {
	iface_kind_t kind;
	char* name;

	int dgram_sock;
	struct ifreq ifr;
} iface_t;

// functions

char* iface_err_str(void);
int iface_list(iface_t** ifaces_ref, size_t* count_ref);
void iface_free(iface_t* ifaces, size_t iface_count);
