#include <stdio.h>
#include <unistd.h>
#include <linux/gps.h>

int main(int argc, char* argv[])
{
    struct gps_location gps;

    if (argc != 2) {
        printf("this utility takes exactly one argument: [filepath]\n");
        return -1;
    }
    if (syscall(399, argv[1], &gps) != 0) {
        printf("get gps failed for %s - check if file exists\n", argv[1]);
        return -1;
    }

    printf("%s location info\n"
            "latitude\t\tlongitude\t\taccuracy(m)\n"
            "%d.%d\t\t%d.%d\t\t%d\n",
            argv[1],
            gps.lat_integer, gps.lat_fractional,
            gps.lng_integer, gps.lng_fractional,
            gps.accuracy);

    printf("google maps link: http://www.google.com/maps/place/%d.%d, %d.%d",
            gps.lat_integer, gps.lat_fractional,
            gps.lng_integer, gps.lng_fractional);

    return 0;
}