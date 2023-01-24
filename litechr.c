#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>

//Device details 
#define DEVICE_NAME "litechr"

static dev_t litechr_dev;
static struct cdev litechr_cdev;
static struct class *plitechr_class;

static int litechr_open(struct inode *pinode, struct file *pfile);
static int litechr_release(struct inode *pinode, struct file *pfile);
static ssize_t litechr_read(struct file *pfile, char *buffer, size_t length, loff_t *poffset);
static ssize_t litechr_write(struct file *pfile, const char *buffer, size_t length, loff_t *poffset);

static int litechr_uevent(struct device *pdev, struct kobj_uevent_env *penv);

static struct file_operations litechr_fops = {
    .owner = THIS_MODULE,
    .open = litechr_open,
    .release = litechr_release,
    .read = litechr_read,
    .write = litechr_write,
};

// Initialize the driver
static int __init litechr_init(void)
{
    int ret;
    struct device *pdevice_pcd;

    // Allocate device number with a single minor number
    if ((ret = alloc_chrdev_region(&litechr_dev, 0, 1, DEVICE_NAME)) < 0) {
        pr_err("Failed to allocate device number\n");
	return ret;
    }
    else
        pr_info("Device number %u allocated\n", MAJOR(litechr_dev));

    // Create class
    if (IS_ERR(plitechr_class = class_create(THIS_MODULE, DEVICE_NAME))) {
        pr_err("Failed to create class\n");        
	ret = PTR_ERR(plitechr_class);
	goto un_reg;
    }

    // Set driver file permissions callback
    plitechr_class->dev_uevent = litechr_uevent;

    // Initialize the character device
    cdev_init(&litechr_cdev, &litechr_fops);

    litechr_cdev.owner = THIS_MODULE;

    // Add device to system
    if ((ret = cdev_add(&litechr_cdev, litechr_dev, 1)) < 0) {
        pr_err(KERN_ERR "Failed to add device to the system\n");
	goto un_class;
    }
 
    // Create device file node and register it with sysfs
    if (IS_ERR(pdevice_pcd = device_create(plitechr_class, NULL, litechr_dev, NULL, DEVICE_NAME))) {
        pr_err("Failed to create device\n");        
	ret = PTR_ERR(pdevice_pcd);
	goto un_add;
    }
 
    pr_info("Linux Character Driver successfully initialized\n");

    return 0;

un_add:
    // Delete character device
    cdev_del(&litechr_cdev);
un_class:
    class_unregister(plitechr_class);
    class_destroy(plitechr_class);
un_reg:
    // Delete device number
    unregister_chrdev_region(litechr_dev, 1);
    return ret;
}

// Deinitialize driver
static void __exit litechr_exit(void)
{
    device_destroy(plitechr_class, litechr_dev);
 
    // Delete character device
    cdev_del(&litechr_cdev);

    class_unregister(plitechr_class);

    class_destroy(plitechr_class);
 
    // Delete device number
    unregister_chrdev_region(litechr_dev, 1);
 
    pr_info("Lite Character Driver successfully uninitialized\n");
}

static int litechr_open(struct inode *pinode, struct file *pfile)
{
    //TODO: Add your code here
 
    return 0;
}

static int litechr_release(struct inode *pinode, struct file *pfile)
{
    //TODO: Add your code here
 
    return 0;
}

static ssize_t litechr_read(struct file *pfile, char *buffer, size_t length, loff_t *poffset)
{
    //TODO: Add your code here
 
    return 0;
}

static ssize_t litechr_write(struct file *pfile, const char *buffer, size_t length, loff_t *poffset)
{
    //TODO: Add your code here
 
    return 0;
}

static int litechr_uevent(struct device *pdev, struct kobj_uevent_env *penv)
{
    add_uevent_var(penv, "DEVMODE=%#o", 0666);

    return 0;
}

module_init(litechr_init);
module_exit(litechr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lite Character Device Driver");
MODULE_AUTHOR("Yury Laykov");
