#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gps.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gps.h>
#include <errno.h>
#include <string.h>

#define SYSCALL_SET_GPS_LOCATION 398

void print_gps(struct gps_location loc)
{
    printf("latitude\t\tlongitude\t\taccuracy(m)\n"
            "%d.%06d\t\t%d.%06d\t\t%d\n",
            loc.lat_integer, loc.lat_fractional,
            loc.lng_integer, loc.lng_fractional,
            loc.accuracy);
}

void input_gps(struct gps_location *loc)
{  
    int input;
    printf("insert lat_integer : ");
    scanf("%d", &input);
    loc->lat_integer = input;
    printf("insert lat_fractional : ");
    scanf("%d", &input);
    loc->lat_fractional = input;
    printf("insert lng_integer : ");
    scanf("%d", &input);
    loc->lng_integer = input;
    printf("insert lng_fractional : ");
    scanf("%d", &input);
    loc->lng_fractional = input;
    printf("insert accuracy : ");
    scanf("%d", &input);
    loc->accuracy = input;
}


int main(int argc, char* argv[])
{
    struct gps_location loc;
    int input;

    input_gps(&loc);
    if (syscall(SYSCALL_SET_GPS_LOCATION, &loc) != 0) {
        printf("GPS SET ERROR!: %s\n", strerror(errno));
    }
    else {
        printf("system's gps setted to \n");
        print_gps(loc);
    }
    
    return 0;
}
