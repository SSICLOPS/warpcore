#include <arpa/inet.h>

#include "udp.h"
#include "debug.h"
#include "icmp.h"
#include "warpcore.h"


void udp_rx(const struct warpcore * const w,
            char * const buf, const uint_fast16_t off)
{
	const struct udp_hdr * const udp = (struct udp_hdr * const)(buf + off);
	const uint_fast16_t sport = ntohs(udp->sport);
	const uint_fast16_t dport = ntohs(udp->dport);
	const uint_fast16_t len = ntohs(udp->len);

	D("UDP :%d -> :%d, len %d", sport, dport, len);

	if (w->udp[dport]) {
		// nobody bound to this port locally
		icmp_tx_unreach(w, ICMP_UNREACH_PORT, buf, off);
	} else {
		D("this is for us!");
	}
}
