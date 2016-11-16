#include <arpa/inet.h>
#include <poll.h>

#include "icmp.h"
#include "udp.h"
#include "util.h"
#include "warpcore_internal.h"


// Log a UDP segment
#ifndef NDEBUG
/// Print a summary of the udp_hdr @p udp.
///
/// @param      udp   The udp_hdr to print.
///
#define udp_log(udp)                                                           \
    do {                                                                       \
        warn(info, "UDP :%d -> :%d, cksum 0x%04x, len %u", ntohs(udp->sport),  \
             ntohs(udp->dport), ntohs(udp->cksum), ntohs(udp->len));           \
    } while (0)
#else
#define udp_log(udp)                                                           \
    do {                                                                       \
    } while (0)
#endif


// Receive a UDP packet.

/// Receive a UDP packet. Validates the UDP checksum and appends the payload
/// data to the corresponding w_sock. Also makes the receive timestamp and IPv4
/// flags available, via w_iov::ts and w_iov::flags, respectively.
///
/// @param      w     Warpcore engine.
/// @param      buf   Buffer containing the Ethernet frame to receive from.
/// @param[in]  src   The IPv4 source address of the sender of this packet.
///
void __attribute__((nonnull))
udp_rx(struct warpcore * const w, void * const buf, const uint32_t src)
{
    const struct ip_hdr * const ip = eth_data(buf);
    struct udp_hdr * const udp = ip_data(buf);
    const uint16_t len = ntohs(udp->len);

    udp_log(udp);

    // validate the checksum
    const uint16_t orig = udp->cksum;
    udp->cksum = in_pseudo(ip->src, ip->dst, htons(len + ip->p));
    const uint16_t cksum = in_cksum(udp, len);
    udp->cksum = orig;
    if (unlikely(orig != cksum)) {
        warn(warn, "invalid UDP checksum, received 0x%04x != 0x%04x",
             ntohs(orig), ntohs(cksum));
        return;
    }

    struct w_sock ** const s = get_sock(w, IP_P_UDP, udp->dport);
    if (unlikely(*s == 0)) {
        // nobody bound to this port locally
        // send an ICMP unreachable
        icmp_tx_unreach(w, ICMP_UNREACH_PORT, buf);
        return;
    }

    // grab an unused iov for the data in this packet
    struct w_iov * const i = STAILQ_FIRST(&w->iov);
    assert(i != 0, "out of spare bufs");
    struct netmap_ring * const rxr = NETMAP_RXRING(w->nif, w->cur_rxr);
    struct netmap_slot * const rxs = &rxr->slot[rxr->cur];
    STAILQ_REMOVE_HEAD(&w->iov, next);

    warn(debug, "swapping rx ring %u slot %d (buf %d) and spare buf %u",
         w->cur_rxr, rxr->cur, rxs->buf_idx, i->idx);

    // remember index of this buffer
    const uint32_t tmp_idx = i->idx;

    // adjust the buffer offset to the received data into the iov
    i->buf = (char *)ip_data(buf) + sizeof(struct udp_hdr);
    i->len = len - sizeof(struct udp_hdr);
    i->idx = rxs->buf_idx;

    // tag the iov with sender information and metadata
    i->src = src;
    i->sport = udp->sport;
    i->flags = ip->tos;
    memcpy(&i->ts, &rxr->ts, sizeof(i->ts));

    // append the iov to the socket
    STAILQ_INSERT_TAIL(&(*s)->iv, i, next);

    // put the original buffer of the iov into the receive ring
    rxs->buf_idx = tmp_idx;
    rxs->flags = NS_BUF_CHANGED;
}


// Put the socket template header in front of the data in the iov and send.

/// Sends w_iov payloads contained in a w_sock::ov via UDP. Prepends the
/// template header from w_sock::hdr, computes the UDP length and checksum, and
/// hands the packet off to ip_tx(). Stops processing packets from w_sock::ov if
/// ip_tx() indicates that the TX rings are full.
///
/// @param      s     The w_sock whose packets are to be transmitted.
///
void __attribute__((nonnull)) udp_tx(struct w_sock * const s)
{
#ifndef NDEBUG
    uint32_t n = 0, l = 0;
#endif
    // packetize bufs and place in tx ring
    while (likely(!STAILQ_EMPTY(&s->ov))) {
        struct w_iov * const v = STAILQ_FIRST(&s->ov);

        // copy template header into buffer and fill in remaining fields
        void * const buf = IDX2BUF(s->w, v->idx);
        memcpy(buf, &s->hdr, sizeof(s->hdr));

        struct udp_hdr * const udp = ip_data(buf);
        const uint16_t len = v->len + sizeof(struct udp_hdr);
        udp->len = htons(len);

        // compute the checksum
        udp->cksum = in_pseudo(s->w->ip, s->hdr.ip.dst, htons(len + IP_P_UDP));
        udp->cksum = in_cksum(udp, len);

        udp_log(udp);

        // do IP transmit preparation
        if (ip_tx(s->w, v, len)) {
#ifndef NDEBUG
            n++;
            l += v->len;
#endif
            STAILQ_REMOVE_HEAD(&s->ov, next);
            STAILQ_INSERT_HEAD(&s->w->iov, v, next);
        } else
            // no space left in rings
            goto done;
    }
done:
    warn(info, "proto %d tx iov (len %d in %d bufs) done", s->hdr.ip.p, l, n);

    // kick tx ring
    w_kick_tx(s->w);
}
