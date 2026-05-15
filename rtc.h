#ifndef RTC_H
#define RTC_H

typedef struct {
    int hours;
    int minutes;
    int seconds;
} rtc_time_t;

void rtc_init(void);
void rtc_read_time(rtc_time_t *t);

#endif