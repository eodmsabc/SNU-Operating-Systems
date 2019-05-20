#include <linux/syscalls.h>
#include <linux/uaccess.h>
//#include <linux/path.h>
#include <linux/namei.h>
#include <linux/gps.h>

static struct gps_location current_location = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0
};


static int valid_gps_location(struct gps_location loc)
{
    if ((-90 <= loc.lat_integer && loc.lat_integer <= 90) &&
        (-180 <= loc.lng_integer && loc.lng_integer <= 180) &&
        (0 <= loc.lat_fractional && loc.lat_fractional <= 999999) &&
        (0 <= loc.lng_fractional && loc.lng_fractional <= 999999))
        return 0;
    return -1; 
}

/*
 * sys_set_gps_location - set device's GPS location
 * loc: gps information in user-space
 *
 * Return 0 on success. An error code otherwise.
 */
SYSCALL_DEFINE1(set_gps_location, struct gps_location __user *, loc)
{
    struct gps_location new_location;

    if (!loc)
        return -EINVAL;
    if (copy_from_user(&new_location, loc, sizeof(struct gps_location)))
        return -EFAULT;

    if (!valid_gps_location(new_location))
        return -EINVAL;

    current_location = new_location;

    return 0;
}

/*
 * sys_get_gps_location - fill the file's gps information into loc
 * @pathname: file path
 * @loc: location data will be filled in here
 *
 * Return: 0 on success. An error code otherwise.
 */
SYSCALL_DEFINE2(get_gps_location, const char __user *, pathname, struct gps_location __user *, loc)
{
    struct gps_location k_file_loc;
    struct inode *inode;
    struct path path;

    if (!pathname || !loc)
        return -EINVAL;

    if (kern_path(pathname, LOOKUP_FOLLOW, &path))
        return -EINVAL;

    inode = path.dentry->d_inode;
    
    // TODO
    // Need to add struct gps_location into inode
    // get proper information from it
    
    if (copy_to_user(loc, &k_file_loc, sizeof(struct gps_location)))
        return -EFAULT;

    return 0;
}
