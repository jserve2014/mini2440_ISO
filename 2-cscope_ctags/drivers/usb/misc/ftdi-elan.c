/*
* USB FTDI client driver for Elan Digital Systems's Uxxx adapters
*
* Copyright(C) 2006 Elan Digital Systems Limited
* http://www.elandigitalsystems.com
*
* Author and Maintainer - Tony Olech - Elan Digital Systems
* tony.olech@elandigitalsystems.com
*
* This program is free software;you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation, version 2.
*
*
* This driver was written by Tony Olech(tony.olech@elandigitalsystems.com)
* based on various USB client drivers in the 2.6.15 linux kernel
* with constant reference to the 3rd Edition of Linux Device Drivers
* published by O'Reilly
*
* The U132 adapter is a USB to CardBus adapter specifically designed
* for PC cards that contain an OHCI host controller. Typical PC cards
* are the Orange Mobile 3G Option GlobeTrotter Fusion card.
*
* The U132 adapter will *NOT *work with PC cards that do not contain
* an OHCI controller. A simple way to test whether a PC card has an
* OHCI controller as an interface is to insert the PC card directly
* into a laptop(or desktop) with a CardBus slot and if "lspci" shows
* a new USB controller and "lsusb -v" shows a new OHCI Host Controller
* then there is a good chance that the U132 adapter will support the
* PC card.(you also need the specific client driver for the PC card)
*
* Please inform the Author and Maintainer about any PC cards that
* contain OHCI Host Controller and work when directly connected to
* an embedded CardBus slot but do not work when they are connected
* via an ELAN U132 adapter.
*
*/
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
MODULE_AUTHOR("Tony Olech");
MODULE_DESCRIPTION("FTDI ELAN driver");
MODULE_LICENSE("GPL");
#define INT_MODULE_PARM(n, v) static int n = v;module_param(n, int, 0444)
static int distrust_firmware = 1;
module_param(distrust_firmware, bool, 0);
MODULE_PARM_DESC(distrust_firmware, "true to distrust firmware power/overcurren"
        "t setup");
extern struct platform_driver u132_platform_driver;
static struct workqueue_struct *status_queue;
static struct workqueue_struct *command_queue;
static struct workqueue_struct *respond_queue;
/*
* ftdi_module_lock exists to protect access to global variables
*
*/
static struct mutex ftdi_module_lock;
static int ftdi_instances = 0;
static struct list_head ftdi_static_list;
/*
* end of the global variables protected by ftdi_module_lock
*/
#include "usb_u132.h"
#include <asm/io.h>
#include "../core/hcd.h"

	/* FIXME ohci.h is ONLY for internal use by the OHCI driver.
	 * If you're going to try stuff like this, you need to split
	 * out shareable stuff (register declarations?) into its own
	 * file, maybe name <linux/usb/ohci.h>
	 */

#include "../host/ohci.h"
/* Define these values to match your devices*/
#define USB_FTDI_ELAN_VENDOR_ID 0x0403
#define USB_FTDI_ELAN_PRODUCT_ID 0xd6ea
/* table of devices that work with this driver*/
static struct usb_device_id ftdi_elan_table[] = {
        {USB_DEVICE(USB_FTDI_ELAN_VENDOR_ID, USB_FTDI_ELAN_PRODUCT_ID)},
        { /* Terminating entry */ }
};

MODULE_DEVICE_TABLE(usb, ftdi_elan_table);
/* only the jtag(firmware upgrade device) interface requires
* a device file and corresponding minor number, but the
* interface is created unconditionally - I suppose it could
* be configured or not according to a module parameter.
* But since we(now) require one interface per device,
* and since it unlikely that a normal installation would
* require more than a couple of elan-ftdi devices, 8 seems
* like a reasonable limit to have here, and if someone
* really requires more than 8 devices, then they can frig the
* code and recompile
*/
#define USB_FTDI_ELAN_MINOR_BASE 192
#define COMMAND_BITS 5
#define COMMAND_SIZE (1<<COMMAND_BITS)
#define COMMAND_MASK (COMMAND_SIZE-1)
struct u132_command {
        u8 header;
        u16 length;
        u8 address;
        u8 width;
        u32 value;
        int follows;
        void *buffer;
};
#define RESPOND_BITS 5
#define RESPOND_SIZE (1<<RESPOND_BITS)
#define RESPOND_MASK (RESPOND_SIZE-1)
struct u132_respond {
        u8 header;
        u8 address;
        u32 *value;
        int *result;
        struct completion wait_completion;
};
struct u132_target {
        void *endp;
        struct urb *urb;
        int toggle_bits;
        int error_count;
        int condition_code;
        int repeat_number;
        int halted;
        int skipped;
        int actual;
        int non_null;
        int active;
        int abandoning;
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
                int toggle_bits, int error_count, int condition_code,
                int repeat_number, int halted, int skipped, int actual,
                int non_null);
};
/* Structure to hold all of our device specific stuff*/
struct usb_ftdi {
        struct list_head ftdi_list;
        struct mutex u132_lock;
        int command_next;
        int command_head;
        struct u132_command command[COMMAND_SIZE];
        int respond_next;
        int respond_head;
        struct u132_respond respond[RESPOND_SIZE];
        struct u132_target target[4];
        char device_name[16];
        unsigned synchronized:1;
        unsigned enumerated:1;
        unsigned registered:1;
        unsigned initialized:1;
        unsigned card_ejected:1;
        int function;
        int sequence_num;
        int disconnected;
        int gone_away;
        int stuck_status;
        int status_queue_delay;
        struct semaphore sw_lock;
        struct usb_device *udev;
        struct usb_interface *interface;
        struct usb_class_driver *class;
        struct delayed_work status_work;
        struct delayed_work command_work;
        struct delayed_work respond_work;
        struct u132_platform_data platform_data;
        struct resource resources[0];
        struct platform_device platform_dev;
        unsigned char *bulk_in_buffer;
        size_t bulk_in_size;
        size_t bulk_in_last;
        size_t bulk_in_left;
        __u8 bulk_in_endpointAddr;
        __u8 bulk_out_endpointAddr;
        struct kref kref;
        u32 controlreg;
        u8 response[4 + 1024];
        int expected;
        int recieved;
        int ed_found;
};
#define kref_to_usb_ftdi(d) container_of(d, struct usb_ftdi, kref)
#define platform_device_to_usb_ftdi(d) container_of(d, struct usb_ftdi, \
        platform_dev)
static struct usb_driver ftdi_elan_driver;
static void ftdi_elan_delete(struct kref *kref)
{
        struct usb_ftdi *ftdi = kref_to_usb_ftdi(kref);
        dev_warn(&ftdi->udev->dev, "FREEING ftdi=%p\n", ftdi);
        usb_put_dev(ftdi->udev);
        ftdi->disconnected += 1;
        mutex_lock(&ftdi_module_lock);
        list_del_init(&ftdi->ftdi_list);
        ftdi_instances -= 1;
        mutex_unlock(&ftdi_module_lock);
        kfree(ftdi->bulk_in_buffer);
        ftdi->bulk_in_buffer = NULL;
}

static void ftdi_elan_put_kref(struct usb_ftdi *ftdi)
{
        kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_elan_get_kref(struct usb_ftdi *ftdi)
{
        kref_get(&ftdi->kref);
}

static void ftdi_elan_init_kref(struct usb_ftdi *ftdi)
{
        kref_init(&ftdi->kref);
}

static void ftdi_status_requeue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (!queue_delayed_work(status_queue, &ftdi->status_work, delta))
		kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_status_queue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (queue_delayed_work(status_queue, &ftdi->status_work, delta))
		kref_get(&ftdi->kref);
}

static void ftdi_status_cancel_work(struct usb_ftdi *ftdi)
{
        if (cancel_delayed_work(&ftdi->status_work))
                kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_command_requeue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (!queue_delayed_work(command_queue, &ftdi->command_work, delta))
		kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_command_queue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (queue_delayed_work(command_queue, &ftdi->command_work, delta))
		kref_get(&ftdi->kref);
}

