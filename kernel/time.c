#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>

#include <asm/segment.h>
#include <asm/io.h>

#include <linux/mc146818rtc.h>
#define RTC_ALWAYS_BCD 1

#include <linux/timex.h>
extern volatile struct timeval xtime;

#include <linux/mktime.h>
extern long kernel_mktime(struct mktime * time);

void time_init(void) {
    struct mktime time;
    int i;

    for (i = 0; i < 1000000; i++) /* may take up to 1 second... */
        if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
            break;
    for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms*/
		if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
			break;
    do {
        time.sec = CMOS_READ(RTC_SECONDS);
		time.min = CMOS_READ(RTC_MINUTES);
		time.hour = CMOS_READ(RTC_HOURS);
		time.day = CMOS_READ(RTC_DAY_OF_MONTH);
		time.mon = CMOS_READ(RTC_MONTH);
		time.year = CMOS_READ(RTC_YEAR);
    } while (time.sec != CMOS_READ(RTC_SECONDS));

    if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
        BCD_TO_BIN(time.sec);
	    BCD_TO_BIN(time.min);
	    BCD_TO_BIN(time.hour);
	    BCD_TO_BIN(time.day);
	    BCD_TO_BIN(time.mon);
	    BCD_TO_BIN(time.year);
    }
    time.mon--;
    xtime.tv_sec = kernel_mktime(&time);
}
int set_rtc_mmss(unsigned long nowtime) {
    int retval = 0;
    short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;
    unsigned char save_control, save_freq_select, cmos_minutes;

    save_control = CMOS_READ(RTC_CONTROL); /* tell the clock it's being set */
    CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

    save_freq_select = CMOS_READ(RTC_FREQ_SELECT); /* stop and reset prescaler */
    CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

    cmos_minutes = CMOS_READ(RTC_MINUTES);
    if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
        BCD_TO_BIN(cmos_minutes);
    
    if (((cmos_minutes < real_minutes) ?
       (real_minutes - cmos_minutes) :
       (cmos_minutes - real_minutes)) < 30) {
        if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
            BIN_TO_BCD(real_seconds);
            BIN_TO_BCD(real_minutes);
        }
        CMOS_WRITE(real_seconds,RTC_SECONDS);
        CMOS_WRITE(real_minutes,RTC_MINUTES);
    } else
        retval = -1;
    CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
    CMOS_WRITE(save_control, RTC_CONTROL);
    return retval;
}