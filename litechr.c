#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#include "litechr.h"
#include "context.h"

#define DEVICE_NAME         "litechr"
// Maximum size of data queue
#define MAX_BUFFER_SIZE     1000
// Limit of simultaneously opened files
#define MAX_OPENED_FILES    1000
// Limit of file contexts used for multiple contexts mode
#define MAX_FILE_CONTEXTS   1001

static dev_t litechr_dev;
static struct cdev litechr_cdev;
static struct class *plitechr_class;

// Number of opened files
static unsigned int litechr_opened_files_count;

// Open/close operations mutex
static struct mutex litechr_openclose_mtx;

// The list head entry is used for shared/exclusive file data queue.
// The new entries are added dynamically to be used for separate file data queues.
static struct file_context litechr_file_context;
static unsigned int litechr_file_contexts_count;

static bool litechr_exclusive_mode;


// The main driver's file operations structure
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

    file_context_init(&litechr_file_context);

    litechr_file_contexts_count = 1;

    mutex_init(&litechr_openclose_mtx);
    
    litechr_opened_files_count = 0;
    
    litechr_exclusive_mode = false;
    
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
    struct file_context *pfile_ctx, *ptmp_file_ctx;

    // Clear shared file context
    file_context_data_queue_clear(&litechr_file_context);
    // Remove file contexts from list
    list_for_each_entry_safe(pfile_ctx, ptmp_file_ctx, &litechr_file_context.ctx_head, ctx_head) {
        file_context_remove(pfile_ctx);
    }

    device_destroy(plitechr_class, litechr_dev);
 
    // Delete character device
    cdev_del(&litechr_cdev);

    class_unregister(plitechr_class);

    class_destroy(plitechr_class);
 
    // Delete device number
    unregister_chrdev_region(litechr_dev, 1);
 
    pr_info("Lite Character Driver successfully uninitialized\n");
}

// Driver open file callback
static int litechr_open(struct inode *pinode, struct file *pfile)
{
    struct file_context* pnew_file_ctx;
    
    if (mutex_lock_interruptible(&litechr_openclose_mtx))
        return -EINTR;
    
    // Test open files limit
    if (litechr_opened_files_count >= MAX_OPENED_FILES) {
        pr_err("Maximum opened files count reached\n");
        mutex_unlock(&litechr_openclose_mtx);
        return -EMFILE;   
    }

    // Check for exclusive mode on
    if (litechr_opened_files_count > 0 && litechr_exclusive_mode) {
        pr_err("The device is already in exclusive mode\n");
        mutex_unlock(&litechr_openclose_mtx);
        return -EBUSY;
    }

    // Simultaneous O_CREAT and O_EXCL is not allowed - os controlled

    // Treat O_EXCL flag as the file being opened in exclusive mode
    if (pfile->f_flags & O_EXCL) {
        //pr_info("Opening with exclusive mode flag\n");
        // Check for exclusive mode on
        if (litechr_opened_files_count > 0) {
            pr_err("The device is busy\n");
            mutex_unlock(&litechr_openclose_mtx);
            return -EBUSY;
        }
        litechr_opened_files_count++;
        
        litechr_exclusive_mode = true;
        mutex_unlock(&litechr_openclose_mtx);
        return 0;
    }
    // Treat O_CREAT flag as the file being opened in multi context mode
    if (pfile->f_flags & O_CREAT) {
        //pr_info("Opening with create flag (multi context mode)\n");
        
        if (litechr_file_contexts_count >= MAX_FILE_CONTEXTS) {
            pr_err("Reached maximum file contexts count\n");
            mutex_unlock(&litechr_openclose_mtx);
            return -EBUSY;   
        }
        
        pnew_file_ctx = file_context_add(&litechr_file_context);
        if (IS_ERR(pnew_file_ctx)) {
            pr_err("Failed to add a new file context\n");
            mutex_unlock(&litechr_openclose_mtx);
            return PTR_ERR(pnew_file_ctx);
        }
        pfile->private_data = pnew_file_ctx;
        litechr_file_contexts_count++;
        
        litechr_opened_files_count++;
        mutex_unlock(&litechr_openclose_mtx);
        return 0;
    }
    // If the file is opened with neither O_CREAT nor O_EXCL flag consider it being opened in shared mode
    //pr_info("Opening with no flags (shared mode)\n");
    litechr_opened_files_count++;
    mutex_unlock(&litechr_openclose_mtx);
    return 0;
}