static void ftdi_command_cancel_work(struct usb_ftdi *ftdi)
{
        if (cancel_delayed_work(&ftdi->command_work))
                kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_response_requeue_work(struct usb_ftdi *ftdi,
        unsigned int delta)
{
	if (!queue_delayed_work(respond_queue, &ftdi->respond_work, delta))
		kref_put(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_respond_queue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (queue_delayed_work(respond_queue, &ftdi->respond_work, delta))
		kref_get(&ftdi->kref);
}

static void ftdi_response_cancel_work(struct usb_ftdi *ftdi)
{
        if (cancel_delayed_work(&ftdi->respond_work))
                kref_put(&ftdi->kref, ftdi_elan_delete);
}

void ftdi_elan_gone_away(struct platform_device *pdev)
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        ftdi->gone_away += 1;
        ftdi_elan_put_kref(ftdi);
}


EXPORT_SYMBOL_GPL(ftdi_elan_gone_away);
static void ftdi_release_platform_dev(struct device *dev)
{
        dev->parent = NULL;
}

static void ftdi_elan_do_callback(struct usb_ftdi *ftdi,
        struct u132_target *target, u8 *buffer, int length);
static void ftdi_elan_kick_command_queue(struct usb_ftdi *ftdi);
static void ftdi_elan_kick_respond_queue(struct usb_ftdi *ftdi);
static int ftdi_elan_setupOHCI(struct usb_ftdi *ftdi);
static int ftdi_elan_checkingPCI(struct usb_ftdi *ftdi);
static int ftdi_elan_enumeratePCI(struct usb_ftdi *ftdi);
static int ftdi_elan_synchronize(struct usb_ftdi *ftdi);
static int ftdi_elan_stuck_waiting(struct usb_ftdi *ftdi);
static int ftdi_elan_command_engine(struct usb_ftdi *ftdi);
static int ftdi_elan_respond_engine(struct usb_ftdi *ftdi);
static int ftdi_elan_hcd_init(struct usb_ftdi *ftdi)
{
        int result;
        if (ftdi->platform_dev.dev.parent)
                return -EBUSY;
        ftdi_elan_get_kref(ftdi);
        ftdi->platform_data.potpg = 100;
        ftdi->platform_data.reset = NULL;
        ftdi->platform_dev.id = ftdi->sequence_num;
        ftdi->platform_dev.resource = ftdi->resources;
        ftdi->platform_dev.num_resources = ARRAY_SIZE(ftdi->resources);
        ftdi->platform_dev.dev.platform_data = &ftdi->platform_data;
        ftdi->platform_dev.dev.parent = NULL;
        ftdi->platform_dev.dev.release = ftdi_release_platform_dev;
        ftdi->platform_dev.dev.dma_mask = NULL;
        snprintf(ftdi->device_name, sizeof(ftdi->device_name), "u132_hcd");
        ftdi->platform_dev.name = ftdi->device_name;
        dev_info(&ftdi->udev->dev, "requesting module '%s'\n", "u132_hcd");
        request_module("u132_hcd");
        dev_info(&ftdi->udev->dev, "registering '%s'\n",
                ftdi->platform_dev.name);
        result = platform_device_register(&ftdi->platform_dev);
        return result;
}

static void ftdi_elan_abandon_completions(struct usb_ftdi *ftdi)
{
        mutex_lock(&ftdi->u132_lock);
        while (ftdi->respond_next > ftdi->respond_head) {
                struct u132_respond *respond = &ftdi->respond[RESPOND_MASK &
                        ftdi->respond_head++];
                *respond->result = -ESHUTDOWN;
                *respond->value = 0;
                complete(&respond->wait_completion);
        } mutex_unlock(&ftdi->u132_lock);
}

static void ftdi_elan_abandon_targets(struct usb_ftdi *ftdi)
{
        int ed_number = 4;
        mutex_lock(&ftdi->u132_lock);
        while (ed_number-- > 0) {
                struct u132_target *target = &ftdi->target[ed_number];
                if (target->active == 1) {
                        target->condition_code = TD_DEVNOTRESP;
                        mutex_unlock(&ftdi->u132_lock);
                        ftdi_elan_do_callback(ftdi, target, NULL, 0);
                        mutex_lock(&ftdi->u132_lock);
                }
        }
        ftdi->recieved = 0;
        ftdi->expected = 4;
        ftdi->ed_found = 0;
        mutex_unlock(&ftdi->u132_lock);
}

static void ftdi_elan_flush_targets(struct usb_ftdi *ftdi)
{
        int ed_number = 4;
        mutex_lock(&ftdi->u132_lock);
        while (ed_number-- > 0) {
                struct u132_target *target = &ftdi->target[ed_number];
                target->abandoning = 1;
              wait_1:if (target->active == 1) {
                        int command_size = ftdi->command_next -
                                ftdi->command_head;
                        if (command_size < COMMAND_SIZE) {
                                struct u132_command *command = &ftdi->command[
                                        COMMAND_MASK & ftdi->command_next];
                                command->header = 0x80 | (ed_number << 5) | 0x4;
                                command->length = 0x00;
                                command->address = 0x00;
                                command->width = 0x00;
                                command->follows = 0;
                                command->value = 0;
                                command->buffer = &command->value;
                                ftdi->command_next += 1;
                                ftdi_elan_kick_command_queue(ftdi);
                        } else {
                                mutex_unlock(&ftdi->u132_lock);
                                msleep(100);
                                mutex_lock(&ftdi->u132_lock);
                                goto wait_1;
                        }
                }
              wait_2:if (target->active == 1) {
                        int command_size = ftdi->command_next -
                                ftdi->command_head;
                        if (command_size < COMMAND_SIZE) {
                                struct u132_command *command = &ftdi->command[
                                        COMMAND_MASK & ftdi->command_next];
                                command->header = 0x90 | (ed_number << 5);
                                command->length = 0x00;
                                command->address = 0x00;
                                command->width = 0x00;
                                command->follows = 0;
                                command->value = 0;
                                command->buffer = &command->value;
                                ftdi->command_next += 1;
                                ftdi_elan_kick_command_queue(ftdi);
                        } else {
                                mutex_unlock(&ftdi->u132_lock);
                                msleep(100);
                                mutex_lock(&ftdi->u132_lock);
                                goto wait_2;
                        }
                }
        }
        ftdi->recieved = 0;
        ftdi->expected = 4;
        ftdi->ed_found = 0;
        mutex_unlock(&ftdi->u132_lock);
}

static void ftdi_elan_cancel_targets(struct usb_ftdi *ftdi)
{
        int ed_number = 4;
        mutex_lock(&ftdi->u132_lock);
        while (ed_number-- > 0) {
                struct u132_target *target = &ftdi->target[ed_number];
                target->abandoning = 1;
              wait:if (target->active == 1) {
                        int command_size = ftdi->command_next -
                                ftdi->command_head;
                        if (command_size < COMMAND_SIZE) {
                                struct u132_command *command = &ftdi->command[
                                        COMMAND_MASK & ftdi->command_next];
                                command->header = 0x80 | (ed_number << 5) | 0x4;
                                command->length = 0x00;
                                command->address = 0x00;
                                command->width = 0x00;
                                command->follows = 0;
                                command->value = 0;
                                command->buffer = &command->value;
                                ftdi->command_next += 1;
                                ftdi_elan_kick_command_queue(ftdi);
                        } else {
                                mutex_unlock(&ftdi->u132_lock);
                                msleep(100);
                                mutex_lock(&ftdi->u132_lock);
                                goto wait;
                        }
                }
        }
        ftdi->recieved = 0;
        ftdi->expected = 4;
        ftdi->ed_found = 0;
        mutex_unlock(&ftdi->u132_lock);
}

static void ftdi_elan_kick_command_queue(struct usb_ftdi *ftdi)
{
        ftdi_command_queue_work(ftdi, 0);
        return;
}

static void ftdi_elan_command_work(struct work_struct *work)
{
        struct usb_ftdi *ftdi =
		container_of(work, struct usb_ftdi, command_work.work);

        if (ftdi->disconnected > 0) {
                ftdi_elan_put_kref(ftdi);
                return;
        } else {
                int retval = ftdi_elan_command_engine(ftdi);
                if (retval == -ESHUTDOWN) {
                        ftdi->disconnected += 1;
                } else if (retval == -ENODEV) {
                        ftdi->disconnected += 1;
                } else if (retval)
                        dev_err(&ftdi->udev->dev, "command error %d\n", retval);
                ftdi_command_requeue_work(ftdi, msecs_to_jiffies(10));
                return;
        }
}

static void ftdi_elan_kick_respond_queue(struct usb_ftdi *ftdi)
{
        ftdi_respond_queue_work(ftdi, 0);
        return;
}

static void ftdi_elan_respond_work(struct work_struct *work)
{
        struct usb_ftdi *ftdi =
		container_of(work, struct usb_ftdi, respond_work.work);
        if (ftdi->disconnected > 0) {
                ftdi_elan_put_kref(ftdi);
                return;
        } else {
                int retval = ftdi_elan_respond_engine(ftdi);
                if (retval == 0) {
                } else if (retval == -ESHUTDOWN) {
                        ftdi->disconnected += 1;
                } else if (retval == -ENODEV) {
                        ftdi->disconnected += 1;
                } else if (retval == -EILSEQ) {
                        ftdi->disconnected += 1;
                } else {
                        ftdi->disconnected += 1;
                        dev_err(&ftdi->udev->dev, "respond error %d\n", retval);
                }
                if (ftdi->disconnected > 0) {
                        ftdi_elan_abandon_completions(ftdi);
                        ftdi_elan_abandon_targets(ftdi);
                }
                ftdi_response_requeue_work(ftdi, msecs_to_jiffies(10));
                return;
        }
}


/*
* the sw_lock is initially held and will be freed
* after the FTDI has been synchronized
*
*/
static void ftdi_elan_status_work(struct work_struct *work)
{
        struct usb_ftdi *ftdi =
		container_of(work, struct usb_ftdi, status_work.work);
        int work_delay_in_msec = 0;
        if (ftdi->disconnected > 0) {
                ftdi_elan_put_kref(ftdi);
                return;
        } else if (ftdi->synchronized == 0) {
                down(&ftdi->sw_lock);
                if (ftdi_elan_synchronize(ftdi) == 0) {
                        ftdi->synchronized = 1;
                        ftdi_command_queue_work(ftdi, 1);
                        ftdi_respond_queue_work(ftdi, 1);
                        up(&ftdi->sw_lock);
                        work_delay_in_msec = 100;
                } else {
                        dev_err(&ftdi->udev->dev, "synchronize failed\n");
                        up(&ftdi->sw_lock);
                        work_delay_in_msec = 10 *1000;
                }
        } else if (ftdi->stuck_status > 0) {
                if (ftdi_elan_stuck_waiting(ftdi) == 0) {
                        ftdi->stuck_status = 0;
                        ftdi->synchronized = 0;
                } else if ((ftdi->stuck_status++ % 60) == 1) {
                        dev_err(&ftdi->udev->dev, "WRONG type of card inserted "
                                "- please remove\n");
                } else
                        dev_err(&ftdi->udev->dev, "WRONG type of card inserted "
                                "- checked %d times\n", ftdi->stuck_status);
                work_delay_in_msec = 100;
        } else if (ftdi->enumerated == 0) {
                if (ftdi_elan_enumeratePCI(ftdi) == 0) {
                        ftdi->enumerated = 1;
                        work_delay_in_msec = 250;
                } else
                        work_delay_in_msec = 1000;
        } else if (ftdi->initialized == 0) {
                if (ftdi_elan_setupOHCI(ftdi) == 0) {
                        ftdi->initialized = 1;
                        work_delay_in_msec = 500;
                } else {
                        dev_err(&ftdi->udev->dev, "initialized failed - trying "
                                "again in 10 seconds\n");
                        work_delay_in_msec = 1 *1000;
                }
        } else if (ftdi->registered == 0) {
                work_delay_in_msec = 10;
                if (ftdi_elan_hcd_init(ftdi) == 0) {
                        ftdi->registered = 1;
                } else
                        dev_err(&ftdi->udev->dev, "register failed\n");
                work_delay_in_msec = 250;
        } else {
                if (ftdi_elan_checkingPCI(ftdi) == 0) {
                        work_delay_in_msec = 250;
                } else if (ftdi->controlreg & 0x00400000) {
                        if (ftdi->gone_away > 0) {
                                dev_err(&ftdi->udev->dev, "PCI device eject con"
                                        "firmed platform_dev.dev.parent=%p plat"
                                        "form_dev.dev=%p\n",
                                        ftdi->platform_dev.dev.parent,
                                        &ftdi->platform_dev.dev);
                                platform_device_unregister(&ftdi->platform_dev);
                                ftdi->platform_dev.dev.parent = NULL;
                                ftdi->registered = 0;
                                ftdi->enumerated = 0;
                                ftdi->card_ejected = 0;
                                ftdi->initialized = 0;
                                ftdi->gone_away = 0;
                        } else
                                ftdi_elan_flush_targets(ftdi);
                        work_delay_in_msec = 250;
                } else {
                        dev_err(&ftdi->udev->dev, "PCI device has disappeared\n"
                                );
                        ftdi_elan_cancel_targets(ftdi);
                        work_delay_in_msec = 500;
                        ftdi->enumerated = 0;
                        ftdi->initialized = 0;
                }
        }
        if (ftdi->disconnected > 0) {
                ftdi_elan_put_kref(ftdi);
                return;
        } else {
                ftdi_status_requeue_work(ftdi,
                        msecs_to_jiffies(work_delay_in_msec));
                return;
        }
}


/*
* file_operations for the jtag interface
*
* the usage count for the device is incremented on open()
* and decremented on release()
*/
static int ftdi_elan_open(struct inode *inode, struct file *file)
{
        int subminor = iminor(inode);
        struct usb_interface *interface = usb_find_interface(&ftdi_elan_driver,
                subminor);
        if (!interface) {
                printk(KERN_ERR "can't find device for minor %d\n", subminor);
                return -ENODEV;
        } else {
                struct usb_ftdi *ftdi = usb_get_intfdata(interface);
                if (!ftdi) {
                        return -ENODEV;
                } else {
                        if (down_interruptible(&ftdi->sw_lock)) {
                                return -EINTR;
                        } else {
                                ftdi_elan_get_kref(ftdi);
                                file->private_data = ftdi;
                                return 0;
                        }
                }
        }
}

static int ftdi_elan_release(struct inode *inode, struct file *file)
{
        struct usb_ftdi *ftdi = (struct usb_ftdi *)file->private_data;
        if (ftdi == NULL)
                return -ENODEV;
        up(&ftdi->sw_lock);        /* decrement the count on our device */
        ftdi_elan_put_kref(ftdi);
        return 0;
}


/*
*
* blocking bulk reads are used to get data from the device
*
*/
static ssize_t ftdi_elan_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *ppos)
{
        char data[30 *3 + 4];
        char *d = data;
        int m = (sizeof(data) - 1) / 3;
        int bytes_read = 0;
        int retry_on_empty = 10;
        int retry_on_timeout = 5;
        struct usb_ftdi *ftdi = (struct usb_ftdi *)file->private_data;
        if (ftdi->disconnected > 0) {
                return -ENODEV;
        }
        data[0] = 0;
      have:if (ftdi->bulk_in_left > 0) {
                if (count-- > 0) {
                        char *p = ++ftdi->bulk_in_last + ftdi->bulk_in_buffer;
                        ftdi->bulk_in_left -= 1;
                        if (bytes_read < m) {
                                d += sprintf(d, " %02X", 0x000000FF & *p);
                        } else if (bytes_read > m) {
                        } else
                                d += sprintf(d, " ..");
                        if (copy_to_user(buffer++, p, 1)) {
                                return -EFAULT;
                        } else {
                                bytes_read += 1;
                                goto have;
                        }
                } else
                        return bytes_read;
        }
      more:if (count > 0) {
                int packet_bytes = 0;
                int retval = usb_bulk_msg(ftdi->udev,
                        usb_rcvbulkpipe(ftdi->udev, ftdi->bulk_in_endpointAddr),
                         ftdi->bulk_in_buffer, ftdi->bulk_in_size,
                        &packet_bytes, 50);
                if (packet_bytes > 2) {
                        ftdi->bulk_in_left = packet_bytes - 2;
                        ftdi->bulk_in_last = 1;
                        goto have;
                } else if (retval == -ETIMEDOUT) {
                        if (retry_on_timeout-- > 0) {
                                goto more;
                        } else if (bytes_read > 0) {
                                return bytes_read;
                        } else
                                return retval;
                } else if (retval == 0) {
                        if (retry_on_empty-- > 0) {
                                goto more;
                        } else
                                return bytes_read;
                } else
                        return retval;
        } else
                return bytes_read;
}

