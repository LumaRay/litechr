#pragma once

// Driver open file callback
static int litechr_open(struct inode *pinode, struct file *pfile);
// Driver close file callback
static int litechr_release(struct inode *pinode, struct file *pfile);
// Driver read file callback
static ssize_t litechr_read(struct file *pfile, char *ubuf, size_t length, loff_t *poffset);
// Driver write file callback
static ssize_t litechr_write(struct file *pfile, const char *ubuf, size_t length, loff_t *poffset);

// Set access rights for the device file
static int litechr_uevent(struct device *pdev, struct kobj_uevent_env *penv);
