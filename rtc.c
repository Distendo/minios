#include "rtc.h"
#include "ports.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static int bcd_mode;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    io_wait();
    return inb(CMOS_DATA);
}

static int from_bcd(uint8_t val) {
    if (bcd_mode)
        return (val & 0x0F) + ((val >> 4) * 10);
    return val;
}

void rtc_init(void) {
    uint8_t reg_b = cmos_read(0x0B);
    bcd_mode = !(reg_b & 0x04);
}

void rtc_read_time(rtc_time_t *t) {
    uint8_t sec, min, hour;
    do {
        while (cmos_read(0x0A) & 0x80);
        sec = cmos_read(0x00);
        min = cmos_read(0x02);
        hour = cmos_read(0x04);
    } while (cmos_read(0x0A) & 0x80);

    t->seconds = from_bcd(sec);
    t->minutes = from_bcd(min);
    t->hours = from_bcd(hour);
}