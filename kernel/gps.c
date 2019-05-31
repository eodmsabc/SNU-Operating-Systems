#include <linux/syscalls.h>
#include <linux/uaccess.h>
//#include <linux/path.h>
#include <linux/namei.h>
#include <linux/gps.h>
#include <linux/spinlock.h>

#define PRECISION 1000000
#define EARTH_R 6371000

extern int inode_permission_without_gps(struct inode*, int);

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

struct myFloat {
    long long int integer;
    long long int fractional;
};

static const struct myFloat MF_ZERO = {0, 0};

//#define MFLOAT(a,b) ((struct myFloat){(a), (b)})
#define MY_PI MFLOAT(3, 141593)

struct myFloat MFLOAT(long long int a, long long int b)
{
    struct myFloat retval = { a, b };
    return retval;
}

/*
static inline struct myFloat myfloat(int a, int b)
{
    return (struct myFloat){a, b};
}
*/
struct myFloat _carry_myFloat(struct myFloat mf)
{
    struct myFloat result;
    long long int carry = mf.fractional / PRECISION;

    result.integer = mf.integer + carry;
    result.fractional = mf.fractional % PRECISION;

    if (result.fractional < 0) {
        result.fractional += PRECISION;
        result.integer -= 1;
    }

    return result;
}

struct myFloat int_to_float(long long int n)
{
    struct myFloat result;
    result = MFLOAT(n / PRECISION, n % PRECISION);
    result = _carry_myFloat(result);

    return result;
}

struct myFloat add_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    struct myFloat result;
    
    result.integer = mf1.integer + mf2.integer;
    result.fractional = mf1.fractional + mf2.fractional;

    result = _carry_myFloat(result);

    return result;
}

struct myFloat sub_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    struct myFloat result;

    result.integer = mf1.integer - mf2.integer - 1;
    result.fractional = mf1.fractional - mf2.fractional + PRECISION;

    result = _carry_myFloat(result);

    return result;
}

struct myFloat avg_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    long long int _result;
    struct myFloat result;

    _result = (mf1.integer * PRECISION + mf1.fractional + mf2.integer * PRECISION + mf2.fractional) / 2;
    result = int_to_float(_result);

    return result;
}

struct myFloat mul_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    struct myFloat result;
    
    result.integer = mf1.integer * mf2.integer;
    result.fractional = 0;
    
    result.integer += mf1.integer * mf2.fractional / PRECISION;
    result.fractional += mf1.integer * mf2.fractional % PRECISION;

    result.integer += mf1.fractional * mf2.integer / PRECISION;
    result.fractional += mf1.fractional * mf2.integer % PRECISION;
    
    result.fractional += mf1.fractional * mf2.fractional / PRECISION;

    result = _carry_myFloat(result);

    return result;
}

struct myFloat deg_to_rad(struct myFloat mf)
{
    struct myFloat result;
    long long int _result;

    result = mul_myFloat(mf, MY_PI);

    _result = (result.integer * PRECISION + result.fractional) / 180;
    
    result = int_to_float(_result);

    return result;
}

struct myFloat neg_myFloat(struct myFloat mf)
{
    return sub_myFloat(MFLOAT(0, 0), mf);
}

// mf1 == mf2
int eq_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    if ((mf1.integer == mf2.integer) &&
        (mf1.fractional == mf2.fractional))
        return 1;
    return 0;
}

// mf1 < mf2
int lt_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    if (mf1.integer < mf2.integer)
        return 1;

    if (mf1.integer > mf2.integer)
        return 0;

    if (mf1.fractional < mf2.fractional)
        return 1;
    
    return 0;
}

// mf1 <= mf2
int lteq_myFloat(struct myFloat mf1, struct myFloat mf2)
{
    if (lt_myFloat(mf1, mf2) || eq_myFloat(mf1, mf2))
        return 1;
    
    return 0;
}

struct myFloat abs_myFloat(struct myFloat mf)
{
    if (lt_myFloat(mf, MF_ZERO))
        return neg_myFloat(mf);
    return mf;
}

int _sin_degree[90] = {  0,  17452,  34899,  52336,  69756,  87156, 104528, 121869, 139173, 156434, 
                    173648, 190809, 207912, 224951, 241922, 258819, 275637, 292372, 309017, 325568, 
                    342020, 358368, 374607, 390731, 406737, 422618, 438371, 453990, 469472, 484810, 
                    500000, 515038, 529919, 544639, 559193, 573576, 587785, 601815, 615661, 629320, 
                    642788, 656059, 669131, 681998, 694658, 707107, 719340, 731354, 743145, 754710, 
                    766044, 777146, 788011, 798636, 809017, 819152, 829038, 838671, 848048, 857167, 
                    866025, 874620, 882948, 891007, 898794, 906308, 913545, 920505, 927184, 933580, 
                    939693, 945519, 951057, 956305, 961262, 965926, 970296, 974370, 978148, 981627, 
                    984808, 987688, 990268, 992546, 994522, 996195, 997564, 998630, 999391, 999848};

struct myFloat sin_myFloat(struct myFloat deg)
{
    long long int left, right, result;

    // deg < 0 then sin(deg) = -sin(-deg)
    if (lt_myFloat(deg, MF_ZERO))
        return neg_myFloat(sin_myFloat(neg_myFloat(deg)));

    // deg == 90 then return 1.0
    if (eq_myFloat(deg, MFLOAT(90, 0)))
        return MFLOAT(1, 0);

    // 90 < deg then sin(deg) = sin(180 - deg)
    if (lt_myFloat(MFLOAT(90, 0), deg))
        return sin_myFloat(sub_myFloat(MFLOAT(180, 0), deg));

