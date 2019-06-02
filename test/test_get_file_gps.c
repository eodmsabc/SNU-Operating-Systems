#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gps.h>
#include <errno.h>
#include <string.h>

void print_gps(struct gps_location loc)
{
    printf("latitude\t\tlongitude\t\taccuracy(m)\n"
            "%d.%d\t\t%d.%d\t\t%d\n",
            loc.lat_integer, loc.lat_fractional,
            loc.lng_integer, loc.lng_fractional,
            loc.accuracy);
}

int main(int argc, char* argv[])
{
    struct gps_location gps;
    char path[2048];
    int input;

    while(1)
    {
        printf("if you want to exit, insert 0, else insert 1. :");
        scanf("%d", &input);
        if(input == 0) break;
        else
        {
            printf("insert path :");
            fgets(path, sizeof(path), stdin);

            if (syscall(399, path, &gps) != 0) {
                printf("GPS GET ERROR!: %s\n", strerror(errno));
            }
            else {
                printf("path %s file's gps is\n", path);
                print_gps(gps);

                printf("google maps link: http://www.google.com/maps/place/%d.%d, %d.%d",
                gps.lat_integer, gps.lat_fractional,
                gps.lng_integer, gps.lng_fractional);
            }
        }
    }

    return 0;
}
