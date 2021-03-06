/*
	the nothing todo driver 
	powered by Chowe.
*/

#include <linux/init.h>

#include <asm/io.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/fs.h>

#include <linux/gfp.h>

#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/poll.h>

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>

#include <linux/firmware.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/moduleparam.h>
#include <linux/timekeeping.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>

// #include <linux/i2c.h>
// #include <linux/spi/spi.h>

#define DEFAULT_DEVNAME "foo"
#define DEVICE_FIRMWARE_BIN "update.bin"

struct foo_data
{
	struct gpio_desc *irq_gpio;
	struct gpio_desc *rst_gpio;
	struct proc_dir_entry *proc_node;
	struct platform_device *pdev;
	struct task_struct *foo_kthread;
	struct timer_list timer;
	char devname[128];
	int sysfs_off;
	int miscdev_off;
	unsigned int irq;
	int gpio;
	spinlock_t spinlock;
	void *iomap;
	unsigned char *fw_data;
	int fw_size;
};

static int ext = 0; // the extern argument for this driver
module_param(ext, int, 0644);

static ssize_t foo_read(struct file *flip, char __user *data, size_t size, loff_t *offset)
{
	char buf[128];

	// fill data here

	return copy_to_user(data, buf, size);
}

static ssize_t foo_write(struct file *flip, const char __user *data, size_t size, loff_t *offset)
{
	char buf[128];
	return copy_from_user(buf, data, size);
}

static unsigned int foo_poll(struct file *flip, struct poll_table_struct *pts)
{
	unsigned int mask = 0;

	// check ready or not here

	mask = POLLIN | POLLRDNORM; // ready

	return mask;
}

static long foo_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	void *data = (void *)arg;

	switch (cmd)
	{
	case 1:
		/* code */
		break;

	case 2:
		/* code */
		break;

	default:
		return -ENOIOCTLCMD;
		break;
	}

	return 0;
}

static int foo_open(struct inode *inode, struct file *flip)
{
	return 0;
}

static int foo_release(struct inode *inode, struct file *flip)
{
	return 0;
}

static ssize_t foo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t foo_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}

static struct file_operations foo_ops = {
	.owner = THIS_MODULE,
	.read = foo_read,
	.write = foo_write,
	.poll = foo_poll,
	.unlocked_ioctl = foo_ioctl,
	.open = foo_open,
	.release = foo_release,
};

static struct miscdevice foo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &foo_ops,
};

static DEVICE_ATTR(value, 0640, foo_show, foo_store);

static struct attribute *foo_attributes[] = {
	&dev_attr_value.attr,
	NULL};

static const struct attribute_group foo_attr_group = {
	.attrs = foo_attributes,
};

static void parse_of_node(struct device_node *np, struct foo_data *fdp)
{
	if (!np)
		return;

	if (of_find_property(np, "irq-gpio", NULL))
	{
		fdp->irq_gpio = devm_gpiod_get(&fdp->pdev->dev, "irq", GPIOD_IN);
	}

	if (of_find_property(np, "rst-gpio", NULL))
	{
		fdp->rst_gpio = devm_gpiod_get(&fdp->pdev->dev, "rst", GPIOD_OUT_LOW);
	}

	if (of_find_property(np, "devname", NULL))
	{
		const char *buf;
		of_property_read_string(np, "devname", &buf);
		strcpy(fdp->devname, buf);
	}
	else
	{
		strcpy(fdp->devname, DEFAULT_DEVNAME);
	}
}

static void request_gpio_without_dts(struct foo_data *fdp)
{
	int gpio = 507;
	int ret = -1;

	if (ext)
		fdp->gpio = ext; // using extern gpio
	else
		fdp->gpio = gpio;

	if (gpio_is_valid(fdp->gpio))
		ret = devm_gpio_request_one(&fdp->pdev->dev, fdp->gpio, GPIOF_OUT_INIT_HIGH, "foo gpio");

	if (!ret)
	{
		gpio_direction_output(fdp->gpio, 1);
	}
}