    // from here, 0 <= deg <= 89.999999

    // if deg is integer
    if (deg.fractional == 0)
        return MFLOAT(0, _sin_degree[deg.integer]);

    // interpolation
    left = _sin_degree[deg.integer];
    right = (deg.integer == 89)? PRECISION : _sin_degree[deg.integer + 1];

    result = (left * (PRECISION - deg.fractional) + right * deg.fractional) / PRECISION;

    if (result >= PRECISION)
        result = PRECISION - 1;

    return MFLOAT(0, result);
}

struct myFloat cos_myFloat(struct myFloat deg)
{
    return sin_myFloat(sub_myFloat(MFLOAT(90, 0), deg));
}

struct myFloat deg_arc_len(struct myFloat deg, int radius)
{
    // TODO
    struct myFloat rad = deg_to_rad(deg);
    return mul_myFloat(rad, MFLOAT(radius, 0));
}

static int valid_gps_location(struct gps_location loc)
{
    struct myFloat lat = {loc.lat_integer, loc.lat_fractional};
    struct myFloat lng = {loc.lng_integer, loc.lng_fractional};

    if  (lteq_myFloat(MFLOAT( -90, 0), lat) &&
        (lteq_myFloat(lat, MFLOAT(  90, 0))) &&
        (lteq_myFloat(MFLOAT(-180, 0), lng)) &&
        (lteq_myFloat(lng, MFLOAT( 180, 0))))
        return 1;
    
    return 0;
}

// this function called in fs/namei.c (generic_permission function.)
// return 1 if current location & file location overlap (have permission), else return 0 (not have permission)
int check_gps_permission(struct gps_location loc)
{
	struct gps_location curr_loc;
    //int lat_diff, lng_diff, diff_mul;
    struct myFloat cur_lat, cur_lng, loc_lat, loc_lng, avg_lat, lat_diff, lng_diff, dx, dy, diagsq;
    long long int rot_radius, dist;

	spin_lock(&gps_lock);
	curr_loc = current_location;
    spin_unlock(&gps_lock);

    /*
    lat_diff = curr_loc.lat_integer - loc.lat_integer;
    lng_diff = curr_loc.lng_integer - loc.lng_integer;
    diff_mul = lat_diff * lng_diff;

    if (-100 < diff_mul && diff_mul < 100) {
        return 1;
    }
    return 0;
    */

    cur_lat = MFLOAT(curr_loc.lat_integer, curr_loc.lat_fractional);
    cur_lng = MFLOAT(curr_loc.lng_integer, curr_loc.lng_fractional);
    loc_lat = MFLOAT(loc.lat_integer, loc.lat_fractional);
    loc_lng = MFLOAT(loc.lng_integer, loc.lng_fractional);

    avg_lat = avg_myFloat(cur_lat, loc_lat);
    lat_diff = abs_myFloat(sub_myFloat(cur_lat, loc_lat));
    lng_diff = abs_myFloat(sub_myFloat(cur_lng, loc_lng));

    printk(KERN_ALERT "lat_diff is %lld.%lld, lng_diff is %lld.%lld",
            lat_diff.integer, lat_diff.fractional,
            lng_diff.integer, lng_diff.fractional);

    // if 180 < lng_diff then lng_diff = 360 - lng_diff
    if (lt_myFloat(MFLOAT(180, 0), lng_diff))
        lng_diff = sub_myFloat(MFLOAT(360, 0), lng_diff);

    rot_radius = EARTH_R;
    rot_radius *= cos_myFloat(avg_lat).fractional;
    rot_radius /= PRECISION;

    printk(KERN_ALERT "rot_radius is %lld",
            rot_radius);

    dist = curr_loc.accuracy + loc.accuracy;
    
    dx = deg_arc_len(lng_diff, rot_radius);
    dy = deg_arc_len(lat_diff, EARTH_R);

    diagsq = add_myFloat(mul_myFloat(dx, dx), mul_myFloat(dy, dy));

    printk(KERN_ALERT "dist is %lld",
            dist);

    printk(KERN_ALERT "diagsq is %lld.%lld",
            diagsq.integer, diagsq.fractional);

    return lteq_myFloat(diagsq, MFLOAT(dist * dist, 0));
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


    spin_lock(&gps_lock);
    current_location = new_location;
    spin_unlock(&gps_lock);

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
	//char * ker_pathname;
    int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;
    if (!pathname || !loc){
        return -EINVAL;
	}

    // statfs.c
    // sys_statfs
    // -> user_statfs
    // -> user_path_at
    /*
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

    if (kern_path(ker_pathname, LOOKUP_FOLLOW, &path))
	{
		kfree(ker_pathname);
        return -EINVAL;
	}
    */
    if (user_path_at(AT_FDCWD, pathname, lookup_flags, &path)) {
        return -EFAULT;
    }
    inode = path.dentry->d_inode;

	if(generic_permission(inode, MAY_READ))	// it calls generic_permission inside, so we don't care..
	{
		//kfree(ker_pathname);
        return -EACCES;
	}

	if(inode->i_op->get_gps_location)
		inode->i_op->get_gps_location(inode, &k_file_loc);
	else
	{
		//kfree(ker_pathname);
		return -EFAULT;
	}	// file is not EXT2 file system.
    
    // TODO
    // Need to add struct gps_location into inode
    // get proper information from it
    
    if (copy_to_user(loc, &k_file_loc, sizeof(struct gps_location)))
	{
		//kfree(ker_pathname);
		return -EFAULT;
	}
        
	//kfree(ker_pathname);
    return 0;
}
