#ifndef _udp_h_
#define _udp_h_

#include <stdint.h>
#include <stdbool.h>


struct udp_hdr {
	uint16_t sport;		// source port
	uint16_t dport;		// destination port
	uint16_t len;		// udp length
	uint16_t cksum;		// udp checksum
} __packed __aligned(4);


struct warpcore;
struct w_sock;
struct w_iov;

extern void
udp_rx(struct warpcore * const w, char * const buf, const uint16_t off,
       const uint32_t ip);

extern bool
udp_tx(const struct w_sock * const s, struct w_iov * const v);


#endif
