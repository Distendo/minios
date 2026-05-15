#include "net.h"
#include "rtl8139.h"
#include "ports.h"
#include "vga.h"

#define MY_IP       ((10UL << 24) | (0UL << 16) | (2UL << 8) | 15UL)
#define GATEWAY_IP  ((10UL << 24) | (0UL << 16) | (2UL << 8) | 2UL)
#define DNS_IP      ((10UL << 24) | (0UL << 16) | (2UL << 8) | 3UL)

static uint8_t my_mac[6];
static uint8_t gw_mac[6];
static int net_ok;
static uint32_t tcp_seq = 0x12345678;
static uint16_t eph_port = 40000;

static uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }

static uint16_t chksum(const uint8_t *d, int len) {
    uint32_t s = 0;
    for (int i = 0; i < len; i += 2)
        s += (d[i] << 8) | ((i + 1 < len) ? d[i + 1] : 0);
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return ~s & 0xFFFF;
}

static void mcpy(uint8_t *d, const uint8_t *s, int n) {
    for (int i = 0; i < n; i++) d[i] = s[i];
}

// Send an ARP request
static int arp_request(uint32_t tip) {
    uint8_t pkt[42];
    // Ethernet header
    for (int i = 0; i < 6; i++) pkt[i] = 0xFF; // broadcast dst
    mcpy(pkt + 6, my_mac, 6);                   // src
    pkt[12] = 0x08; pkt[13] = 0x06;              // ARP type

    // ARP header
    pkt[14] = 0x00; pkt[15] = 0x01;              // hardware: Ethernet
    pkt[16] = 0x08; pkt[17] = 0x00;              // protocol: IPv4
    pkt[18] = 6;                                  // MAC addr length
    pkt[19] = 4;                                  // IP addr length
    pkt[20] = 0x00; pkt[21] = 0x01;              // operation: request
    mcpy(pkt + 22, my_mac, 6);                   // sender MAC
    pkt[28] = (MY_IP >> 24) & 0xFF;
    pkt[29] = (MY_IP >> 16) & 0xFF;
    pkt[30] = (MY_IP >> 8) & 0xFF;
    pkt[31] = MY_IP & 0xFF;                       // sender IP
    for (int i = 0; i < 6; i++) pkt[32 + i] = 0; // target MAC (zero)
    pkt[38] = (tip >> 24) & 0xFF;
    pkt[39] = (tip >> 16) & 0xFF;
    pkt[40] = (tip >> 8) & 0xFF;
    pkt[41] = tip & 0xFF;                         // target IP

    return rtl8139_send(pkt, 42);
}

static int eth_send(const uint8_t *dst, uint16_t type, const uint8_t *pay, int plen) {
    uint8_t pkt[2048];
    int o = 0;
    mcpy(pkt + o, dst, 6); o += 6;
    mcpy(pkt + o, my_mac, 6); o += 6;
    pkt[o++] = type >> 8; pkt[o++] = type & 0xFF;
    for (int i = 0; i < plen; i++) pkt[o++] = pay[i];
    int r = rtl8139_send(pkt, o);
    return r;
}

static int ip_send(uint32_t dip, uint8_t proto, const uint8_t *pay, int plen) {
    uint8_t hdr[20];
    hdr[0] = 0x45; hdr[1] = 0;
    int tot = 20 + plen;
    hdr[2] = tot >> 8; hdr[3] = tot & 0xFF;
    hdr[4] = 0; hdr[5] = 0;
    hdr[6] = 0; hdr[7] = 0;
    hdr[8] = 64; hdr[9] = proto;
    hdr[10] = 0; hdr[11] = 0;
    hdr[12] = (MY_IP >> 24) & 0xFF;
    hdr[13] = (MY_IP >> 16) & 0xFF;
    hdr[14] = (MY_IP >> 8) & 0xFF;
    hdr[15] = MY_IP & 0xFF;
    hdr[16] = (dip >> 24) & 0xFF;
    hdr[17] = (dip >> 16) & 0xFF;
    hdr[18] = (dip >> 8) & 0xFF;
    hdr[19] = dip & 0xFF;
    uint16_t ck = chksum(hdr, 20);
    hdr[10] = ck >> 8; hdr[11] = ck & 0xFF;

    uint8_t ipp[1500];
    for (int i = 0; i < 20; i++) ipp[i] = hdr[i];
    for (int i = 0; i < plen; i++) ipp[20 + i] = pay[i];
    return eth_send(gw_mac, 0x0800, ipp, tot);
}