static void ftdi_elan_write_bulk_callback(struct urb *urb)
{
	struct usb_ftdi *ftdi = urb->context;
	int status = urb->status;

	if (status && !(status == -ENOENT || status == -ECONNRESET ||
	    status == -ESHUTDOWN)) {
                dev_err(&ftdi->udev->dev, "urb=%p write bulk status received: %"
                        "d\n", urb, status);
        }
        usb_buffer_free(urb->dev, urb->transfer_buffer_length,
                urb->transfer_buffer, urb->transfer_dma);
}

static int fill_buffer_with_all_queued_commands(struct usb_ftdi *ftdi,
        char *buf, int command_size, int total_size)
{
        int ed_commands = 0;
        int b = 0;
        int I = command_size;
        int i = ftdi->command_head;
        while (I-- > 0) {
                struct u132_command *command = &ftdi->command[COMMAND_MASK &
                        i++];
                int F = command->follows;
                u8 *f = command->buffer;
                if (command->header & 0x80) {
                        ed_commands |= 1 << (0x3 & (command->header >> 5));
                }
                buf[b++] = command->header;
                buf[b++] = (command->length >> 0) & 0x00FF;
                buf[b++] = (command->length >> 8) & 0x00FF;
                buf[b++] = command->address;
                buf[b++] = command->width;
                while (F-- > 0) {
                        buf[b++] = *f++;
                }
        }
        return ed_commands;
}

static int ftdi_elan_total_command_size(struct usb_ftdi *ftdi, int command_size)
{
        int total_size = 0;
        int I = command_size;
        int i = ftdi->command_head;
        while (I-- > 0) {
                struct u132_command *command = &ftdi->command[COMMAND_MASK &
                        i++];
                total_size += 5 + command->follows;
        } return total_size;
}

static int ftdi_elan_command_engine(struct usb_ftdi *ftdi)
{
        int retval;
        char *buf;
        int ed_commands;
        int total_size;
        struct urb *urb;
        int command_size = ftdi->command_next - ftdi->command_head;
        if (command_size == 0)
                return 0;
        total_size = ftdi_elan_total_command_size(ftdi, command_size);
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
                dev_err(&ftdi->udev->dev, "could not get a urb to write %d comm"
                        "ands totaling %d bytes to the Uxxx\n", command_size,
                        total_size);
                return -ENOMEM;
        }
        buf = usb_buffer_alloc(ftdi->udev, total_size, GFP_KERNEL,
                &urb->transfer_dma);
        if (!buf) {
                dev_err(&ftdi->udev->dev, "could not get a buffer to write %d c"
                        "ommands totaling %d bytes to the Uxxx\n", command_size,
                         total_size);
                usb_free_urb(urb);
                return -ENOMEM;
        }
        ed_commands = fill_buffer_with_all_queued_commands(ftdi, buf,
                command_size, total_size);
        usb_fill_bulk_urb(urb, ftdi->udev, usb_sndbulkpipe(ftdi->udev,
                ftdi->bulk_out_endpointAddr), buf, total_size,
                ftdi_elan_write_bulk_callback, ftdi);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        if (ed_commands) {
                char diag[40 *3 + 4];
                char *d = diag;
                int m = total_size;
                u8 *c = buf;
                int s = (sizeof(diag) - 1) / 3;
                diag[0] = 0;
                while (s-- > 0 && m-- > 0) {
                        if (s > 0 || m == 0) {
                                d += sprintf(d, " %02X", *c++);
                        } else
                                d += sprintf(d, " ..");
                }
        }
        retval = usb_submit_urb(urb, GFP_KERNEL);
        if (retval) {
                dev_err(&ftdi->udev->dev, "failed %d to submit urb %p to write "
                        "%d commands totaling %d bytes to the Uxxx\n", retval,
                        urb, command_size, total_size);
                usb_buffer_free(ftdi->udev, total_size, buf, urb->transfer_dma);
                usb_free_urb(urb);
                return retval;
        }
        usb_free_urb(urb);        /* release our reference to this urb,
                the USB core will eventually free it entirely */
        ftdi->command_head += command_size;
        ftdi_elan_kick_respond_queue(ftdi);
        return 0;
}

static void ftdi_elan_do_callback(struct usb_ftdi *ftdi,
        struct u132_target *target, u8 *buffer, int length)
{
        struct urb *urb = target->urb;
        int halted = target->halted;
        int skipped = target->skipped;
        int actual = target->actual;
        int non_null = target->non_null;
        int toggle_bits = target->toggle_bits;
        int error_count = target->error_count;
        int condition_code = target->condition_code;
        int repeat_number = target->repeat_number;
        void (*callback) (void *, struct urb *, u8 *, int, int, int, int, int,
                int, int, int, int) = target->callback;
        target->active -= 1;
        target->callback = NULL;
        (*callback) (target->endp, urb, buffer, length, toggle_bits,
                error_count, condition_code, repeat_number, halted, skipped,
                actual, non_null);
}

static char *have_ed_set_response(struct usb_ftdi *ftdi,
        struct u132_target *target, u16 ed_length, int ed_number, int ed_type,
        char *b)
{
        int payload = (ed_length >> 0) & 0x07FF;
        mutex_lock(&ftdi->u132_lock);
        target->actual = 0;
        target->non_null = (ed_length >> 15) & 0x0001;
        target->repeat_number = (ed_length >> 11) & 0x000F;
        if (ed_type == 0x02) {
                if (payload == 0 || target->abandoning > 0) {
                        target->abandoning = 0;
                        mutex_unlock(&ftdi->u132_lock);
                        ftdi_elan_do_callback(ftdi, target, 4 + ftdi->response,
                                payload);
                        ftdi->recieved = 0;
                        ftdi->expected = 4;
                        ftdi->ed_found = 0;
                        return ftdi->response;
                } else {
                        ftdi->expected = 4 + payload;
                        ftdi->ed_found = 1;
                        mutex_unlock(&ftdi->u132_lock);
                        return b;
                }
        } else if (ed_type == 0x03) {
                if (payload == 0 || target->abandoning > 0) {
                        target->abandoning = 0;
                        mutex_unlock(&ftdi->u132_lock);
                        ftdi_elan_do_callback(ftdi, target, 4 + ftdi->response,
                                payload);
                        ftdi->recieved = 0;
                        ftdi->expected = 4;
                        ftdi->ed_found = 0;
                        return ftdi->response;
                } else {
                        ftdi->expected = 4 + payload;
                        ftdi->ed_found = 1;
                        mutex_unlock(&ftdi->u132_lock);
                        return b;
                }
        } else if (ed_type == 0x01) {
                target->abandoning = 0;
                mutex_unlock(&ftdi->u132_lock);
                ftdi_elan_do_callback(ftdi, target, 4 + ftdi->response,
                        payload);
                ftdi->recieved = 0;
                ftdi->expected = 4;
                ftdi->ed_found = 0;
                return ftdi->response;
        } else {
                target->abandoning = 0;
                mutex_unlock(&ftdi->u132_lock);
                ftdi_elan_do_callback(ftdi, target, 4 + ftdi->response,
                        payload);
                ftdi->recieved = 0;
                ftdi->expected = 4;
                ftdi->ed_found = 0;
                return ftdi->response;
        }
}

static char *have_ed_get_response(struct usb_ftdi *ftdi,
        struct u132_target *target, u16 ed_length, int ed_number, int ed_type,
        char *b)
{
        mutex_lock(&ftdi->u132_lock);
        target->condition_code = TD_DEVNOTRESP;
        target->actual = (ed_length >> 0) & 0x01FF;
        target->non_null = (ed_length >> 15) & 0x0001;
        target->repeat_number = (ed_length >> 11) & 0x000F;
        mutex_unlock(&ftdi->u132_lock);
        if (target->active)
                ftdi_elan_do_callback(ftdi, target, NULL, 0);
        target->abandoning = 0;
        ftdi->recieved = 0;
        ftdi->expected = 4;
        ftdi->ed_found = 0;
        return ftdi->response;
}


/*
* The engine tries to empty the FTDI fifo
*
* all responses found in the fifo data are dispatched thus
* the response buffer can only ever hold a maximum sized
* response from the Uxxx.
*
*/
static int ftdi_elan_respond_engine(struct usb_ftdi *ftdi)
{
        u8 *b = ftdi->response + ftdi->recieved;
        int bytes_read = 0;
        int retry_on_empty = 1;
        int retry_on_timeout = 3;
        int empty_packets = 0;
      read:{
                int packet_bytes = 0;
                int retval = usb_bulk_msg(ftdi->udev,
                        usb_rcvbulkpipe(ftdi->udev, ftdi->bulk_in_endpointAddr),
                         ftdi->bulk_in_buffer, ftdi->bulk_in_size,
                        &packet_bytes, 500);
                char diag[30 *3 + 4];
                char *d = diag;
                int m = packet_bytes;
                u8 *c = ftdi->bulk_in_buffer;
                int s = (sizeof(diag) - 1) / 3;
                diag[0] = 0;
                while (s-- > 0 && m-- > 0) {
                        if (s > 0 || m == 0) {
                                d += sprintf(d, " %02X", *c++);
                        } else
                                d += sprintf(d, " ..");
                }
                if (packet_bytes > 2) {
                        ftdi->bulk_in_left = packet_bytes - 2;
                        ftdi->bulk_in_last = 1;
                        goto have;
                } else if (retval == -ETIMEDOUT) {
                        if (retry_on_timeout-- > 0) {
                                dev_err(&ftdi->udev->dev, "TIMED OUT with packe"
                                        "t_bytes = %d with total %d bytes%s\n",
                                        packet_bytes, bytes_read, diag);
                                goto more;
                        } else if (bytes_read > 0) {
                                dev_err(&ftdi->udev->dev, "ONLY %d bytes%s\n",
                                        bytes_read, diag);
                                return -ENOMEM;
                        } else {
                                dev_err(&ftdi->udev->dev, "TIMED OUT with packe"
                                        "t_bytes = %d with total %d bytes%s\n",
                                        packet_bytes, bytes_read, diag);
                                return -ENOMEM;
                        }
                } else if (retval == -EILSEQ) {
                        dev_err(&ftdi->udev->dev, "error = %d with packet_bytes"
                                " = %d with total %d bytes%s\n", retval,
                                packet_bytes, bytes_read, diag);
                        return retval;
                } else if (retval) {
                        dev_err(&ftdi->udev->dev, "error = %d with packet_bytes"
                                " = %d with total %d bytes%s\n", retval,
                                packet_bytes, bytes_read, diag);
                        return retval;
                } else if (packet_bytes == 2) {
                        unsigned char s0 = ftdi->bulk_in_buffer[0];
                        unsigned char s1 = ftdi->bulk_in_buffer[1];
                        empty_packets += 1;
                        if (s0 == 0x31 && s1 == 0x60) {
                                if (retry_on_empty-- > 0) {
                                        goto more;
                                } else
                                        return 0;
                        } else if (s0 == 0x31 && s1 == 0x00) {
                                if (retry_on_empty-- > 0) {
                                        goto more;
                                } else
                                        return 0;
                        } else {
                                if (retry_on_empty-- > 0) {
                                        goto more;
                                } else
                                        return 0;
                        }
                } else if (packet_bytes == 1) {
                        if (retry_on_empty-- > 0) {
                                goto more;
                        } else
                                return 0;
                } else {
                        if (retry_on_empty-- > 0) {
                                goto more;
                        } else
                                return 0;
                }
        }
      more:{
                goto read;
        }
      have:if (ftdi->bulk_in_left > 0) {
                u8 c = ftdi->bulk_in_buffer[++ftdi->bulk_in_last];
                bytes_read += 1;
                ftdi->bulk_in_left -= 1;
                if (ftdi->recieved == 0 && c == 0xFF) {
                        goto have;
                } else
                        *b++ = c;
                if (++ftdi->recieved < ftdi->expected) {
                        goto have;
                } else if (ftdi->ed_found) {
                        int ed_number = (ftdi->response[0] >> 5) & 0x03;
                        u16 ed_length = (ftdi->response[2] << 8) |
                                ftdi->response[1];
                        struct u132_target *target = &ftdi->target[ed_number];
                        int payload = (ed_length >> 0) & 0x07FF;
                        char diag[30 *3 + 4];
                        char *d = diag;
                        int m = payload;
                        u8 *c = 4 + ftdi->response;
                        int s = (sizeof(diag) - 1) / 3;
                        diag[0] = 0;
                        while (s-- > 0 && m-- > 0) {
                                if (s > 0 || m == 0) {
                                        d += sprintf(d, " %02X", *c++);
                                } else
                                        d += sprintf(d, " ..");
                        }
                        ftdi_elan_do_callback(ftdi, target, 4 + ftdi->response,
                                payload);
                        ftdi->recieved = 0;
                        ftdi->expected = 4;
                        ftdi->ed_found = 0;
                        b = ftdi->response;
                        goto have;
                } else if (ftdi->expected == 8) {
                        u8 buscmd;
                        int respond_head = ftdi->respond_head++;
                        struct u132_respond *respond = &ftdi->respond[
                                RESPOND_MASK & respond_head];
                        u32 data = ftdi->response[7];
                        data <<= 8;
                        data |= ftdi->response[6];
                        data <<= 8;
                        data |= ftdi->response[5];
                        data <<= 8;
                        data |= ftdi->response[4];
                        *respond->value = data;
                        *respond->result = 0;
                        complete(&respond->wait_completion);
                        ftdi->recieved = 0;
                        ftdi->expected = 4;
                        ftdi->ed_found = 0;
                        b = ftdi->response;
                        buscmd = (ftdi->response[0] >> 0) & 0x0F;
                        if (buscmd == 0x00) {
                        } else if (buscmd == 0x02) {
                        } else if (buscmd == 0x06) {
                        } else if (buscmd == 0x0A) {
                        } else
                                dev_err(&ftdi->udev->dev, "Uxxx unknown(%0X) va"
                                        "lue = %08X\n", buscmd, data);
                        goto have;
                } else {
                        if ((ftdi->response[0] & 0x80) == 0x00) {
                                ftdi->expected = 8;
                                goto have;
                        } else {
                                int ed_number = (ftdi->response[0] >> 5) & 0x03;
                                int ed_type = (ftdi->response[0] >> 0) & 0x03;
                                u16 ed_length = (ftdi->response[2] << 8) |
                                        ftdi->response[1];
                                struct u132_target *target = &ftdi->target[
                                        ed_number];
                                target->halted = (ftdi->response[0] >> 3) &
                                        0x01;
                                target->skipped = (ftdi->response[0] >> 2) &
                                        0x01;
                                target->toggle_bits = (ftdi->response[3] >> 6)
                                        & 0x03;
                                target->error_count = (ftdi->response[3] >> 4)
                                        & 0x03;
                                target->condition_code = (ftdi->response[
                                        3] >> 0) & 0x0F;
                                if ((ftdi->response[0] & 0x10) == 0x00) {
                                        b = have_ed_set_response(ftdi, target,
                                                ed_length, ed_number, ed_type,
                                                b);
                                        goto have;
                                } else {
                                        b = have_ed_get_response(ftdi, target,
                                                ed_length, ed_number, ed_type,
                                                b);
                                        goto have;
                                }
                        }
                }
        } else
                goto more;
}