static irqreturn_t foo_gpio_trigger(int irq, void *data)
{
	struct foo_data *fdp = (struct foo_data *)data;
	unsigned long flags;
	if (fdp->irq != irq)
		return IRQ_NONE;

	spin_lock_irqsave(&fdp->spinlock, flags);

	/* something to do here... */

	spin_unlock_irqrestore(&fdp->spinlock, flags);

	// irq comfirm to handle
	return IRQ_HANDLED;

	// require foo_do_irq_action_thread to handle
	return IRQ_WAKE_THREAD;
}

static irqreturn_t foo_do_irq_action_thread(int irq, void *data)
{

	return IRQ_HANDLED;
}

static int set_gpio_action(struct foo_data *fdp)
{
	int ret = 0;
	if (fdp->irq_gpio)
	{
		fdp->irq = gpiod_to_irq(fdp->irq_gpio);
		ret = devm_request_threaded_irq(&fdp->pdev->dev, fdp->irq, foo_gpio_trigger, foo_do_irq_action_thread, IRQF_TRIGGER_FALLING, fdp->devname, fdp);
		if (ret)
		{
			dev_err(&fdp->pdev->dev, "%s request interrupt failed with %d\n", __func__, ret);
			return ret;
		}
		else
			disable_irq(fdp->irq); // enable it when device open
	}

	if (fdp->rst_gpio)
	{
		// reset ic
		gpiod_direction_output(fdp->rst_gpio, 0);
		usleep_range(10, 50);
		gpiod_set_value(fdp->rst_gpio, 1);
	}

	return 0;
}

static void gen_proc_node(struct foo_data *fdp, int perm)
{
	struct proc_dir_entry *node = proc_create(fdp->devname, perm, NULL, &foo_ops);
	fdp->proc_node = node;
}

static void gen_sysfs_node(struct foo_data *fdp)
{
	fdp->sysfs_off = sysfs_create_group(&fdp->pdev->dev.kobj, &foo_attr_group);
}

static void gen_misc_dev(struct foo_data *fdp)
{
	foo_dev.name = fdp->devname;
	fdp->miscdev_off = misc_register(&foo_dev);
}

static void modify_reg_val(struct foo_data *fdp)
{
	unsigned int val;

	if (fdp->pdev->resource)
		fdp->iomap = devm_ioremap(&fdp->pdev->dev, fdp->pdev->resource->start,
								  fdp->pdev->resource->end - fdp->pdev->resource->start + 1);
	if (!fdp->iomap)
	{
		pr_warn("devm_ioremap failed at %d\n", __LINE__);
		return;
	}

	// val = readl(fdp->iomap + 0x04);
	// val |= 0x10;
	// writel(val);
}

static void foo_timeout_func(unsigned long data)
{
	struct foo_data *fdp = (struct foo_data *)data;

	// do something here after timeout
}

static int foo_kthread_func(void *data)
{
	struct foo_data *fdp = (struct foo_data *)data;

	while (!kthread_should_stop())
	{
		/* do something here */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(1000));
	}

	return 0;
}

static void foo_kthread_create(struct foo_data *fdp)
{
	fdp->foo_kthread = kthread_run(foo_kthread_func, fdp, "foo_kthread");
	if (IS_ERR(fdp->foo_kthread))
		fdp->foo_kthread = NULL;
}

static void foo_timer_create(struct foo_data *fdp)
{
	// DEFINE_TIMER(timer, foo_timeout_func, jiffies + msecs_to_jiffies(1), fdp);
	fdp->timer.data = (unsigned long)fdp;
	fdp->timer.function = foo_timeout_func;
	fdp->timer.expires = jiffies + msecs_to_jiffies(1);
	add_timer(&fdp->timer);
}

