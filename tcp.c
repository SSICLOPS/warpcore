#include <arpa/inet.h>

#include "warpcore.h"
#include "tcp.h"


// Use a spare iov to transmit an ARP query for the given destination
// IP address.
static void
tcp_tx_syn(struct w_sock * const s)
{
	// grab a spare buffer
	struct w_iov * const v = STAILQ_FIRST(&s->w->iov);
	if (unlikely(v == 0))
		die("out of spare bufs");
	STAILQ_REMOVE_HEAD(&s->w->iov, next);

	// copy template header into buffer and fill in remaining fields
	v->buf = IDX2BUF(s->w, v->idx);
	memcpy(v->buf, s->hdr, s->hdr_len);

	struct tcp_hdr * const tcp = (struct tcp_hdr *)
		(v->buf + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));

	tcp->flags = SYN;

	// send the IP packet
	ip_tx(s->w, v, sizeof(struct tcp_hdr));
	w_kick_tx(s->w);

	// make iov available again
	STAILQ_INSERT_HEAD(&s->w->iov, v, next);
}


void
tcp_rx(struct warpcore * const w, char * const buf, const uint16_t off,
       const uint16_t len, const uint32_t src)
{
	const struct tcp_hdr * const tcp =
		(const struct tcp_hdr * const)(buf + off);

	dlog(info, "TCP :%d -> :%d, flags %d, len %ld", ntohs(tcp->sport),
	     ntohs(tcp->dport), tcp->flags, len - sizeof(struct tcp_hdr));
}


void
tcp_tx(struct w_sock * const s)
{
	dlog(debug, "state %d", s->cb->state);

	switch (s->cb->state) {
	case CLOSED:
		tcp_tx_syn(s);
		s->cb->state = SYN_SENT;
		break;
	default:
		die("unknown TCP state %d", s->cb->state);
	}
}