/*
* create a urb, and a buffer for it, and copy the data to the urb
*
*/
static ssize_t ftdi_elan_write(struct file *file,
			       const char __user *user_buffer, size_t count,
			       loff_t *ppos)
{
        int retval = 0;
        struct urb *urb;
        char *buf;
        struct usb_ftdi *ftdi = file->private_data;

        if (ftdi->disconnected > 0) {
                return -ENODEV;
        }
        if (count == 0) {
                goto exit;
        }
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
                retval = -ENOMEM;
                goto error_1;
        }
        buf = usb_buffer_alloc(ftdi->udev, count, GFP_KERNEL,
                &urb->transfer_dma);
        if (!buf) {
                retval = -ENOMEM;
                goto error_2;
        }
        if (copy_from_user(buf, user_buffer, count)) {
                retval = -EFAULT;
                goto error_3;
        }
        usb_fill_bulk_urb(urb, ftdi->udev, usb_sndbulkpipe(ftdi->udev,
                ftdi->bulk_out_endpointAddr), buf, count,
                ftdi_elan_write_bulk_callback, ftdi);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        retval = usb_submit_urb(urb, GFP_KERNEL);
        if (retval) {
                dev_err(&ftdi->udev->dev, "failed submitting write urb, error %"
                        "d\n", retval);
                goto error_3;
        }
        usb_free_urb(urb);

exit:
        return count;
error_3:
	usb_buffer_free(ftdi->udev, count, buf, urb->transfer_dma);
error_2:
	usb_free_urb(urb);
error_1:
	return retval;
}

static const struct file_operations ftdi_elan_fops = {
        .owner = THIS_MODULE,
        .llseek = no_llseek,
        .read = ftdi_elan_read,
        .write = ftdi_elan_write,
        .open = ftdi_elan_open,
        .release = ftdi_elan_release,
};

/*
* usb class driver info in order to get a minor number from the usb core,
* and to have the device registered with the driver core
*/
static struct usb_class_driver ftdi_elan_jtag_class = {
        .name = "ftdi-%d-jtag",
        .fops = &ftdi_elan_fops,
        .minor_base = USB_FTDI_ELAN_MINOR_BASE,
};

