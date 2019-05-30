#ifndef _PROJ4_LINUX_GPS_H
#define _PROJ4_LINUX_GPS_H

struct gps_location {
    int lat_integer;
    int lat_fractional;
    int lng_integer;
    int lng_fractional;
    int accuracy;
};

struct gps_location get_current_location(void);
//int get_gps_permission(gps_location *loc);

#endif