static int tcp_send(uint32_t dip, uint16_t sp, uint16_t dp,
                     uint32_t seq, uint32_t ack, uint8_t flags,
                     const uint8_t *data, int dlen) {
    uint8_t seg[1500];
    int o = 0;
    seg[o++] = sp >> 8; seg[o++] = sp & 0xFF;
    seg[o++] = dp >> 8; seg[o++] = dp & 0xFF;
    seg[o++] = seq >> 24; seg[o++] = seq >> 16;
    seg[o++] = seq >> 8; seg[o++] = seq & 0xFF;
    seg[o++] = ack >> 24; seg[o++] = ack >> 16;
    seg[o++] = ack >> 8; seg[o++] = ack & 0xFF;
    seg[o++] = (20 / 4) << 4;
    seg[o++] = flags;
    seg[o++] = 0xFF; seg[o++] = 0xFF; // window
    seg[o++] = 0; seg[o++] = 0;       // checksum (placeholder)
    seg[o++] = 0; seg[o++] = 0;       // urgent ptr
    for (int i = 0; i < dlen; i++) seg[o++] = data[i];

    uint16_t tcplen = 20 + dlen;
    uint16_t pseudo[6];
    pseudo[0] = (MY_IP >> 16) & 0xFFFF;
    pseudo[1] = MY_IP & 0xFFFF;
    pseudo[2] = (dip >> 16) & 0xFFFF;
    pseudo[3] = dip & 0xFFFF;
    pseudo[4] = 6;       // zero + protocol (TCP) in network byte order
    pseudo[5] = tcplen;  // TCP segment length in network byte order

    uint32_t s = 0;
    for (int i = 0; i < 6; i++) s += pseudo[i];
    for (int i = 0; i < tcplen; i += 2)
        s += (seg[i] << 8) | ((i + 1 < tcplen) ? seg[i + 1] : 0);
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    uint16_t cs = ~s & 0xFFFF;
    seg[16] = cs >> 8; seg[17] = cs & 0xFF;

    return ip_send(dip, 6, seg, 20 + dlen);
}

static int poll_pkt(uint8_t proto, uint16_t sport, uint16_t dport,
                     uint8_t want_flags, uint8_t *got_flags,
                     uint8_t *payload, int *plen, int max_plen,
                     uint32_t *ret_seq, uint32_t *ret_ack,
                     int max_tries) {
    for (int t = 0; t < max_tries; t++) {
        uint8_t buf[2048];
        int len = rtl8139_recv(buf, 2048);
        if (len <= 0) continue;
        if (len < 42) continue;

        uint16_t tpe = (buf[12] << 8) | buf[13];

        // ARP reply
        if (tpe == 0x0806) {
            if (len < 42) continue;
            uint16_t op = (buf[20] << 8) | buf[21];
            if (op == 2) {
                uint32_t spa = (buf[28] << 24) | (buf[29] << 16) |
                               (buf[30] << 8) | buf[31];
                if (spa == GATEWAY_IP) {
                    mcpy(gw_mac, buf + 22, 6);
                    return 1;
                }
            }
            continue;
        }

        // IPv4
        if (tpe != 0x0800) continue;
        if ((buf[14] & 0xF0) != 0x40) continue;
        if (buf[23] != proto) continue;

        int ip_hl = (buf[14] & 0x0F) * 4;
        int tcp_off = 14 + ip_hl;

        if (proto == 6) {
            if (len < tcp_off + 20) continue;
            uint16_t sp = (buf[tcp_off] << 8) | buf[tcp_off + 1];
            uint16_t dp = (buf[tcp_off + 2] << 8) | buf[tcp_off + 3];
            if (sp != dport || dp != sport) continue;
            uint8_t f = buf[tcp_off + 13];
            uint32_t seq = (buf[tcp_off + 4] << 24) |
                           (buf[tcp_off + 5] << 16) |
                           (buf[tcp_off + 6] << 8) | buf[tcp_off + 7];
            uint32_t ack = (buf[tcp_off + 8] << 24) |
                           (buf[tcp_off + 9] << 16) |
                           (buf[tcp_off + 10] << 8) | buf[tcp_off + 11];
            if (ret_seq) *ret_seq = seq;
            if (ret_ack) *ret_ack = ack;
            if (got_flags) *got_flags = f;

            // Check wanted flags
            if (want_flags) {
                if ((f & want_flags) != want_flags) continue;
            }

            int tcp_hlen = ((buf[tcp_off + 12] >> 4) & 0x0F) * 4;
            int ip_total = (buf[16] << 8) | buf[17];
            int pay_len = ip_total - ip_hl - tcp_hlen;
            if (pay_len > 0 && payload && plen && max_plen > 0) {
                int cp = pay_len < max_plen ? pay_len : max_plen;
                for (int i = 0; i < cp; i++)
                    payload[i] = buf[tcp_off + tcp_hlen + i];
                *plen = cp;
            }
            return 1;
        } else if (proto == 17) {
            if (len < tcp_off + 8) continue;
            uint16_t sp = (buf[tcp_off] << 8) | buf[tcp_off + 1];
            uint16_t dp = (buf[tcp_off + 2] << 8) | buf[tcp_off + 3];
            if (sp != dport || dp != sport) continue;
            int udp_len = (buf[tcp_off + 4] << 8) | buf[tcp_off + 5];
            int pay_len = udp_len - 8;
            if (pay_len > 0 && payload && plen && max_plen > 0) {
                int cp = pay_len < max_plen ? pay_len : max_plen;
                for (int i = 0; i < cp; i++)
                    payload[i] = buf[tcp_off + 8 + i];
                *plen = cp;
            }
            return 1;
        }
    }
    return 0;
}