/*
* the following definitions are for the
* ELAN FPGA state machgine processor that
* lies on the other side of the FTDI chip
*/
#define cPCIu132rd 0x0
#define cPCIu132wr 0x1
#define cPCIiord 0x2
#define cPCIiowr 0x3
#define cPCImemrd 0x6
#define cPCImemwr 0x7
#define cPCIcfgrd 0xA
#define cPCIcfgwr 0xB
#define cPCInull 0xF
#define cU132cmd_status 0x0
#define cU132flash 0x1
#define cPIDsetup 0x0
#define cPIDout 0x1
#define cPIDin 0x2
#define cPIDinonce 0x3
#define cCCnoerror 0x0
#define cCCcrc 0x1
#define cCCbitstuff 0x2
#define cCCtoggle 0x3
#define cCCstall 0x4
#define cCCnoresp 0x5
#define cCCbadpid1 0x6
#define cCCbadpid2 0x7
#define cCCdataoverrun 0x8
#define cCCdataunderrun 0x9
#define cCCbuffoverrun 0xC
#define cCCbuffunderrun 0xD
#define cCCnotaccessed 0xF
static int ftdi_elan_write_reg(struct usb_ftdi *ftdi, u32 data)
{
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x00 | cPCIu132wr;
                        command->length = 0x04;
                        command->address = 0x00;
                        command->width = 0x00;
                        command->follows = 4;
                        command->value = data;
                        command->buffer = &command->value;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

static int ftdi_elan_write_config(struct usb_ftdi *ftdi, int config_offset,
        u8 width, u32 data)
{
        u8 addressofs = config_offset / 4;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x00 | (cPCIcfgwr & 0x0F);
                        command->length = 0x04;
                        command->address = addressofs;
                        command->width = 0x00 | (width & 0x0F);
                        command->follows = 4;
                        command->value = data;
                        command->buffer = &command->value;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

static int ftdi_elan_write_pcimem(struct usb_ftdi *ftdi, int mem_offset,
        u8 width, u32 data)
{
        u8 addressofs = mem_offset / 4;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x00 | (cPCImemwr & 0x0F);
                        command->length = 0x04;
                        command->address = addressofs;
                        command->width = 0x00 | (width & 0x0F);
                        command->follows = 4;
                        command->value = data;
                        command->buffer = &command->value;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_write_pcimem(struct platform_device *pdev, int mem_offset,
        u8 width, u32 data)
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_write_pcimem(ftdi, mem_offset, width, data);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_write_pcimem);
static int ftdi_elan_read_reg(struct usb_ftdi *ftdi, u32 *data)
{
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                int respond_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                respond_size = ftdi->respond_next - ftdi->respond_head;
                if (command_size < COMMAND_SIZE && respond_size < RESPOND_SIZE)
                        {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        struct u132_respond *respond = &ftdi->respond[
                                RESPOND_MASK & ftdi->respond_next];
                        int result = -ENODEV;
                        respond->result = &result;
                        respond->header = command->header = 0x00 | cPCIu132rd;
                        command->length = 0x04;
                        respond->address = command->address = cU132cmd_status;
                        command->width = 0x00;
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        respond->value = data;
                        init_completion(&respond->wait_completion);
                        ftdi->command_next += 1;
                        ftdi->respond_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        wait_for_completion(&respond->wait_completion);
                        return result;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

static int ftdi_elan_read_config(struct usb_ftdi *ftdi, int config_offset,
        u8 width, u32 *data)
{
        u8 addressofs = config_offset / 4;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                int respond_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                respond_size = ftdi->respond_next - ftdi->respond_head;
                if (command_size < COMMAND_SIZE && respond_size < RESPOND_SIZE)
                        {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        struct u132_respond *respond = &ftdi->respond[
                                RESPOND_MASK & ftdi->respond_next];
                        int result = -ENODEV;
                        respond->result = &result;
                        respond->header = command->header = 0x00 | (cPCIcfgrd &
                                0x0F);
                        command->length = 0x04;
                        respond->address = command->address = addressofs;
                        command->width = 0x00 | (width & 0x0F);
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        respond->value = data;
                        init_completion(&respond->wait_completion);
                        ftdi->command_next += 1;
                        ftdi->respond_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        wait_for_completion(&respond->wait_completion);
                        return result;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

static int ftdi_elan_read_pcimem(struct usb_ftdi *ftdi, int mem_offset,
        u8 width, u32 *data)
{
        u8 addressofs = mem_offset / 4;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else {
                int command_size;
                int respond_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                respond_size = ftdi->respond_next - ftdi->respond_head;
                if (command_size < COMMAND_SIZE && respond_size < RESPOND_SIZE)
                        {
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        struct u132_respond *respond = &ftdi->respond[
                                RESPOND_MASK & ftdi->respond_next];
                        int result = -ENODEV;
                        respond->result = &result;
                        respond->header = command->header = 0x00 | (cPCImemrd &
                                0x0F);
                        command->length = 0x04;
                        respond->address = command->address = addressofs;
                        command->width = 0x00 | (width & 0x0F);
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        respond->value = data;
                        init_completion(&respond->wait_completion);
                        ftdi->command_next += 1;
                        ftdi->respond_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        wait_for_completion(&respond->wait_completion);
                        return result;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_read_pcimem(struct platform_device *pdev, int mem_offset,
        u8 width, u32 *data)
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        if (ftdi->initialized == 0) {
                return -ENODEV;
        } else
                return ftdi_elan_read_pcimem(ftdi, mem_offset, width, data);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_read_pcimem);
static int ftdi_elan_edset_setup(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        u8 ed = ed_number - 1;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_target *target = &ftdi->target[ed];
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x80 | (ed << 5);
                        command->length = 0x8007;
                        command->address = (toggle_bits << 6) | (ep_number << 2)
                                | (address << 0);
                        command->width = usb_maxpacket(urb->dev, urb->pipe,
                                usb_pipeout(urb->pipe));
                        command->follows = 8;
                        command->value = 0;
                        command->buffer = urb->setup_packet;
                        target->callback = callback;
                        target->endp = endp;
                        target->urb = urb;
                        target->active = 1;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_edset_setup(struct platform_device *pdev, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_setup(ftdi, ed_number, endp, urb, address,
                ep_number, toggle_bits, callback);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_setup);
static int ftdi_elan_edset_input(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        u8 ed = ed_number - 1;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_target *target = &ftdi->target[ed];
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        u32 remaining_length = urb->transfer_buffer_length -
                                urb->actual_length;
                        command->header = 0x82 | (ed << 5);
                        if (remaining_length == 0) {
                                command->length = 0x0000;
                        } else if (remaining_length > 1024) {
                                command->length = 0x8000 | 1023;
                        } else
                                command->length = 0x8000 | (remaining_length -
                                        1);
                        command->address = (toggle_bits << 6) | (ep_number << 2)
                                | (address << 0);
                        command->width = usb_maxpacket(urb->dev, urb->pipe,
                                usb_pipeout(urb->pipe));
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        target->callback = callback;
                        target->endp = endp;
                        target->urb = urb;
                        target->active = 1;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_edset_input(struct platform_device *pdev, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_input(ftdi, ed_number, endp, urb, address,
                ep_number, toggle_bits, callback);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_input);
static int ftdi_elan_edset_empty(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        u8 ed = ed_number - 1;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        struct u132_target *target = &ftdi->target[ed];
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x81 | (ed << 5);
                        command->length = 0x0000;
                        command->address = (toggle_bits << 6) | (ep_number << 2)
                                | (address << 0);
                        command->width = usb_maxpacket(urb->dev, urb->pipe,
                                usb_pipeout(urb->pipe));
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        target->callback = callback;
                        target->endp = endp;
                        target->urb = urb;
                        target->active = 1;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_edset_empty(struct platform_device *pdev, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_empty(ftdi, ed_number, endp, urb, address,
                ep_number, toggle_bits, callback);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_empty);
static int ftdi_elan_edset_output(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        u8 ed = ed_number - 1;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        u8 *b;
                        u16 urb_size;
                        int i = 0;
                        char data[30 *3 + 4];
                        char *d = data;
                        int m = (sizeof(data) - 1) / 3;
                        int l = 0;
                        struct u132_target *target = &ftdi->target[ed];
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x81 | (ed << 5);
                        command->address = (toggle_bits << 6) | (ep_number << 2)
                                | (address << 0);
                        command->width = usb_maxpacket(urb->dev, urb->pipe,
                                usb_pipeout(urb->pipe));
                        command->follows = min_t(u32, 1024,
                                urb->transfer_buffer_length -
                                urb->actual_length);
                        command->value = 0;
                        command->buffer = urb->transfer_buffer +
                                urb->actual_length;
                        command->length = 0x8000 | (command->follows - 1);
                        b = command->buffer;
                        urb_size = command->follows;
                        data[0] = 0;
                        while (urb_size-- > 0) {
                                if (i > m) {
                                } else if (i++ < m) {
                                        int w = sprintf(d, " %02X", *b++);
                                        d += w;
                                        l += w;
                                } else
                                        d += sprintf(d, " ..");
                        }
                        target->callback = callback;
                        target->endp = endp;
                        target->urb = urb;
                        target->active = 1;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_edset_output(struct platform_device *pdev, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_output(ftdi, ed_number, endp, urb, address,
                ep_number, toggle_bits, callback);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_output);
static int ftdi_elan_edset_single(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        u8 ed = ed_number - 1;
      wait:if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                int command_size;
                mutex_lock(&ftdi->u132_lock);
                command_size = ftdi->command_next - ftdi->command_head;
                if (command_size < COMMAND_SIZE) {
                        u32 remaining_length = urb->transfer_buffer_length -
                                urb->actual_length;
                        struct u132_target *target = &ftdi->target[ed];
                        struct u132_command *command = &ftdi->command[
                                COMMAND_MASK & ftdi->command_next];
                        command->header = 0x83 | (ed << 5);
                        if (remaining_length == 0) {
                                command->length = 0x0000;
                        } else if (remaining_length > 1024) {
                                command->length = 0x8000 | 1023;
                        } else
                                command->length = 0x8000 | (remaining_length -
                                        1);
                        command->address = (toggle_bits << 6) | (ep_number << 2)
                                | (address << 0);
                        command->width = usb_maxpacket(urb->dev, urb->pipe,
                                usb_pipeout(urb->pipe));
                        command->follows = 0;
                        command->value = 0;
                        command->buffer = NULL;
                        target->callback = callback;
                        target->endp = endp;
                        target->urb = urb;
                        target->active = 1;
                        ftdi->command_next += 1;
                        ftdi_elan_kick_command_queue(ftdi);
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        mutex_unlock(&ftdi->u132_lock);
                        msleep(100);
                        goto wait;
                }
        }
}

int usb_ftdi_elan_edset_single(struct platform_device *pdev, u8 ed_number,
        void *endp, struct urb *urb, u8 address, u8 ep_number, u8 toggle_bits,
        void (*callback) (void *endp, struct urb *urb, u8 *buf, int len,
        int toggle_bits, int error_count, int condition_code, int repeat_number,
         int halted, int skipped, int actual, int non_null))
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_single(ftdi, ed_number, endp, urb, address,
                ep_number, toggle_bits, callback);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_single);
static int ftdi_elan_edset_flush(struct usb_ftdi *ftdi, u8 ed_number,
        void *endp)
{
        u8 ed = ed_number - 1;
        if (ftdi->disconnected > 0) {
                return -ENODEV;
        } else if (ftdi->initialized == 0) {
                return -ENODEV;
        } else {
                struct u132_target *target = &ftdi->target[ed];
                mutex_lock(&ftdi->u132_lock);
                if (target->abandoning > 0) {
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                } else {
                        target->abandoning = 1;
                      wait_1:if (target->active == 1) {
                                int command_size = ftdi->command_next -
                                        ftdi->command_head;
                                if (command_size < COMMAND_SIZE) {
                                        struct u132_command *command =
                                                &ftdi->command[COMMAND_MASK &
                                                ftdi->command_next];
                                        command->header = 0x80 | (ed << 5) |
                                                0x4;
                                        command->length = 0x00;
                                        command->address = 0x00;
                                        command->width = 0x00;
                                        command->follows = 0;
                                        command->value = 0;
                                        command->buffer = &command->value;
                                        ftdi->command_next += 1;
                                        ftdi_elan_kick_command_queue(ftdi);
                                } else {
                                        mutex_unlock(&ftdi->u132_lock);
                                        msleep(100);
                                        mutex_lock(&ftdi->u132_lock);
                                        goto wait_1;
                                }
                        }
                        mutex_unlock(&ftdi->u132_lock);
                        return 0;
                }
        }
}

int usb_ftdi_elan_edset_flush(struct platform_device *pdev, u8 ed_number,
        void *endp)
{
        struct usb_ftdi *ftdi = platform_device_to_usb_ftdi(pdev);
        return ftdi_elan_edset_flush(ftdi, ed_number, endp);
}


EXPORT_SYMBOL_GPL(usb_ftdi_elan_edset_flush);
static int ftdi_elan_flush_input_fifo(struct usb_ftdi *ftdi)
{
        int retry_on_empty = 10;
        int retry_on_timeout = 5;
        int retry_on_status = 20;
      more:{
                int packet_bytes = 0;
                int retval = usb_bulk_msg(ftdi->udev,
                        usb_rcvbulkpipe(ftdi->udev, ftdi->bulk_in_endpointAddr),
                         ftdi->bulk_in_buffer, ftdi->bulk_in_size,
                        &packet_bytes, 100);
                if (packet_bytes > 2) {
                        char diag[30 *3 + 4];
                        char *d = diag;
                        int m = (sizeof(diag) - 1) / 3;
                        char *b = ftdi->bulk_in_buffer;
                        int bytes_read = 0;
                        diag[0] = 0;
                        while (packet_bytes-- > 0) {
                                char c = *b++;
                                if (bytes_read < m) {
                                        d += sprintf(d, " %02X",
                                                0x000000FF & c);
                                } else if (bytes_read > m) {
                                } else
                                        d += sprintf(d, " ..");
                                bytes_read += 1;
                                continue;
                        }
                        goto more;
                } else if (packet_bytes > 1) {
                        char s1 = ftdi->bulk_in_buffer[0];
                        char s2 = ftdi->bulk_in_buffer[1];
                        if (s1 == 0x31 && s2 == 0x60) {
                                return 0;
                        } else if (retry_on_status-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "STATUS ERROR retry l"
                                        "imit reached\n");
                                return -EFAULT;
                        }
                } else if (packet_bytes > 0) {
                        char b1 = ftdi->bulk_in_buffer[0];
                        dev_err(&ftdi->udev->dev, "only one byte flushed from F"
                                "TDI = %02X\n", b1);
                        if (retry_on_status-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "STATUS ERROR retry l"
                                        "imit reached\n");
                                return -EFAULT;
                        }
                } else if (retval == -ETIMEDOUT) {
                        if (retry_on_timeout-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "TIMED OUT retry limi"
                                        "t reached\n");
                                return -ENOMEM;
                        }
                } else if (retval == 0) {
                        if (retry_on_empty-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "empty packet retry l"
                                        "imit reached\n");
                                return -ENOMEM;
                        }
                } else {
                        dev_err(&ftdi->udev->dev, "error = %d\n", retval);
                        return retval;
                }
        }
        return -1;
}


/*
* send the long flush sequence
*
*/
static int ftdi_elan_synchronize_flush(struct usb_ftdi *ftdi)
{
        int retval;
        struct urb *urb;
        char *buf;
        int I = 257;
        int i = 0;
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
                dev_err(&ftdi->udev->dev, "could not alloc a urb for flush sequ"
                        "ence\n");
                return -ENOMEM;
        }
        buf = usb_buffer_alloc(ftdi->udev, I, GFP_KERNEL, &urb->transfer_dma);
        if (!buf) {
                dev_err(&ftdi->udev->dev, "could not get a buffer for flush seq"
                        "uence\n");
                usb_free_urb(urb);
                return -ENOMEM;
        }
        while (I-- > 0)
                buf[i++] = 0x55;
        usb_fill_bulk_urb(urb, ftdi->udev, usb_sndbulkpipe(ftdi->udev,
                ftdi->bulk_out_endpointAddr), buf, i,
                ftdi_elan_write_bulk_callback, ftdi);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        retval = usb_submit_urb(urb, GFP_KERNEL);
        if (retval) {
                dev_err(&ftdi->udev->dev, "failed to submit urb containing the "
                        "flush sequence\n");
                usb_buffer_free(ftdi->udev, i, buf, urb->transfer_dma);
                usb_free_urb(urb);
                return -ENOMEM;
        }
        usb_free_urb(urb);
        return 0;
}


/*
* send the reset sequence
*
*/
static int ftdi_elan_synchronize_reset(struct usb_ftdi *ftdi)
{
        int retval;
        struct urb *urb;
        char *buf;
        int I = 4;
        int i = 0;
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
                dev_err(&ftdi->udev->dev, "could not get a urb for the reset se"
                        "quence\n");
                return -ENOMEM;
        }
        buf = usb_buffer_alloc(ftdi->udev, I, GFP_KERNEL, &urb->transfer_dma);
        if (!buf) {
                dev_err(&ftdi->udev->dev, "could not get a buffer for the reset"
                        " sequence\n");
                usb_free_urb(urb);
                return -ENOMEM;
        }
        buf[i++] = 0x55;
        buf[i++] = 0xAA;
        buf[i++] = 0x5A;
        buf[i++] = 0xA5;
        usb_fill_bulk_urb(urb, ftdi->udev, usb_sndbulkpipe(ftdi->udev,
                ftdi->bulk_out_endpointAddr), buf, i,
                ftdi_elan_write_bulk_callback, ftdi);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        retval = usb_submit_urb(urb, GFP_KERNEL);
        if (retval) {
                dev_err(&ftdi->udev->dev, "failed to submit urb containing the "
                        "reset sequence\n");
                usb_buffer_free(ftdi->udev, i, buf, urb->transfer_dma);
                usb_free_urb(urb);
                return -ENOMEM;
        }
        usb_free_urb(urb);
        return 0;
}

static int ftdi_elan_synchronize(struct usb_ftdi *ftdi)
{
        int retval;
        int long_stop = 10;
        int retry_on_timeout = 5;
        int retry_on_empty = 10;
        int err_count = 0;
        retval = ftdi_elan_flush_input_fifo(ftdi);
        if (retval)
                return retval;
        ftdi->bulk_in_left = 0;
        ftdi->bulk_in_last = -1;
        while (long_stop-- > 0) {
                int read_stop;
                int read_stuck;
                retval = ftdi_elan_synchronize_flush(ftdi);
                if (retval)
                        return retval;
                retval = ftdi_elan_flush_input_fifo(ftdi);
                if (retval)
                        return retval;
              reset:retval = ftdi_elan_synchronize_reset(ftdi);
                if (retval)
                        return retval;
                read_stop = 100;
                read_stuck = 10;
              read:{
                        int packet_bytes = 0;
                        retval = usb_bulk_msg(ftdi->udev,
                                usb_rcvbulkpipe(ftdi->udev,
                                ftdi->bulk_in_endpointAddr),
                                ftdi->bulk_in_buffer, ftdi->bulk_in_size,
                                &packet_bytes, 500);
                        if (packet_bytes > 2) {
                                char diag[30 *3 + 4];
                                char *d = diag;
                                int m = (sizeof(diag) - 1) / 3;
                                char *b = ftdi->bulk_in_buffer;
                                int bytes_read = 0;
                                unsigned char c = 0;
                                diag[0] = 0;
                                while (packet_bytes-- > 0) {
                                        c = *b++;
                                        if (bytes_read < m) {
                                                d += sprintf(d, " %02X", c);
                                        } else if (bytes_read > m) {
                                        } else
                                                d += sprintf(d, " ..");
                                        bytes_read += 1;
                                        continue;
                                }
                                if (c == 0x7E) {
                                        return 0;
                                } else {
                                        if (c == 0x55) {
                                                goto read;
                                        } else if (read_stop-- > 0) {
                                                goto read;
                                        } else {
                                                dev_err(&ftdi->udev->dev, "retr"
                                                        "y limit reached\n");
                                                continue;
                                        }
                                }
                        } else if (packet_bytes > 1) {
                                unsigned char s1 = ftdi->bulk_in_buffer[0];
                                unsigned char s2 = ftdi->bulk_in_buffer[1];
                                if (s1 == 0x31 && s2 == 0x00) {
                                        if (read_stuck-- > 0) {
                                                goto read;
                                        } else
                                                goto reset;
                                } else if (s1 == 0x31 && s2 == 0x60) {
                                        if (read_stop-- > 0) {
                                                goto read;
                                        } else {
                                                dev_err(&ftdi->udev->dev, "retr"
                                                        "y limit reached\n");
                                                continue;
                                        }
                                } else {
                                        if (read_stop-- > 0) {
                                                goto read;
                                        } else {
                                                dev_err(&ftdi->udev->dev, "retr"
                                                        "y limit reached\n");
                                                continue;
                                        }
                                }
                        } else if (packet_bytes > 0) {
                                if (read_stop-- > 0) {
                                        goto read;
                                } else {
                                        dev_err(&ftdi->udev->dev, "retry limit "
                                                "reached\n");
                                        continue;
                                }
                        } else if (retval == -ETIMEDOUT) {
                                if (retry_on_timeout-- > 0) {
                                        goto read;
                                } else {
                                        dev_err(&ftdi->udev->dev, "TIMED OUT re"
                                                "try limit reached\n");
                                        continue;
                                }
                        } else if (retval == 0) {
                                if (retry_on_empty-- > 0) {
                                        goto read;
                                } else {
                                        dev_err(&ftdi->udev->dev, "empty packet"
                                                " retry limit reached\n");
                                        continue;
                                }
                        } else {
                                err_count += 1;
                                dev_err(&ftdi->udev->dev, "error = %d\n",
                                        retval);
                                if (read_stop-- > 0) {
                                        goto read;
                                } else {
                                        dev_err(&ftdi->udev->dev, "retry limit "
                                                "reached\n");
                                        continue;
                                }
                        }
                }
        }
        dev_err(&ftdi->udev->dev, "failed to synchronize\n");
        return -EFAULT;
}

static int ftdi_elan_stuck_waiting(struct usb_ftdi *ftdi)
{
        int retry_on_empty = 10;
        int retry_on_timeout = 5;
        int retry_on_status = 50;
      more:{
                int packet_bytes = 0;
                int retval = usb_bulk_msg(ftdi->udev,
                        usb_rcvbulkpipe(ftdi->udev, ftdi->bulk_in_endpointAddr),
                         ftdi->bulk_in_buffer, ftdi->bulk_in_size,
                        &packet_bytes, 1000);
                if (packet_bytes > 2) {
                        char diag[30 *3 + 4];
                        char *d = diag;
                        int m = (sizeof(diag) - 1) / 3;
                        char *b = ftdi->bulk_in_buffer;
                        int bytes_read = 0;
                        diag[0] = 0;
                        while (packet_bytes-- > 0) {
                                char c = *b++;
                                if (bytes_read < m) {
                                        d += sprintf(d, " %02X",
                                                0x000000FF & c);
                                } else if (bytes_read > m) {
                                } else
                                        d += sprintf(d, " ..");
                                bytes_read += 1;
                                continue;
                        }
                        goto more;
                } else if (packet_bytes > 1) {
                        char s1 = ftdi->bulk_in_buffer[0];
                        char s2 = ftdi->bulk_in_buffer[1];
                        if (s1 == 0x31 && s2 == 0x60) {
                                return 0;
                        } else if (retry_on_status-- > 0) {
                                msleep(5);
                                goto more;
                        } else
                                return -EFAULT;
                } else if (packet_bytes > 0) {
                        char b1 = ftdi->bulk_in_buffer[0];
                        dev_err(&ftdi->udev->dev, "only one byte flushed from F"
                                "TDI = %02X\n", b1);
                        if (retry_on_status-- > 0) {
                                msleep(5);
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "STATUS ERROR retry l"
                                        "imit reached\n");
                                return -EFAULT;
                        }
                } else if (retval == -ETIMEDOUT) {
                        if (retry_on_timeout-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "TIMED OUT retry limi"
                                        "t reached\n");
                                return -ENOMEM;
                        }
                } else if (retval == 0) {
                        if (retry_on_empty-- > 0) {
                                goto more;
                        } else {
                                dev_err(&ftdi->udev->dev, "empty packet retry l"
                                        "imit reached\n");
                                return -ENOMEM;
                        }
                } else {
                        dev_err(&ftdi->udev->dev, "error = %d\n", retval);
                        return -ENOMEM;
                }
        }
        return -1;
}

static int ftdi_elan_checkingPCI(struct usb_ftdi *ftdi)
{
        int UxxxStatus = ftdi_elan_read_reg(ftdi, &ftdi->controlreg);
        if (UxxxStatus)
                return UxxxStatus;
        if (ftdi->controlreg & 0x00400000) {
                if (ftdi->card_ejected) {
                } else {
                        ftdi->card_ejected = 1;
                        dev_err(&ftdi->udev->dev, "CARD EJECTED - controlreg = "
                                "%08X\n", ftdi->controlreg);
                }
                return -ENODEV;
        } else {
                u8 fn = ftdi->function - 1;
                int activePCIfn = fn << 8;
                u32 pcidata;
                u32 pciVID;
                u32 pciPID;
                int reg = 0;
                UxxxStatus = ftdi_elan_read_config(ftdi, activePCIfn | reg, 0,
                        &pcidata);
                if (UxxxStatus)
                        return UxxxStatus;
                pciVID = pcidata & 0xFFFF;
                pciPID = (pcidata >> 16) & 0xFFFF;
                if (pciVID == ftdi->platform_data.vendor && pciPID ==
                        ftdi->platform_data.device) {
                        return 0;
                } else {
                        dev_err(&ftdi->udev->dev, "vendor=%04X pciVID=%04X devi"
                                "ce=%04X pciPID=%04X\n",
                                ftdi->platform_data.vendor, pciVID,
                                ftdi->platform_data.device, pciPID);
                        return -ENODEV;
                }
        }
}


#define ftdi_read_pcimem(ftdi, member, data) ftdi_elan_read_pcimem(ftdi, \
        offsetof(struct ohci_regs, member), 0, data);
#define ftdi_write_pcimem(ftdi, member, data) ftdi_elan_write_pcimem(ftdi, \
        offsetof(struct ohci_regs, member), 0, data);