static void foo_debug_info(struct platform_device *dev)
{
	pr_debug("%s: pid=%d, %s@%d\n", current->comm, current->pid, __func__, __LINE__);

	pr_debug("name: %s\n", dev->name);

	pr_debug("reg: %x, %x\n", dev->resource->start, dev->resource->end);
}

static int get_firmware(struct foo_data *fdp)
{
	const struct firmware *fw;

	fdp->fw_data = (unsigned char *)devm_kzalloc(&fdp->pdev->dev, fw->size, GFP_KERNEL);
	if (!fdp->fw_data)
		return -ENOMEM;

	if (request_firmware(&fw, DEVICE_FIRMWARE_BIN, &fdp->pdev->dev))
		return -EINVAL;

	fdp->fw_size = fw->size;
	memcpy(fdp->fw_data, fw->data, fw->size);

	release_firmware(fw);
	return 0;
}

static int foo_probe(struct platform_device *dev)
{
	struct foo_data *fdp;

	foo_debug_info(dev);

	fdp = (struct foo_data *)devm_kzalloc(&dev->dev, sizeof(struct foo_data), GFP_KERNEL);
	if (!fdp)
		return -ENOMEM;

	fdp->pdev = dev;

	parse_of_node(dev->dev.of_node, fdp);

	spin_lock_init(&fdp->spinlock);

	if (set_gpio_action(fdp) < 0)
		return -EIO;

	get_firmware(fdp);

	modify_reg_val(fdp);
	foo_kthread_create(fdp);
	foo_timer_create(fdp);

	gen_proc_node(fdp, 0640);
	gen_sysfs_node(fdp);
	gen_misc_dev(fdp);

	platform_set_drvdata(dev, fdp);

	return 0;
}

static int foo_remove(struct platform_device *dev)
{
	struct foo_data *fdp = platform_get_drvdata(dev);

	if (fdp->fw_data)
		devm_kfree(&dev->dev, fdp->fw_data);

	if (!fdp->miscdev_off)
		misc_deregister(&foo_dev);

	if (!fdp->sysfs_off)
		sysfs_remove_group(&dev->dev.kobj, &foo_attr_group);

	if (fdp->proc_node)
		proc_remove(fdp->proc_node);

	if (fdp->iomap)
		devm_iounmap(&dev->dev, fdp->iomap);

	if (fdp->rst_gpio)
		devm_gpiod_put(&dev->dev, fdp->rst_gpio);

	if (fdp->irq_gpio)
		devm_gpiod_put(&dev->dev, fdp->irq_gpio);

	if (fdp->foo_kthread)
		kthread_stop(fdp->foo_kthread);

	try_to_del_timer_sync(&fdp->timer);

	devm_kfree(&dev->dev, fdp);

	// devm_* resource can automatically cleaned.

	return 0;
}

static const struct of_device_id foo_of_match_table[] = {
	{
		.compatible = "foo,foo", // manufacturer,model
	},
	{},
};
MODULE_DEVICE_TABLE(of, foo_of_match_table);

static struct platform_driver foo_drv = {
	.probe = foo_probe,
	.remove = foo_remove,
	.driver = {
		.name = "foo",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(foo_of_match_table),
	},
};

module_platform_driver(foo_drv);

MODULE_AUTHOR("Chowe");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("the nothing todo driver");

/* dts
        foo@0 {
            compatible = "foo,foo";
            reg = <0xf0000000 0x1000>;
            devname = "foo";
            rst-gpio = <&gpio_porta 1 GPIO_ACTIVE_HIGH>;
            irq-gpio = <&gpio_porta 2 GPIO_ACTIVE_HIGH>;
        };
*/

/*
device major

42 char	Demo/sample use

42 block	Demo/sample use

60-63 char	LOCAL/EXPERIMENTAL USE

60-63 block	LOCAL/EXPERIMENTAL USE

120-127 char	LOCAL/EXPERIMENTAL USE

120-127 block	LOCAL/EXPERIMENTAL USE
*/