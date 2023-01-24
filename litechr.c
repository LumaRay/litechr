#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>

//Device details 
#define DEVICE_NAME "litechr"

#define MAX_BUF_SIZE 1000

static dev_t litechr_dev;
static struct cdev litechr_cdev;
static struct class *plitechr_class;

static int litechr_open(struct inode *pinode, struct file *pfile);
static int litechr_release(struct inode *pinode, struct file *pfile);
static ssize_t litechr_read(struct file *pfile, char *ubuf, size_t length, loff_t *poffset);
static ssize_t litechr_write(struct file *pfile, const char *ubuf, size_t length, loff_t *poffset);

static int litechr_uevent(struct device *pdev, struct kobj_uevent_env *penv);

static struct file_operations litechr_fops = {
    .owner = THIS_MODULE,
    .open = litechr_open,
    .release = litechr_release,
    .read = litechr_read,
    .write = litechr_write,
};

struct data_queue_entry_t {
    struct list_head entry_head;
    uint8_t data_byte;
};

static LIST_HEAD(data_queue_head);
static struct list_head *pdata_queue_tail = &data_queue_head;

//static struct data_queue_entry_t data_queue;

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
    struct data_queue_entry_t *pentry, *ptentry;
    size_t count = 0;
    list_for_each_entry_safe(pentry, ptentry, &data_queue_head, entry_head) {
        list_del(&pentry->entry_head);
        kfree(pentry);
	count++;
    }
    pr_info("Cleared %ld entries\n", count);

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

static ssize_t litechr_read(struct file *pfile, char *ubuf, size_t length, loff_t *poffset)
{
    size_t buf_len;
    char *kbuf;
    struct data_queue_entry_t *pentry;

    if (*poffset != 0)
        return -ESPIPE;
    if (length == 0)
        return -EINVAL;
    if (ubuf == NULL)
        return -EINVAL;
    
    kbuf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
    if (kbuf == NULL)
        return -ENOMEM;

    pr_info("Sending data: ");

    for (   buf_len = 0;
            buf_len < length && !list_empty(pdata_queue_tail);
            ++buf_len ) {
        pentry = list_entry(pdata_queue_tail, struct data_queue_entry_t, entry_head);
        kbuf[buf_len] = pentry->data_byte;
        pdata_queue_tail = pdata_queue_tail->next;
        list_del(&pentry->entry_head);
        kfree(pentry);
        pr_cont("%c (0x%02X) ", kbuf[buf_len], kbuf[buf_len]);
    }
    pr_info("");

    if (copy_to_user(ubuf, kbuf, buf_len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kfree(kbuf);

    return buf_len;
}

static ssize_t litechr_write(struct file *pfile, const char *ubuf, size_t length, loff_t *poffset)
{
    uint8_t *kbuf;
    int i;
    struct data_queue_entry_t *pnew_entry;
    
    if (*poffset != 0)
        return -ESPIPE;
    if (length == 0)
        return -EINVAL;
    if (length > MAX_BUF_SIZE)
        return -ENOBUFS;
    if (ubuf == NULL)
        return -EINVAL;

    kbuf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
    if (kbuf == NULL)
        return -ENOMEM;

    if (copy_from_user(kbuf, ubuf, length)) {
        kfree(kbuf);
        return -EFAULT;
    }

    pr_info("Incoming data: ");
    for (i = 0; i < length; ++i) {
        pnew_entry = kzalloc(sizeof(struct data_queue_entry_t), GFP_KERNEL);
        if (pnew_entry == NULL) {
            kfree(kbuf);		
            return -ENOMEM;
        }
        pnew_entry->data_byte = kbuf[i];
	INIT_LIST_HEAD(&pnew_entry->entry_head);
	if (list_empty(&data_queue_head))
            pdata_queue_tail = &pnew_entry->entry_head;
	list_add_tail(&pnew_entry->entry_head, &data_queue_head);
	pr_cont("%c (0x%02X) ", kbuf[i], kbuf[i]);
    }
    pr_info("");

    kfree(kbuf);

    return length;
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