#define OHCI_CONTROL_INIT OHCI_CTRL_CBSR
#define OHCI_INTR_INIT (OHCI_INTR_MIE | OHCI_INTR_UE | OHCI_INTR_RD | \
        OHCI_INTR_WDH)
static int ftdi_elan_check_controller(struct usb_ftdi *ftdi, int quirk)
{
        int devices = 0;
        int retval;
        u32 hc_control;
        int num_ports;
        u32 control;
        u32 rh_a = -1;
        u32 status;
        u32 fminterval;
        u32 hc_fminterval;
        u32 periodicstart;
        u32 cmdstatus;
        u32 roothub_a;
        int mask = OHCI_INTR_INIT;
        int sleep_time = 0;
        int reset_timeout = 30;        /* ... allow extra time */
        int temp;
        retval = ftdi_write_pcimem(ftdi, intrdisable, OHCI_INTR_MIE);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, control, &control);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, roothub.a, &rh_a);
        if (retval)
                return retval;
        num_ports = rh_a & RH_A_NDP;
        retval = ftdi_read_pcimem(ftdi, fminterval, &hc_fminterval);
        if (retval)
                return retval;
        hc_fminterval &= 0x3fff;
        if (hc_fminterval != FI) {
        }
        hc_fminterval |= FSMP(hc_fminterval) << 16;
        retval = ftdi_read_pcimem(ftdi, control, &hc_control);
        if (retval)
                return retval;
        switch (hc_control & OHCI_CTRL_HCFS) {
        case OHCI_USB_OPER:
                sleep_time = 0;
                break;
        case OHCI_USB_SUSPEND:
        case OHCI_USB_RESUME:
                hc_control &= OHCI_CTRL_RWC;
                hc_control |= OHCI_USB_RESUME;
                sleep_time = 10;
                break;
        default:
                hc_control &= OHCI_CTRL_RWC;
                hc_control |= OHCI_USB_RESET;
                sleep_time = 50;
                break;
        }
        retval = ftdi_write_pcimem(ftdi, control, hc_control);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, control, &control);
        if (retval)
                return retval;
        msleep(sleep_time);
        retval = ftdi_read_pcimem(ftdi, roothub.a, &roothub_a);
        if (retval)
                return retval;
        if (!(roothub_a & RH_A_NPS)) {        /* power down each port */
                for (temp = 0; temp < num_ports; temp++) {
                        retval = ftdi_write_pcimem(ftdi,
                                roothub.portstatus[temp], RH_PS_LSDA);
                        if (retval)
                                return retval;
                }
        }
        retval = ftdi_read_pcimem(ftdi, control, &control);
        if (retval)
                return retval;
      retry:retval = ftdi_read_pcimem(ftdi, cmdstatus, &status);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, cmdstatus, OHCI_HCR);
        if (retval)
                return retval;
      extra:{
                retval = ftdi_read_pcimem(ftdi, cmdstatus, &status);
                if (retval)
                        return retval;
                if (0 != (status & OHCI_HCR)) {
                        if (--reset_timeout == 0) {
                                dev_err(&ftdi->udev->dev, "USB HC reset timed o"
                                        "ut!\n");
                                return -ENODEV;
                        } else {
                                msleep(5);
                                goto extra;
                        }
                }
        }
        if (quirk & OHCI_QUIRK_INITRESET) {
                retval = ftdi_write_pcimem(ftdi, control, hc_control);
                if (retval)
                        return retval;
                retval = ftdi_read_pcimem(ftdi, control, &control);
                if (retval)
                        return retval;
        }
        retval = ftdi_write_pcimem(ftdi, ed_controlhead, 0x00000000);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, ed_bulkhead, 0x11000000);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, hcca, 0x00000000);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, fminterval, &fminterval);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, fminterval,
                ((fminterval & FIT) ^ FIT) | hc_fminterval);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, periodicstart,
                ((9 *hc_fminterval) / 10) & 0x3fff);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, fminterval, &fminterval);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, periodicstart, &periodicstart);
        if (retval)
                return retval;
        if (0 == (fminterval & 0x3fff0000) || 0 == periodicstart) {
                if (!(quirk & OHCI_QUIRK_INITRESET)) {
                        quirk |= OHCI_QUIRK_INITRESET;
                        goto retry;
                } else
                        dev_err(&ftdi->udev->dev, "init err(%08x %04x)\n",
                                fminterval, periodicstart);
        }                        /* start controller operations */
        hc_control &= OHCI_CTRL_RWC;
        hc_control |= OHCI_CONTROL_INIT | OHCI_CTRL_BLE | OHCI_USB_OPER;
        retval = ftdi_write_pcimem(ftdi, control, hc_control);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, cmdstatus, OHCI_BLF);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, cmdstatus, &cmdstatus);
        if (retval)
                return retval;
        retval = ftdi_read_pcimem(ftdi, control, &control);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, roothub.status, RH_HS_DRWE);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, intrstatus, mask);
        if (retval)
                return retval;
        retval = ftdi_write_pcimem(ftdi, intrdisable,
                OHCI_INTR_MIE | OHCI_INTR_OC | OHCI_INTR_RHSC | OHCI_INTR_FNO |
                OHCI_INTR_UE | OHCI_INTR_RD | OHCI_INTR_SF | OHCI_INTR_WDH |
                OHCI_INTR_SO);
        if (retval)
                return retval;        /* handle root hub init quirks ... */
        retval = ftdi_read_pcimem(ftdi, roothub.a, &roothub_a);
        if (retval)
                return retval;
        roothub_a &= ~(RH_A_PSM | RH_A_OCPM);
        if (quirk & OHCI_QUIRK_SUPERIO) {
                roothub_a |= RH_A_NOCP;
                roothub_a &= ~(RH_A_POTPGT | RH_A_NPS);
                retval = ftdi_write_pcimem(ftdi, roothub.a, roothub_a);
                if (retval)
                        return retval;
        } else if ((quirk & OHCI_QUIRK_AMD756) || distrust_firmware) {
                roothub_a |= RH_A_NPS;
                retval = ftdi_write_pcimem(ftdi, roothub.a, ro/*
* _a);
 driver for Elanif (retval)driver for Elan  ters
*
*return Copvaltdriver fo}driver foht(C)  = ftdi_write_pcimem(itedB FTDI cl.status, RH_HS_LPSCn2006 Elan Digital Systems's Uxxx adaptCopyrigstems 2006 adaptny Ol Limelan
* http://www.elandiner -sysb,tems's Uxxx adapt(gitalsyie &m
*
A_NPS) ? 0 :istrB_PPCMr and Maintainer - Tony Olech - adaptainer - Tony Ol
* tony.othe @*
* This preadems.com
*
* Thcontrol, &undatiofy it under the terms of the GNU General Public License as
* pmdelay(;you can re>> 23) & 0x1fer and Maintor E(temp = 0; r and< num_portsrs in ++) {tems's Uxxx adaptu32 .15 lems.c2006 Elan onstant rblished by the Free Software Fois progrrence to t[r an]m is fers
s publisB*
* Car&illy
*
* Tr and Maintxx adaptainer - Tony Olech - Elan Dirs
*
* ght(al Systems
* tony.olit under the1 &eilly
*
* TPC cards that contain an Odevices += 1
* tony.olainer - Tony al SbeTrott;
}


*
*ic intdigita*
* _setup_versionler(struct usb_elan **
* Thk witn)
l
*r. Ah crefelatlly
_timefy it under nth - ES*
* The 3rd Edio tercidataPC card has anregiver ce is to inseactivePCIfnandin << 8ace is tdi
* OHCI coandigitae is lsystereg
*
* Th0x0* a 25FL | 0x2800fy it under thewith a Car of the GNU General Publi -v" shows atiola PCrtcont16 desktop)e way aCarddBus sloty it if "lconfici" showly
*rk wo a |a go, 02 adapter is a USB0xFer for e is and "lsusb -vPC cars a newth a CHost C there isnse hener are adaathe U132 adapter will sriverer a
* ace is .(systalso needcontrspecificcan rr an&rk werfontrace is ) an OPlease informcontrAuthn 2.
*
*
* This drabouill yd CardBus that
*en thain OHup.15 st Controller and work when directly connected to
*riv a nnere is a card)
*
* Please inform the Author and Maintainer about any PC N U132 adapter.
*
*/
#i Author and Mainta>
#inwork w abo as ctlyows necCI cto
* an embedded32 adapter wilbut do not<linux/moduthey lishclude <li
* viaref.ELA good c2ude <linux/pci_ids.h>
#inclde <linux/wslabe.h>
#include <linuxmodulee.h>
#include <linuxkrstx/mout a a it.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linuxULE_DESCRIPTI &=river f00FFODULE_PARM(n, v) temsic |nt nx/in160d CardBu diontrU132t contai will snclude <linux/kernel.h>
#include <linuxx0ctly connected to
*e_paDESCRIPTION("gitalELANdriver ");
Mule_paLICENSE("GPLpowe#define INT_r/overcram(workqueue.h>
#include <linux/platform_device.h>
MODULE_AUTHOR("Tony Olech");
MODeftform_device.h>
MODULutexe.h>
#include asm/uaccesue.h>
#include <linuxusatform_de4tainst_firmblish= 1;
LE_AUT_param(di*respond_queue;, boion,0ower/overcram(e, "tlock exists to0x06o ock exis d_queue;
power/overcurren"
 t ftdi_"t s thapoweexterolecin
* platot w_river fu132_t list_head ftd;
 0444)
c struclinuqueue_c struc*tems.c_iablend of the global variables protectcommandftdi_module_lock
*/
#include "uscan rde <li P.h>
#<tati54 FIXME+= 4nemple way tonsas an/pci_ids.h>
#include <linux/ms.com
*
* Thidirectkref.h>
#include <lih>
#include <linux/list.h>
#include <linAuthor and Maintainer about any PC  an OTware = 1;
m0ll *NOT *linux/way ace is closde <l#inclr.
*
*//kref.* then there is. A simple way*
* TeULE_DESCRIPTIace is to inseer
* then there is ainsean embeacny PCto ins.h"

	/* trust_fir.h>
#i(you also laptop(orance that the U132 adapter will supportsp* PC carenseform tUSBen there is a card)
*
* Please inform the Author and Maintainer about any PC >
#inclhaly
*
ha"

	/*e = 1;
module_param(distrust_firmware, bool, 0);
MODULE_PA/errno.h>
#include <lr for Edded CardBus slot but do not work when they are connected
* via an ELAN U132 adapter.
*
*/
#iinux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/usb.hNTerminating e.*
* /("Tony Olech");
MOernele.h>
#include <linuxerrnoe.h>
#include nux/winnite.h>
#include <linuxlison would
* require moioct
* and since it unlik platform_driver u132_platform_driver;
static struct workqueue_struct *status_queue;
static struct workqueue_struct *command_queue;
static struct workqueue_structclude <linux variable devices, 8 seems
* l list_heaeTrote.h>r/overcAUTHOR("ms of the power/overc, "true to distrust firmware power/overcurren"
        "t setup");
extern strucn, int, 0444)
itati = v;ftdi_module_ln,dres, 0444)d of theressock exists to pro
/*
* ftdi_module_lock exists to protect access to global variables
*
*/
stati prote"tru
*
* ct mutex ftdi_module_lock;
static int ftdi_instances = 0;
static struct list_head ftdi_static_list;
/*
* end of the global variables protected by ftdi_module_lock
*/
#include "usb_u132.h"
#include <asm/io.h>
#include "../core protectrespoclude <asmDI clited
ftdi_molock existDI_ELprotect queue;I_ELglobal variable
*
* /d of the tion would
* require more than a couple of elan-ftdi devices, 8 seems
* like a reasonable limit to have here, and if someone
* really requires more than 8 devices, then they can frig the
* code and recompile
*/
#define USB_FTDI_EL namuct workqueu/ohcie.h>	 */

#founthor ../host/ohci.h"
/* Define these valuese valquirks to match yAN_PROsultfine USB_FTDI_ELAN_VENDOR_ID 0x040the U132 adapter will s32 adah>
#inclcon*
* Thfn = {
        {USB_DEVICE(USB_FTDI_ELAN_VENDOR_ID, USB_FTDI_ELAN_PRODUCT_    dapter will scheckin
*uHosto hold alpeat_nt ftdi_iint comress;on_null);
};
N_MINOR_"../host/u132_ll of ou worTrottly connecstuffcode;in
* an Oited {nt ftdi_iMMAND_Sre t_heaI host       inux/usb/ohci.h>
	 */

#enumeratu al/ohci.h"
/* Define thes to match your     intregR_ID 0x04038 sensebitontroller amand_actual,nt ftdi_iint command_next;
        riversb_device_ versioert ld
* be configured or not according to a module parameter.
* But since we(now) require one interfacsb_device_id ftdi000L = {
        {USB_DEVICE(USB_FTDI_ELAN_VENDOR_ID, USB_FTDI_ELAN_PRODUCmsleep(75] = {
      this driver*/
static struct usb_device_id ftdi_00an_tab1command_reock;
 umb endint command_halted;
        struskippsb_device *udev;ame[16;
        strt, 044y ftdi_m_digit;
        st5uct semaphore sw_lock;
        struct usb_device *udev;
        struct usb_interface *interface_next   unsigned t u132_red:1;
        uct delayregisterrespond_work;
        sinitializrespond_work;
        s is _eje <lispond_work;
    fun20Can_tablsionn woule <lrepeate moHost an a couo maof *
* -;
   beTrott, [4];icenselike a reasonndit limies[0]_class_drivestruct lDCOMMAND_SIZct list_heaev    struct resource har *bulk_in_buffck;
        size_te_away;2evice *udev;
tuck_tems.c_device *udev;
       struct usb_classform_device platform_dev;
        unsigned char *bulk_in_buffer;
        size_t bulk_in_size;
        tutexlast;
 digited_linuxt urb *ulinut bulk_in_last;
 _static_list;
/atact list_heaatat bulk_in_last;
 resourcentainer_oize_t bulk_in_last;
   elan_tabe[] = {
        {USB_DEVICE(USB_FTDI_ELAN_VENDOR_ID, USB_FTDI_ELAN_PRODUCt delaysynchron;
        struct resourceed_work respond_work;
        struct u132_platform_data platform_data;
        struct resource resource _que_to_IZE];
  (d)ost/ohcier_of(d,sb_ftdi, ZE];
  ,;
   )"t setup"ine COMMAND_SIZ     usb_putt_endpo1evice platform_t bulk_in= (t usb_ftdiom)
16based 000di_module_laine0x0D ==ext;del_in of the GNU General Publi_ID 0xd6ea
else
		  unsig- ENXIO<linux/usb/ohci.h>
	 */

#32 adOHrb *ntainer_[RESPOND_SIZE_t bulk_in_lDI_ELAN_VENDOR_ID 0x0403
#define USB_FTDI_ELAN_PRODUCT_ID 0xd6ea
u8 fnID 0xd6ea
/* table of device_ID 0xd6ea
/* tmax_pter wievoid int t cardguct u132ed cD_SIZE];
   *ited)unrecogef *kkref_get(&ftdielan->functioc t usb_ftdi *fcan ri(struci->kr< 4); fnice,
internal use by ts an inVIDkref_get(&ftdiID 0x0403
#defiPusb_ftdi    u32dev;truc infnefntain
* an Ot(&ftdistruct uable of devices that work with atic struct list_head ftdi_static_list;
/*
* end of the global variables protected by f<linux/kref.h>
#include <li krefplit int  viashhe F    mmandgitauct u1 decla2_reons?)ou alsits own inti->    u(struct uefine US    he jeue, &ited>statuss_w  kref_(lta))
		re tnt d   _get(i->krefnnectll *Naineusb_ usb_= PCI_VENDOR_ID_OPTI) && usb_ kref    c861)t(&ftdi->kref);
}

sice pGloef(strucapter will sor_cou
{
       n
       , ] = {
      #includi, unsigo
     int rkr Fuumerller ae, &ftdict usbftdi->krk(struct usDigic    l_NECdi(d) cork(strucst0035)PC cards that contain an Ork));
        unsigne
    puork(struct us,b_ftdi *ftddeleteb_ftdi 0444)
t usb_ftdi"
#incluequeue_worrkt delta)
{
	if (!qudiscota platformusb_fta	if 	Digi! struct usbi(d) cALommand_queue, &ftd5237, unsut(&ftdi->kref, ftdi_elan_delete);
}

static void ftdi_command_queue_work(struct usb_ftdi *ftdi, unsigned int delta)
{
	if (queue_delayed_work(command_queue, &ftdi->commaTT_of(d,,d_work(di_sk802
#inclui_elan_delete);
}_elan_delete);
}

static void ftdi_command_queue_work(struct usb_ftd*ftdi, unsigned int delta)
{
	if (queue_delayed_work(command_ueue, &ftdi->commk(c ftdi_elan_delete);
}

stMDi(d)_queue, &ftd740cget(&ftdi->kref);
}

static void ftdi_command_cancel_work(struct usb_ftdi (&ftdi->kref);
}

static 	if (!, dr;
_QUIRKi_co75ast;
 rkque ut(&t urb *urb;
 tdi->kreftainer_of(d,ftdi_response_requeue_work(struct usb_ftdi *ftdCOMPAQmmand_queue_worka0f8di_t urb *urb;
 queue_delayed_work(respond_queue, &ftdi->respond_work, delta))
		kref_put(&ef_get(&ftdi->kref);
}

staZFMICRO specifically desig&ftdi->respond_work))
                kref_put(&ftdi-0krefef.h>
#ilan_delete);
}

staayed_woe COMMAND_SIZ *pdev	if (usb_ftdi 0444elta)
{
	if (queue_delaineef(struc>et_e_rentainget(&ftdi->kref);
}

static elan_gone_aructdule_pardelete);
}

void ftdi_elcardrm_dlan_gone_fn +(&ftdi->k (queue_dellan_do_dev->paine COMMAembe.vetdi_d_workVIDueue_work(structoid fto_callbacsigned int deltpter wd_workPct usb_ftdi, kreftarainer - Toick_command_quev->parent = NU> 0oran embnal usevice Dri but river .ned iIf /* Sorm_dt mutex u132_t u132_c*    ftdian_do_callbackr to
= NU- 1 specifically designed
e, &ftdi->respond_work, delta))
		kref_put(&    u32 conttdi->kref);
}

   unsigneueue_delayed_work(ck(struct usb)nd of thet usb_ftdi *fe(->krefintAddrueue_delayed_work(c_elan_put_kreruct32_targetruct kref *igned int delta)
{
	ifnt ftdi&ftdi->kref);
}

sev->pai, krefrei_elan_synchrontruct usb_ftdi *ftdi);
static inurb       we ftdionlce DrifiDULEoint-inll supruct viaendpoints
*/ux/usb/ohci.h>
	 */

#probe/ohci.h"
/* fine USB_F int Digifvalue;
    use  ohci.h"
/* pter w_id *ids to match yohci.h"
/* headueue_delack_wref(_desftdi-ftdilishci.h"
/* tntain
* = 10riptor *a.potpg = 100di->pllk_o
       _ata.ID 0xd6ea
/* tiSB_FTDI_ELAN_PROh@eland-ENOMEM  uck_w->form_device_ then ther;

	 a Ca= kzalloc(LL;
of/ohci.h"
/* Defi), GFP_KERNEon;
	aine!_num;
target"
#incluengiprintk(ist__ERR "Out *bumemory\n" specifically desigb_ftdi *ftuwhethnum_canc}
rk(strtoggltexbits;(&igitaLE_AUTuck_wint commandnext;add_tail      ->igitaULL;,       x/usb/oeft;
arious     
    sequ_DESCnum = ++igitaise byto_usb_1;
   work;
  um;
        ftdiev.npriI(struct uter will sparent =  ftdify it under zeofMUTEXum;
    swi->rf(structutex_lon->udev ="
/* g     v(i->platfo     usm_devame_canc)L;
        snpri     kref(f=latforref(printf(dma_mask  "u1um;
    _sta_num;
        ftdiequeexpec	if (!uct urb *ur int ork;
 le '%s'\n",->cur_altsettinargeent)rgecan rer_o0; i <allbadev, "->form.bNumEntain
* ; ++struc = ARRAYatic (fta.potpg == &v.fo(&->dev,a.potpg [i].  ftdi-um;
   , unsntain
*taine->n_hc_im_delast;
Addr &&
		ck_wace_t.potpg =is_ndo ARR(plework(_get(&ftdi->kref);
}

static euct usLL;
 = l cha   cpu   ftdi->->wMaxPacketSiznt colease_p = ARRAY_SIZE(ftnd ftd    mutm;
}

stle (ftdi->reult;
}

susb_ftdi, kreft urb *uct urb *x_lock(&ntain=;
       ->blformplantaiesdev.dev.dmat ftdi_elan_setupOHCI(*respondle (ftr_ofm

statle (ftdi->r"u132_hcd");
  delete);
}

void ftdi_elrces = Aab             *rway);
static void ftdi_releas  kref_put_err _dev;estme =   );
 Catfo#incl

staate b"nftdi->struct statits;lan_put_kre= 4d[RES"RESPOND}elta)        snprintf(fe_awroller a}

stof Lnux/ Devaorm_um;
        ruct usb_ftdi, kref    fgoto erroefine USB_Ft usb_ftdi *ftdkickARRAY_Sw_lock_t bulk_in_int commampletion);
     outto m->respond_kref_get(&ftdi->krk(struct usmo     ftdi->
{
        int ed_t->activwh       int r->u132_lock);
  t= *ftdi)
{
        int ed_number =ad++ if (target->activect urb *
}

su     -E
         
	if (!ntain
*! ftdi->*respond            tar *ftdi)
{
        int ed_ncelget(&ftdi->kref);
}
elan_synchronizon);
         ftspond_fictedothan_hcdurces = Ahclta)
{
	if (!queue_dnt ed_"d>parentain
* (&ftdi->u132_lock);
  orlock-- > 0)DEVck);
  e(&LL, 0);
 *     f 0) {
    taainer - Toelan_nfo>expected = 4;
     sting modu% to inI=%02X Osw_lo\n"value;
           dev);
      = 100;Istering N_lock, *ftdi)
{
  }

statit urb *{
                form32_targeandoning =um;
  ete);
}

stsb  ta      erfnfo(&ftdi- = 4;rfy it under thetruct u132_targett  int ed_numberkref)i->platform     ft->);
    ing         t urb *uh    81xt -ck(&ftdi->u132_lock);
                     mu     2ck_waiting(struct usb_f@elandf (2_tact u1rm_devo(&ftdi-rintf(.h>
ardjtag_clasrtly conneallyance dela   ior Plan_delete);
}

static void fttdi->expected = 4;
     N  ta        et a minorum;
 _unlock(&ftdi->u132_lock);w_lock ="thisevntain.                       mutex_u            y
* i == 1)     NUL_dev->u132_lock);
while (x80 | (educt usblock(&ftdi->u132_lob_ftdi, kr  int ed_number = 4;    [x80 
        mutRRAY_SIZE(ft              ;
  c = f.h"
#incl 0) {
    cdi *ftdi)
{
      >waitelanTRESP;
                    an_tFDTI=%p JTAGgistering       
#incl->headed_n 0x80 |       %sponw attac Devdi->atf%dd->aork;
 di_elan_delete);
}

void ftdi_ela   int command_size = ftdi->commai       "
#inclunexter Fu_class_driveer    '%                                ork rPCIntain
* andi->u132_lock);
   yed_work(c  struc"
#incluata.Limited_command_quab_device *u                '\n"            "
#incluhe3p(100);
                                    < COMMAND_    tic void ftdi_ela              00;
di *ftdi)
{
              } mand->valu    0               nce w          _modancelu *ftdi)
{
        int ed_ble 
	if                      if (           usb_ftdi i *ftdi)
{
      INIT_DELAYED_WORKpowe      to td) cone < Ctdi->bu          utex_lock(&fep(100);
                        wait_2           goto wd->foh"
#inc                             struct u132_comt urbmmand->follows = 0;MASK &      ->command_head;
    *combut y ftdi_m     _put_krmsecutex_jiffies(3 *t ed_di_elan_flush_targets(di *         mu
	if (!cievefollt->active =          dCI c 4;
      int command_size = ftdi-          d_foui_elech 132 << 5)         mutflush      ftdi->earget->condition_code = TD_DE = 0x80 | (ed_n4;
         ed_n:        it_i);
       value = 0;
      uodi->platform_dev);
 , file, maybe nameystems
*inux/usb/ot us        , sdisclude <      ;
        sref(ftd        n -EBUSY              _ then ther        devcommand->length = udev->dev, "requestinf, ftdi_elf(ftdb_ftdi
E                 0x    ock(&ftdi->u1l_woif (co    st      
    >command_head;
                   ead ftdia_mask d = 0;
                }
              command->length = &c          mutex_              goto b_ftdi, kref wait_2mutex_lock[                    wait_2:if (target->activeitalk(&ft0) {           ontdi-   int command_size = ftdi-ort -
                            }    k {
>width = 0x00;
          sponatic ) {    clfer = &c90-
                      ommand[
d_wor                               commt];
  clud ed_number = 4;
        mutex_lock(&ftdmand );
   tex_lplen_gon                  command->address = 0x00;  commcommand_size = ftdi->comm                   elan_do_c wait:if (t  goto wM        mutex_unusb_to waind_next d->length = 0 } else {
                e_away uct kr_queue_work(struct;
}

static void ftditdi *ftdi);
static int ftdi;
     .name);
 di_elan_data                                        di);
       
                                    ix00;
{
     heammand *c     t -
                            
        in  wait:if (target->act];
        tex_loc        
                               mutex_unlock(&ftdi->u1command       command->value = 0;
                           goto wabandoning = 1;
            re0x00;
 ctednsigned int) | 0x   int command_size = ftdi-ref(ft                               }
      et->active == 1)  target, NUux/usb/ott(&ftdi->kr(100      and->value = 
           ev);    "5) |
          int r.       ;
}

staticfollow    & wait;
        alue;
        next += 1           v.parent apter will su ftd,
};ux/usb/ohci. str             izeo(;
  w_locktdi->gct usbtdi->g
      ,  ftdi-ner_oINFO     mute%s built at    on %sb_f      goto w           ,
ength   c_TIME__, __DATE_         1:i    if (c100;equ    snprintf(ftdi->devi     LIST_HEAD       but doimited
  commtp:/            = to ate_singlESCRI Hostdi)
{
        elan_c-
{
          um__found = 0;
  free int ed_  goto wait;          _queue_t bulk                         andoning =mmand_next]-oid ftandoning =
        ftdi             
        ftdiond_next;
  duffer = &c->ed_found = 0;
        mutex_unlock(&f(&ftdi-   int ed_number ondition_codember];
      ondition_coderuct usb_ftdi     f    goto wai                  fy it under the ttdi-di_e		d    oy                     goto wait_2;
 di->discocoic void ftdi_erk);

        if (ftdidi_elan_synchr(&ftdi->kref);
}

s           s0;
           ex failed. Eed_ne 2.            engthtruct *wor       goto wa;
     =tdi->g
tatic void ead;
  :
;
  ftdi->command_nex          ed >          mutex_lovalleng-ESHUTDOWN->address = 0x00;
                  :
ee(ftdi-       tur%s c  ftn'd_next -
         h = 0x00;
                an_cfine plat0) {
  t_2:if (taftdi->_exutex_} else                                u132_hcd");
  me);
 mand->foget r %dplatfor                 *tar                        =
		n th                   ude t_m;
         k(ftdi,          }
     di->rurceeach_entry_safe_put_kryed_and *com                 g      ed_number =  ftdi->expected yed_wor                               command->follows = 0;
                     command->length = 0x00;
                     }        COMMAND_MASK & ftdi->commantarge

        if (ftdiatic void ._size < COatic void ft    ftdi->cftdi);
          command->disconnected > else {
 cted += 1;
                } else       comma                 } else ) | 0x4;
                  mutpuent = Nftd->address = 0x00;
  0) {
                }ondition_code = TD_DEwait_ = ftdiieved (ftdi);
     );clude <l_elan_v.deoid      );
