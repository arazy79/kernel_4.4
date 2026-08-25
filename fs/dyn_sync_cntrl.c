/*
 * Dynamic sync control driver
 * Credits: faux123, franciscofranco, Ry
 *
 * Compatible: kernel 4.4, Qualcomm CAF (fb_notifier)
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/dyn_sync_cntrl.h>

#define DYN_FSYNC_VERSION_MAJOR 2
#define DYN_FSYNC_VERSION_MINOR 0

bool dyn_fsync_active = true;
EXPORT_SYMBOL(dyn_fsync_active);

bool suspend_active __read_mostly = false;
EXPORT_SYMBOL(suspend_active);

static int fb_notifier_callback(struct notifier_block *self,
                                unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;

    if (event != FB_EVENT_BLANK)
        return NOTIFY_DONE;

    if (!evdata || !evdata->data)
        return NOTIFY_DONE;

    blank = evdata->data;

    switch (*blank) {
    case FB_BLANK_UNBLANK:
        /* Screen ON - disable sync for performance */
        suspend_active = false;
        break;
    case FB_BLANK_POWERDOWN:
    case FB_BLANK_HSYNC_SUSPEND:
    case FB_BLANK_VSYNC_SUSPEND:
    case FB_BLANK_NORMAL:
        /* Screen OFF - enable sync for data safety */
        suspend_active = true;
        break;
    }

    return NOTIFY_OK;
}

static struct notifier_block fb_notif = {
    .notifier_call = fb_notifier_callback,
};

/* Sysfs: /sys/kernel/dyn_fsync/Dyn_fsync_active */
static ssize_t dyn_fsync_active_show(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     char *buf)
{
    return sprintf(buf, "%u\n", (dyn_fsync_active ? 1 : 0));
}

static ssize_t dyn_fsync_active_store(struct kobject *kobj,
                                      struct kobj_attribute *attr,
                                      const char *buf, size_t count)
{
    unsigned int input;

    if (sscanf(buf, "%u", &input) != 1)
        return -EINVAL;

    dyn_fsync_active = (input ? true : false);
    return count;
}

static ssize_t dyn_fsync_version_show(struct kobject *kobj,
                                      struct kobj_attribute *attr,
                                      char *buf)
{
    return sprintf(buf, "version: %u.%u\n",
                   DYN_FSYNC_VERSION_MAJOR,
                   DYN_FSYNC_VERSION_MINOR);
}

static ssize_t dyn_fsync_suspend_show(struct kobject *kobj,
                                      struct kobj_attribute *attr,
                                      char *buf)
{
    return sprintf(buf, "%u\n", (suspend_active ? 1 : 0));
}

static struct kobj_attribute dyn_fsync_active_attribute =
    __ATTR(Dyn_fsync_active, 0644,
           dyn_fsync_active_show, dyn_fsync_active_store);

static struct kobj_attribute dyn_fsync_version_attribute =
    __ATTR(Dyn_fsync_version, 0444, dyn_fsync_version_show, NULL);

static struct kobj_attribute dyn_fsync_suspend_attribute =
    __ATTR(Dyn_fsync_suspend, 0444, dyn_fsync_suspend_show, NULL);

static struct attribute *dyn_fsync_attrs[] = {
    &dyn_fsync_active_attribute.attr,
    &dyn_fsync_version_attribute.attr,
    &dyn_fsync_suspend_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = dyn_fsync_attrs,
};

static struct kobject *dyn_fsync_kobj;

static int __init dyn_fsync_init(void)
{
    int ret;

    dyn_fsync_kobj = kobject_create_and_add("dyn_fsync", kernel_kobj);
    if (!dyn_fsync_kobj) {
        pr_err("[dyn_fsync]: kobject create failed\n");
        return -ENOMEM;
    }

    ret = sysfs_create_group(dyn_fsync_kobj, &attr_group);
    if (ret) {
        pr_err("[dyn_fsync]: sysfs create group failed\n");
        kobject_put(dyn_fsync_kobj);
        return ret;
    }

    ret = fb_register_client(&fb_notif);
    if (ret) {
        pr_err("[dyn_fsync]: fb_register_client failed: %d\n", ret);
        sysfs_remove_group(dyn_fsync_kobj, &attr_group);
        kobject_put(dyn_fsync_kobj);
        return ret;
    }

    pr_info("[dyn_fsync]: dynamic fsync v%d.%d initialized\n",
            DYN_FSYNC_VERSION_MAJOR, DYN_FSYNC_VERSION_MINOR);
    return 0;
}

static void __exit dyn_fsync_exit(void)
{
    fb_unregister_client(&fb_notif);
    sysfs_remove_group(dyn_fsync_kobj, &attr_group);
    kobject_put(dyn_fsync_kobj);
}

module_init(dyn_fsync_init);
module_exit(dyn_fsync_exit);

MODULE_AUTHOR("faux123, franciscofranco");
MODULE_DESCRIPTION("Dynamic fsync control");
MODULE_LICENSE("GPL v2");
