#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gps.h>

#define SYSCALL_SET_GPS_LOCATION 398

int main(int argc, char* argv[])
{
    struct gps_location gps;
    gps.lat_integer = atoi(argv[1]);
    gps.lng_integer = atoi(argv[2]);
    gps.lat_fractional = 0;
    gps.lng_fractional = 0;
    gps.accuracy = 1000;
    if (syscall(SYSCALL_SET_GPS_LOCATION, &gps) != 0) {
        printf("gps not set?\n");
    }
    else {
        printf("gps set?\n");
    }
    return 0;
}
