// SPDX-License-Identifier: GPL-2.0
/*
 * clevo_ecwin.c —— 把 Clevo EC 的内存窗口暴露给用户态
 *
 * DSDT 里有两块 OperationRegion：
 *   RAM  : 0xFE708100, 0x1000  (XFAN@0xE7, RPM3@0xE0, RGB@0x280…)
 *   RAM2 : 0xFE708300, 0x0100  (FANB: 112 字节风扇曲线表) → 落在上面窗口的 0x300 处
 * 所以一次 ioremap 0x1000 就够，用户态按字节偏移读写。
 *
 * 写操作默认禁止（allow_write=1 才开），避免误写 EC 内存。
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define ECWIN_PHYS 0xFE708100UL
#define ECWIN_SIZE 0x1000

static void __iomem *ecwin;
static bool allow_write;
module_param(allow_write, bool, 0644);
MODULE_PARM_DESC(allow_write, "允许写入 EC 窗口（默认 0，只读）");

static ssize_t ecwin_read(struct file *f, char __user *buf, size_t len,
			  loff_t *ppos)
{
	u8 tmp[256];
	size_t n, i;

	if (*ppos >= ECWIN_SIZE)
		return 0;
	if (len > ECWIN_SIZE - *ppos)
		len = ECWIN_SIZE - *ppos;
	if (len > sizeof(tmp))
		len = sizeof(tmp);

	for (i = 0; i < len; i++)
		tmp[i] = ioread8(ecwin + *ppos + i);
	n = copy_to_user(buf, tmp, len);
	if (n)
		return -EFAULT;
	*ppos += len;
	return len;
}

static ssize_t ecwin_write(struct file *f, const char __user *buf, size_t len,
			   loff_t *ppos)
{
	u8 tmp[256];
	size_t n, i;

	if (!allow_write)
		return -EPERM;
	if (*ppos >= ECWIN_SIZE)
		return 0;
	if (len > ECWIN_SIZE - *ppos)
		len = ECWIN_SIZE - *ppos;
	if (len > sizeof(tmp))
		len = sizeof(tmp);

	n = copy_from_user(tmp, buf, len);
	if (n)
		return -EFAULT;
	for (i = 0; i < len; i++)
		iowrite8(tmp[i], ecwin + *ppos + i);
	*ppos += len;
	return len;
}

static const struct file_operations ecwin_fops = {
	.owner = THIS_MODULE,
	.read = ecwin_read,
	.write = ecwin_write,
	.llseek = default_llseek,
};

static struct miscdevice ecwin_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "clevo_ecwin",
	.fops = &ecwin_fops,
	.mode = 0666,
};

static int __init ecwin_init(void)
{
	int ret;

	ecwin = ioremap(ECWIN_PHYS, ECWIN_SIZE);
	if (!ecwin) {
		pr_err("clevo_ecwin: ioremap 0x%lX failed\n", ECWIN_PHYS);
		return -ENOMEM;
	}
	ret = misc_register(&ecwin_dev);
	if (ret) {
		iounmap(ecwin);
		return ret;
	}
	pr_info("clevo_ecwin: window 0x%lX mapped, write=%d\n", ECWIN_PHYS,
		allow_write);
	return 0;
}

static void __exit ecwin_exit(void)
{
	misc_deregister(&ecwin_dev);
	iounmap(ecwin);
}

module_init(ecwin_init);
module_exit(ecwin_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("acer-console");
MODULE_DESCRIPTION("Clevo EC memory window (fan curve / RGB / RPM)");
