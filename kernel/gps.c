#include <linux/syscalls.h>
#include <linux/uaccess.h>
//#include <linux/path.h>
#include <linux/namei.h>
#include <linux/gps.h>
#include <linux/spinlock.h>


DEFINE_SPINLOCK(gps_lock);

static struct gps_location current_location = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0
};


struct gps_location get_current_location(void)
{
    struct gps_location ret_location;

    spin_lock(&gps_lock);
    ret_location = current_location;
    spin_unlock(&gps_lock);

    return ret_location;
}

static int valid_gps_location(struct gps_location loc)
{
    if(loc.lat_integer == 90 && loc.lat_fractional != 0) return -1;
    if(loc.lng_integer == 180 && loc.lng_fractional != 0) return -1;
    // exclude lat > 90, lng > 90.

    if ((-90 <= loc.lat_integer && loc.lat_integer <= 90) &&
        (-180 <= loc.lng_integer && loc.lng_integer <= 180) &&
        (0 <= loc.lat_fractional && loc.lat_fractional <= 999999) &&
        (0 <= loc.lng_fractional && loc.lng_fractional <= 999999))
        return 0;
    return -1; 
}

// this function called in fs/namei.c (generic_permission function.)
// return 1 if current location & file location overlap (have permission), else return 0 (not have permission)
int get_gps_permission(gps_location *loc)
{
	struct gps_location curr_location;

	spin_lock(&gps_lock);
	curr_location = current_location;
    spin_unlock(&gps_lock);
	// TODO implement

}

// returned location's accuracy equal to loc_1.
struct gps_location add_location(struct gps_location loc_1, struct gps_location loc_2)
{
    gps_location loc_ret = loc_1;

    loc_ret.lat_fractional = loc_1.lat_fractional + loc_2.lat_fractional;
    loc_ret.lng_fractional = loc_1.lng_fractional + loc_2.lng_fractional;
    loc_ret.lat_integer = loc_1.lat_integer + loc_2.lat_integer;
    loc_ret.lng_integer = loc_1.lng_integer + loc_2.lng_integer;
    
    
    loc_ret.lat_integer += loc_ret.lat_fractional / 1000000;
    loc_ret.lng_integer += loc_ret.lng_fractional / 1000000;
    loc_ret.lat_fractional = loc_ret.lat_fractional % 1000000;
    loc_ret.lng_fractional = loc_ret.lng_fractional % 1000000;

    return loc_ret;
}


// returned location's accuracy equal to loc_1.
struct gps_location sub_location(struct gps_location loc_1, struct gps_location loc_2)
{
    gps_location loc_ret = loc_1;

    loc_ret.lat_fractional = loc_1.lat_fractional - loc_2.lat_fractional + 1000000;
    loc_ret.lng_fractional = loc_1.lng_fractional - loc_2.lng_fractional + 1000000;
    loc_ret.lat_integer = loc_1.lat_integer - loc_2.lat_integer - 1;
    loc_ret.lng_integer = loc_1.lng_integer - loc_2.lng_integer - 1;
    // first carry down.
    
    loc_ret.lat_integer += loc_ret.lat_fractional / 1000000;
    loc_ret.lng_integer += loc_ret.lng_fractional / 1000000;
    loc_ret.lat_fractional = loc_ret.lat_fractional % 1000000;
    loc_ret.lng_fractional = loc_ret.lng_fractional % 1000000;

    return loc_ret;
}


// returned location's accuracy equal to loc_1.
struct gps_location mul_location(struct gps_location loc_1, struct gps_location loc_2)
{
    gps_location loc_ret;

    long long int lat_1, lng_1, lat_2, lng_2;

    lat_1 = ((long long int) loc_1.lat_integer) * 1000000;
    lat_1 += (long long int) loc_1.lat_fractional;

    lat_2 = ((long long int) loc_2.lat_integer) * 1000000;
    lat_2 += (long long int) loc_2.lat_fractional;

    lng_1 = ((long long int) loc_1.lng_integer) * 1000000;
    lng_1 += (long long int) loc_1.lng_fractional;

    lng_2 = ((long long int) loc_2.lng_integer) * 1000000;
    lng_2 += (long long int) loc_2.lng_fractional;

    lat_1 = lat_1 * lat_2;
    lng_1 = lng_1 * lng_2;

    loc_ret.lat_integer = (int) (lat_1 / (1000000 * 1000000));
    loc_ret.lng_integer = (int) (lng_1 / (1000000 * 1000000));
    loc_ret.lat_fractional = (int) (((lat_1 % 1000000) >= 500000) ?  // round up
 							((lat_1 % (1000000 * 1000000)) / 1000000) + 1 : ((lat_1 % (1000000 * 1000000)) / 1000000));
	loc_ret.lng_fractional = (int) (((lng_1 % 1000000) >= 500000) ?  // round up
 							((lng_1 % (1000000 * 1000000)) / 1000000) + 1 : ((lng_1 % (1000000 * 1000000)) / 1000000));
    loc_ret.accuracy = loc_1.accuracy;

    return loc_ret;
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

    if(!access_ok(VERIFY_READ, loc, sizeof(strucy gps_location)))
        return -EFAULT;

    if (copy_from_user(&new_location, loc, sizeof(struct gps_location)))
        return -EFAULT;

    if (!valid_gps_location(new_location))
        return -EINVAL;


    spin_lock(&gps_lock);
    current_location = new_location;
    spin_unlock(&gps_unlock);

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
	char * ker_pathname;

    if (!pathname || !loc){
        return -EINVAL;
	}

	ker_pathname = kmalloc(sizeof(char) * 1024, GFP_KERNEL);
	if(ker_pathname == NULL)
	{
		kfree(ker_pathname);
		return -ENOMEM;
	}

	if(strncpy_from_user(ker_pathname, pathname, sizeof(char) * 1024) < 0)
	{
		kfree(ker_pathname);
		return -EFAULT;
	}

    if(!access_ok(VERIFY_WRITE, loc, sizeof(struct gps_location)))
	{
		kfree(ker_pathname);
		return -EFAULT;
	}
        

    if (kern_path(ker_pathname, LOOKUP_FOLLOW, &path))
	{
		kfree(ker_pathname);
        return -EINVAL;
	}
    inode = path.dentry->d_inode;

	if(inode_permission(inode, MAY_READ))	// it calls generic_permission inside, so we don't care..
	{
		kfree(ker_pathname);
        return -EACCES;
	}

	if(inode->i_op->get_gps_location)
		inode->i_op->get_gps_location(inode, &k_file_loc);
	else
	{
		kfree(ker_pathname);
		return -EFAULT;
	}	// file is not EXT2 file system.
    
    // TODO
    // Need to add struct gps_location into inode
    // get proper information from it
    
    if (copy_to_user(loc, &k_file_loc, sizeof(struct gps_location)))
	{
		kfree(ker_pathname);
		return -EFAULT;
	}
        
	kfree(ker_pathname);
    return 0;
}
