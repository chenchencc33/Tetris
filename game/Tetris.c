#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "Tetris.h"
#include "Tool.h"

#define DRIVER_NAME "Tetris"

struct tetris_dev
{
	struct resource res; /* Resource: our registers */
	void __iomem *virtbase; /* Where registers can be accessed in memory */
	tetris_arg_t t_arg;
} dev;

// Write the args
static void write(tetris_arg_t *t_arg, int flag)
{
	iowrite16(t_arg->p, dev.virtbase + 2 * flag);
	dev.t_arg = *t_arg;
}

/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
static long tetris_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	tetris_arg_t vla;

	switch (cmd)
	{
		case TETRIS_WRITE_POSITION:
			if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 0);
			break;
		case TETRIS_DEL_ROW:
			if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 1);
			break;
		case TETRIS_WRITE_SCORE:
			if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 2);
			break;
		case TETRIS_WRITE_NEXT:
			if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 3);
			break;
		case TETRIS_WRITE_SPEED:
			if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 4);
			break;
        case TETRIS_RESET:
            if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 5);
			break;
        case TETRIS_PAUSE:
            if (copy_from_user(&vla, (tetris_arg_t*) arg, sizeof(tetris_arg_t))) return -EACCES;
			write(&vla, 6);
			break;
		default: return -EINVAL;
	}

	return 0;
}

/* The operations our device knows how to do */
static const struct file_operations tetris_fops =
{
	.owner = THIS_MODULE,
	.unlocked_ioctl = tetris_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice tetris_misc_device =
{
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &tetris_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init tetris_probe(struct platform_device *pdev)
{
	int ret;
	ret = misc_register(&tetris_misc_device);

	/* Get the address of our registers from the device tree */
	ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
	if (ret)
	{
		ret = -ENOENT;
		goto out_deregister;
	}

	/* Make sure we can use these registers */
	if (request_mem_region(dev.res.start, resource_size(&dev.res), DRIVER_NAME) == NULL)
	{
		ret = -EBUSY;
		goto out_deregister;
	}

	/* Arrange access to our registers */
	dev.virtbase = of_iomap(pdev->dev.of_node, 0);
	if (dev.virtbase == NULL)
	{
		ret = -ENOMEM;
		goto out_release_mem_region;
	}

	return 0;

	out_release_mem_region:
		release_mem_region(dev.res.start, resource_size(&dev.res));
	out_deregister:
		misc_deregister(&tetris_misc_device);
	return ret;
}

/* Clean-up code: release resources */
static int tetris_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&tetris_misc_device);
	return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id tetris_of_match[] =
{
	{ .compatible = "csee4840,vga_tetris-1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, tetris_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver tetris_driver =
{
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tetris_of_match),
	},
	.remove	= __exit_p(tetris_remove),
};

/* Called when the module is loaded: set things up */
static int __init tetris_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&tetris_driver, tetris_probe);
}

/* Calball when the module is unloaded: release resources */
static void __exit tetris_exit(void)
{
	platform_driver_unregister(&tetris_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(tetris_init);
module_exit(tetris_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSEE4840, Columbia University");
MODULE_DESCRIPTION("Tetris driver");