static int ip_to_int(const char *s, uint32_t *ip) {
    uint32_t val = 0;
    int octet = 0;
    *ip = 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') val = val * 10 + (*s - '0');
        else if (*s == '.') {
            if (octet > 3) return 0;
            *ip |= val << ((3 - octet) * 8);
            val = 0; octet++;
        } else return 0;
        s++;
    }
    if (octet != 3) return 0;
    *ip |= val;
    return 1;
}

static int dns_lookup(const char *host, uint32_t *ip) {
    uint32_t ipval = 0;
    if (ip_to_int(host, &ipval)) { *ip = ipval; return 1; }

    uint16_t sport = eph_port++;
    uint8_t dnsq[512];
    int o = 0;
    dnsq[o++] = 0x12; dnsq[o++] = 0x34;
    dnsq[o++] = 0x01; dnsq[o++] = 0x00;
    dnsq[o++] = 0x00; dnsq[o++] = 0x01;
    dnsq[o++] = 0x00; dnsq[o++] = 0x00;
    dnsq[o++] = 0x00; dnsq[o++] = 0x00;
    dnsq[o++] = 0x00; dnsq[o++] = 0x00;

    const char *h = host;
    while (*h) {
        const char *d = h;
        while (*d && *d != '.') d++;
        int len = d - h;
        dnsq[o++] = len;
        for (int i = 0; i < len; i++) dnsq[o++] = *h++;
        if (*h == '.') h++;
    }
    dnsq[o++] = 0;
    dnsq[o++] = 0; dnsq[o++] = 1;
    dnsq[o++] = 0; dnsq[o++] = 1;

    uint8_t udp[512];
    int uo = 0;
    udp[uo++] = sport >> 8; udp[uo++] = sport & 0xFF;
    udp[uo++] = 0; udp[uo++] = 53;
    int udplen = 8 + o;
    udp[uo++] = udplen >> 8; udp[uo++] = udplen & 0xFF;
    udp[uo++] = 0; udp[uo++] = 0;
    for (int i = 0; i < o; i++) udp[uo++] = dnsq[i];

    if (!ip_send(DNS_IP, 17, udp, uo)) return 0;

    for (int tries = 0; tries < 20000; tries++) {
        uint8_t flags;
        uint8_t dns_r[512];
        int dlen = 0;
        int r = poll_pkt(17, sport, 53, 0, &flags, dns_r, &dlen, 512, 0, 0, 1);
        if (!r) continue;
        if (dlen < 12) continue;
        if (dns_r[0] != 0x12 || dns_r[1] != 0x34) continue;
        if (dns_r[3] & 0x0F) return 0;

        int ap = 12;
        while (ap < dlen && dns_r[ap] != 0) {
            if (dns_r[ap] >= 192) { ap += 2; break; }
            ap += dns_r[ap] + 1;
        }
        ap += 5;
        if (ap + 10 > dlen) continue;
        if (dns_r[ap] >= 192) ap += 2;
        else { while (dns_r[ap]) ap++; ap++; }
        int atype = (dns_r[ap] << 8) | dns_r[ap + 1]; ap += 2;
        ap += 2; ap += 4;
        int rdlen = (dns_r[ap] << 8) | dns_r[ap + 1]; ap += 2;
        if (atype == 1 && rdlen == 4 && ap + 4 <= dlen) {
            *ip = (dns_r[ap] << 24) | (dns_r[ap + 1] << 16) |
                  (dns_r[ap + 2] << 8) | dns_r[ap + 3];
            return 1;
        }
        return 0;
    }
    return 0;
}

