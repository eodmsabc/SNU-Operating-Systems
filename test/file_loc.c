#include <stdio.h>
#include <unistd.h>
#include <linux/gps.h>

int main(int argc, char* argv[])
{
    struct gps_location gps;
    if (syscall(399, argv[1], &gps) != 0) {
        printf("get gps failed?\n");
    }
    else {
        printf("gps:  (%d.%d , %d.%d)\n",
                gps.lat_integer,
                gps.lat_fractional,
                gps.lng_integer,
                gps.lng_fractional);
    }
    return 0;
}
