#include "rtl8139.h"
#include "ports.h"
#include "vga.h"

#define RX_BUF_SIZE 32768
#define TX_BUF_SIZE 4096

static uint16_t io_base;
static uint8_t pci_bus, pci_dev;
uint8_t rtl_mac[6];
static uint8_t rx_ring[RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buf[TX_BUF_SIZE] __attribute__((aligned(16)));
static int initted;
static int next_tx_desc;

static uint32_t pci_read_cfg(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = 0x80000000 | (bus << 16) | ((dev & 31) << 11) | ((func & 7) << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static void pci_write_cfg(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val) {
    uint32_t addr = 0x80000000 | (bus << 16) | ((dev & 31) << 11) | ((func & 7) << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

int rtl8139_init(void) {
    io_base = 0;
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            uint32_t vid = pci_read_cfg(bus, dev, 0, 0);
            if ((vid & 0xFFFF) != 0x10EC) continue;
            if ((vid >> 16) != 0x8139) continue;
            uint32_t bar = pci_read_cfg(bus, dev, 0, 0x10);
            if (bar & 1) { io_base = bar & 0xFFFC; pci_bus = bus; pci_dev = dev; goto found; }
        }
    }
    vga_writestring("NIC: RTL8139 not found\n");
    return 0;

found:
    // Enable bus mastering + I/O space in PCI command register
    uint32_t cmd = pci_read_cfg(pci_bus, pci_dev, 0, 0x04);
    cmd |= 0x07;
    pci_write_cfg(pci_bus, pci_dev, 0, 0x04, cmd);

    vga_writestring("NIC: RTL8139 I/O 0x");
    vga_putchar('0' + (char)((io_base >> 12) & 0xF));
    vga_putchar('0' + (char)((io_base >> 8) & 0xF));
    vga_putchar('0' + (char)((io_base >> 4) & 0xF));
    vga_putchar('0' + (char)(io_base & 0xF));
    vga_putchar('\n');

    // QEMU RTL8139C+ register layout:
    //   ChipCmd at 0x37, Cfg9346 at 0x50, Config1 at 0x52
    //   TxConfig at 0x40, RxConfig at 0x44
    //   TxPoll at 0xD9, RxRingAddrLO at 0xE4
    //   TxThresh at 0xEC

    // Soft reset via ChipCmd (0x37)
    outb(io_base + 0x37, 0x10);
    int t = 100000;
    while ((inb(io_base + 0x37) & 0x10) && --t);
    if (!t) { vga_writestring("NIC: reset timeout\n"); return 0; }

    // Unlock config registers via Cfg9346 (0x50)
    outb(io_base + 0x50, 0xC0);

    // Read MAC (always at 0x00-0x05)
    for (int i = 0; i < 6; i++) rtl_mac[i] = inb(io_base + i);
    vga_writestring("NIC: MAC ");
    for (int i = 0; i < 6; i++) {
        uint8_t n = rtl_mac[i];
        vga_putchar("0123456789ABCDEF"[(n >> 4) & 0xF]);
        vga_putchar("0123456789ABCDEF"[n & 0xF]);
        if (i < 5) vga_putchar(':');
    }
    vga_putchar('\n');

    // Set Rx ring buffer address via RxBuf (0x30) - traditional mode
    outl(io_base + 0x30, (uint32_t)(uintptr_t)rx_ring);

    // Reset RxBufPtr (CAPR) at 0x38
    outw(io_base + 0x38, 0);

    // RxConfig at 0x44: accept broadcast+multicast+physical, set buffer size to 32K
    outl(io_base + 0x44, 0x0000100F); // bit 12 = 32K buffer, bits 0-3 = accept all

    // Disable interrupts
    outw(io_base + 0x3C, 0x0000);
    outw(io_base + 0x3E, 0xFFFF); // clear ISR

    // TxConfig at 0x40
    outl(io_base + 0x40, 0x00000600);

    // Early Tx threshold at 0xEC
    outb(io_base + 0xEC, 0x3C);

    // Enable receiver and transmitter via ChipCmd (0x37)
    outb(io_base + 0x37, 0x0C);

    vga_writestring("NIC: ready\n");
    initted = 1;
    return 1;
}

int rtl8139_send(const uint8_t *data, int len) {
    if (!initted || len < 14 || len > TX_BUF_SIZE) return 0;

    for (int i = 0; i < len; i++) tx_buf[i] = data[i];

    int desc = next_tx_desc;
    uint16_t tsad = 0x20 + desc * 4;  // TxAddr0-3
    uint16_t tsd  = 0x10 + desc * 4;  // TxStatus0-3

    // Set buffer address for this descriptor
    outl(io_base + tsad, (uint32_t)(uintptr_t)tx_buf);

    // Write length to TxStatus (triggers transmit, clears TOK/TER)
    outl(io_base + tsd, len & 0x1FFF);

    // Wait for TOK (bit 15)
    int status;
    int t2 = 500000;
    while (t2--) {
        status = inl(io_base + tsd);
        if (status & 0x8000) break;
    }
    if (t2 <= 0) {
        vga_writestring("NIC: Tx timeout\n");
        return 0;
    }

    // Advance to next descriptor
    next_tx_desc = (desc + 1) & 3;

    return 1;
}

int rtl8139_recv(uint8_t *buf, int max) {
    if (!initted) return 0;

    // Hardware returns internal_ptr - 0x10; add 0x10 to get actual position.
    // Writing to RxBufPtr adds 0x10 internally, so the write cancels correctly.
    uint16_t raw = inw(io_base + 0x38);
    uint16_t ptr = raw + 0x10;

    volatile uint16_t *header = (volatile uint16_t *)(rx_ring + ptr);
    uint16_t rx_status = header[0];
    if (!(rx_status & 0x0001)) return 0;

    uint16_t rx_len = header[1];
    if (rx_len < 60 || rx_len > 0x3FFF) return 0;

    int pkt_len = rx_len - 4;
    if (pkt_len > max) pkt_len = max;

    int off = (ptr + 4) % RX_BUF_SIZE;
    for (int i = 0; i < pkt_len; i++) {
        buf[i] = rx_ring[off];
        off = (off + 1) % RX_BUF_SIZE;
    }

    int aligned = (rx_len + 4 + 3) & ~3;
    // Write raw + aligned; the +0x10 in the write handler cancels the -0x10
    // from reading, advancing internal pointer correctly with MOD2 wrap.
    outw(io_base + 0x38, raw + aligned);

    return pkt_len;
}

void rtl8139_dump_rx(void) {
    if (!initted) return;
    uint16_t rba = inw(io_base + 0x3A);
    uint16_t capr = inw(io_base + 0x38);
    uint16_t isr = inw(io_base + 0x3E);
    vga_writestring("NIC: RxBufAddr=");
    vga_putchar("0123456789ABCDEF"[(rba>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(rba>>8)&0xF]);
    vga_putchar("0123456789ABCDEF"[(rba>>4)&0xF]); vga_putchar("0123456789ABCDEF"[rba&0xF]);
    vga_writestring(" CAPR=");
    vga_putchar("0123456789ABCDEF"[(capr>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(capr>>8)&0xF]);
    vga_putchar("0123456789ABCDEF"[(capr>>4)&0xF]); vga_putchar("0123456789ABCDEF"[capr&0xF]);
    vga_writestring(" ISR=");
    vga_putchar("0123456789ABCDEF"[(isr>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(isr>>8)&0xF]);
    vga_putchar("0123456789ABCDEF"[(isr>>4)&0xF]); vga_putchar("0123456789ABCDEF"[isr&0xF]);
    vga_putchar('\n');
    // Dump ring buffer headers from position 0 to rba in steps of 4
    uint16_t pos = 0;
    while (pos < rba && pos < RX_BUF_SIZE) {
        volatile uint16_t *h = (volatile uint16_t *)(rx_ring + pos);
        if (h[0] || h[1]) {
            vga_putchar('[');
            vga_putchar("0123456789ABCDEF"[(pos>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(pos>>8)&0xF]);
            vga_putchar("0123456789ABCDEF"[(pos>>4)&0xF]); vga_putchar("0123456789ABCDEF"[pos&0xF]);
            vga_writestring("] ");
            vga_putchar("0123456789ABCDEF"[(h[0]>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(h[0]>>8)&0xF]);
            vga_putchar("0123456789ABCDEF"[(h[0]>>4)&0xF]); vga_putchar("0123456789ABCDEF"[h[0]&0xF]);
            vga_putchar(' ');
            vga_putchar("0123456789ABCDEF"[(h[1]>>12)&0xF]); vga_putchar("0123456789ABCDEF"[(h[1]>>8)&0xF]);
            vga_putchar("0123456789ABCDEF"[(h[1]>>4)&0xF]); vga_putchar("0123456789ABCDEF"[h[1]&0xF]);
            vga_putchar('\n');
            // Calculate next header position
            if (h[0] & 0x0001) {
                uint16_t rx_len = h[1];
                if (rx_len >= 60 && rx_len <= 0x3FFF) {
                    pos += (rx_len + 4 + 3) & ~3;
                    continue;
                }
            }
            pos += 4;
        } else {
            pos += 4;
        }
    }
}

void rtl8139_get_mac(uint8_t *mac) {
    for (int i = 0; i < 6; i++) mac[i] = rtl_mac[i];
}

int rtl8139_ok(void) {
    return initted;
}