int net_init(void) {
    if (!rtl8139_init()) { vga_writestring("NET: NIC init failed\n"); return 0; }
    rtl8139_get_mac(my_mac);

    // Hardcode default gateway MAC (QEMU user-mode)
    uint8_t defgw[] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    mcpy(gw_mac, defgw, 6);

    // Send ARP request for gateway to ensure the stack knows our MAC
    if (!arp_request(GATEWAY_IP)) {
        vga_writestring("NET: ARP send failed\n");
        // Continue anyway; the hardcoded MAC may still work
    }

    // Wait a bit for ARP reply and process it
    // Poll_pkt will update gw_mac if we get a reply
    for (int i = 0; i < 5000; i++) {
        uint8_t flags;
        int r = poll_pkt(0, 0, 0, 0, &flags, 0, 0, 0, 0, 0, 1);
        if (r) { vga_writestring("NET: gateway MAC resolved via ARP\n"); break; }
    }

    vga_writestring("NET: initialized (IP 10.0.2.15, GW 10.0.2.2)\n");
    net_ok = 1;
    return 1;
}

int net_http_get(const char *host, int port, const char *path,
                  char *resp, int max_resp) {
    if (!net_ok) { vga_writestring("HTTP: network not initialized\n"); return 0; }

    uint32_t ip;
    if (!dns_lookup(host, &ip)) {
        vga_writestring("HTTP: DNS lookup failed for ");
        vga_writestring(host);
        vga_putchar('\n');
        return 0;
    }

    vga_writestring("HTTP: connecting to ");
    vga_writestring(host);
    vga_putchar('\n');

    uint16_t sport = eph_port++;
    uint32_t seq = tcp_seq;
    uint32_t ack = 0;
    uint8_t flags;
    uint32_t rseq, rack;
    int r;

    // Send SYN
    if (!tcp_send(ip, sport, port, seq, 0, 0x02, 0, 0)) {
        vga_writestring("HTTP: SYN send failed\n");
        return 0;
    }
    seq++;

    // Wait for SYN+ACK
    r = 0;
    for (int w = 0; w < 500000; w++) {
        if (poll_pkt(6, sport, port, 0x12, &flags, 0, 0, 0, &rseq, &rack, 1)) { r = 1; break; }
        io_wait();
    }
    if (!r) { vga_writestring("HTTP: no SYN+ACK\n");
        rtl8139_dump_rx();
        return 0; }
    ack = rseq + 1;

    // Build HTTP GET request
    char req[1024];
    int reqlen = 0;
    const char *g = "GET "; while (*g) req[reqlen++] = *g++;
    const char *p = path; while (*p) req[reqlen++] = *p++;
    const char *v = " HTTP/1.0\r\nHost: "; while (*v) req[reqlen++] = *v++;
    const char *h = host; while (*h) req[reqlen++] = *h++;
    const char *c = "\r\nConnection: close\r\n\r\n"; while (*c) req[reqlen++] = *c++;

    // Send HTTP request (PSH+ACK)
    if (!tcp_send(ip, sport, port, seq, ack, 0x18, (uint8_t *)req, reqlen)) {
        vga_writestring("HTTP: request send failed\n");
        return 0;
    }
    seq += reqlen;

    // Receive response
    int total = 0;
    int got_data = 0;
    for (int tries = 0; tries < 500000; tries++) {
        uint8_t data[1500];
        int dlen = 0;
        r = poll_pkt(6, sport, port, 0, &flags, data, &dlen, 1500, &rseq, &rack, 1);
        if (!r) { io_wait(); continue; }

        if (flags & 0x01) { // FIN
            if (dlen > 0) {
                for (int i = 0; i < dlen && total < max_resp - 1; i++)
                    resp[total++] = data[i];
            }
            break;
        }

        if (dlen > 0) {
            got_data = 1;
            for (int i = 0; i < dlen && total < max_resp - 1; i++)
                resp[total++] = data[i];
        }

        // ACK the received data
        seq = rack;
        ack = rseq + dlen;
        tcp_send(ip, sport, port, seq, ack, 0x10, 0, 0);
    }

    resp[total] = 0;

    if (!got_data && total == 0) {
        vga_writestring("HTTP: no response data\n");
    }

    // Send FIN+ACK
    tcp_send(ip, sport, port, seq, ack, 0x11, 0, 0);

    vga_writestring("HTTP: received ");
    // Use simple digit printing
    if (total >= 10000) vga_putchar('0' + (char)(total / 10000 % 10));
    if (total >= 1000) vga_putchar('0' + (char)(total / 1000 % 10));
    vga_putchar('0' + (char)(total / 100 % 10));
    vga_putchar('0' + (char)(total / 10 % 10));
    vga_putchar('0' + (char)(total % 10));
    vga_writestring(" bytes\n");

    return total;
}
