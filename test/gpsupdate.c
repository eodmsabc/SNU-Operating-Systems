#include <unistd.h>
#include <linux/gps.h>

#define SYSCALL_SET_GPS_LOCATION 398

int main(void) {
    struct gps_location gps;
    syscall(SYSCALL_SET_GPS_LOCATION, &gps);
    return 0;
}
