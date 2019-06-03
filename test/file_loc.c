#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gps.h>
#include <errno.h>
#include <string.h>


int main(int argc, char* argv[])
{
    struct gps_location gps;

    if (argc != 2) {
        printf("this utility takes exactly one argument: [filepath]\n");
        return -1;
    }
    if (syscall(399, argv[1], &gps) != 0) {
        printf("GPS GET ERROR!: %s\n", strerror(errno));
        return -1;
    }

    printf("%s location info\n"
            "latitude\t\tlongitude\t\taccuracy(m)\n"
            "%d.%d\t\t%d.%d\t\t%d\n",
            argv[1],
            gps.lat_integer, gps.lat_fractional,
            gps.lng_integer, gps.lng_fractional,
            gps.accuracy);

    printf("google maps link: https://www.google.com/maps/place/%d.%d,%d.%d",
            gps.lat_integer, gps.lat_fractional,
            gps.lng_integer, gps.lng_fractional);

    return 0;
}