// Driver close file callback
static int litechr_release(struct inode *pinode, struct file *pfile)
{
    if (mutex_lock_interruptible(&litechr_openclose_mtx))
        return -EINTR;

    // If the file was opened in multi context mode, destroy it's context
    if (pfile->private_data) {
        file_context_remove(pfile->private_data);
        litechr_file_contexts_count--;
        pfile->private_data = NULL;
    }
    
    litechr_opened_files_count--;

    // If no more opened files, clear driver mode
    if (!litechr_opened_files_count) {
        litechr_exclusive_mode = false;
    }
    
    mutex_unlock(&litechr_openclose_mtx);
    
    //pr_info("Closed file\n");

    return 0;
}

// Driver read file callback
static ssize_t litechr_read(struct file *pfile, char *ubuf, size_t length, loff_t *poffset)
{
    struct file_context *pfile_ctx;
    char *kbuf;
    //if (*poffset != 0)
    //    return -ESPIPE;
    if (length == 0)
        return -EINVAL;
    if (ubuf == NULL)
        return -EINVAL;
    
    // If the file is opened in separate context, use it's unique context
    if (pfile->private_data) {
        pfile_ctx = pfile->private_data;
    }
    // Otherwise use shared context
    else {
        pfile_ctx = &litechr_file_context;
    }
    
    if (mutex_lock_interruptible(&pfile_ctx->data_queue.mtx))
        return -EINTR;

    length = min(length, pfile_ctx->data_queue.size);
    
    kbuf = kmalloc(length, GFP_KERNEL);
    if (kbuf == NULL) {
        mutex_unlock(&pfile_ctx->data_queue.mtx);
        return -ENOMEM;
    }

    length = file_context_data_queue_read_to_buffer(pfile_ctx, kbuf, length);

    mutex_unlock(&pfile_ctx->data_queue.mtx);

    if (copy_to_user(ubuf, kbuf, length)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kfree(kbuf);

    return length;
}

// Driver write file callback
static ssize_t litechr_write(struct file *pfile, const char *ubuf, size_t length, loff_t *poffset)
{
    struct file_context *pfile_ctx;
    uint8_t *kbuf;
    
    //if (*poffset != 0)
    //    return -ESPIPE;
    if (length == 0)
        return -EINVAL;
    if (ubuf == NULL)
        return -EINVAL;

    // If the file is opened in separate context, use it's unique context
    if (pfile->private_data) {
        pfile_ctx = pfile->private_data;
    }
    // Otherwise use shared context
    else {
        pfile_ctx = &litechr_file_context;
    }
    
    if (mutex_lock_interruptible(&pfile_ctx->data_queue.mtx))
        return -EINTR;

    if (length > MAX_BUFFER_SIZE - pfile_ctx->data_queue.size) {
        mutex_unlock(&pfile_ctx->data_queue.mtx);
        return -ENOBUFS;
    }
    kbuf = kmalloc(length, GFP_KERNEL);
    if (kbuf == NULL) {
        mutex_unlock(&pfile_ctx->data_queue.mtx);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, ubuf, length)) {
        mutex_unlock(&pfile_ctx->data_queue.mtx);
        kfree(kbuf);
        return -EFAULT;
    }

    length = file_context_data_queue_write_from_buffer(pfile_ctx, kbuf, length);

    mutex_unlock(&pfile_ctx->data_queue.mtx);

    kfree(kbuf);

    return length;
}

// Set access rights for the device file
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
