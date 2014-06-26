/* 
 * Linkingsoft Chip ID module.
 *
 * Copyright (C) 
 * 2013 - marco <marco@linkingsoft.net>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/wakelock.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/fs.h>

#define DEBUG_M 0
#if DEBUG_M
	#define D(format, arg...) printk(format, ##arg)
#else
	#define D(format, arg...) ((void)0)
#endif

#define	CPUID_DEV	"/dev/rkxx_cpuid"

static char* pri_key="0N1dvANT0L9rFnIPS1D9nDj684ZJoBL7Y8aCl4H00v344GN5j4Xy9fLz4F0R3AwP";

#define SETFLAG(x, y) ((x) = ((x) | (y)))
#define CLRFLAG(x, y) ((x) = ((x) & (y)))
#define TESTFLAG(x, y) ((x) & (y))
#define CHARTOINT(a, b, c, d) ((((a) & 0xFF) << 24) | (((a) & 0xFF) << 16) | (((c) & 0xFF) << 8) | (((d) & 0xFF)))

#define ITME_S     1000 
#define ITME_M     60000 /* 60 * ITME_S */
//#define ITME_M     10000 /* 10 * ITME_S */
#define ITME_H     3600000 /* 60 * 60 * ITME_S */

#define CHIPID_DEVCIE_NAME	"chipid"
#define MSG_LEN	256

typedef struct _flag {
	unsigned char UD[24];
	unsigned char C[4];
	unsigned char R[3];
	unsigned char sum;
}flag;

/* define msg information */
typedef struct _msg{
	unsigned char C[128]; /* Data cpuId*/
	unsigned char D[32];  /* Data magicId */
	unsigned char R[64];  /* Random */
	flag F;  /* flag */
} cmd_t;

typedef struct {
    char name[10];
    struct class *cla;
    struct device *dev;
    int major;
} chr_t;

static  chr_t lks_chipid = {
    .cla = NULL,
    .dev = NULL,
    .major = -1,
};

//static spinlock_t flag_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t flag_lock;

typedef struct _watch_flag {
	int timeout;
}WATCH_FLAG;

static WATCH_FLAG lks_flag = {
	.timeout = 15,
};

ssize_t chipid_cmd(struct class *cla, struct class_attribute *attr, const char *buf, size_t count);

static struct class_attribute chipid_class_attrs[] = {
    __ATTR(cmd, S_IRUGO | S_IWUSR, NULL, chipid_cmd),
    __ATTR_NULL,
};


static struct class chipid_class = {
        .name = CHIPID_DEVCIE_NAME,
       // .class_attrs = chipid_class_attrs,
};

static struct task_struct *watchDog;
static int kthread_index = 0;
static int ko_open_conect = 1;

static unsigned char r[64];
static int random = 0;

extern int efuse_read(uint8_t *pdata);

void creat_key(unsigned char* src, int len) {
	int i = 0;
	for (i = 0; i < len; ++i) {
		src[i] |= 0x01;
	}
}

void encode_mod_or(unsigned char* src, unsigned char* key, int len) {
	int i = 0;
	for (i = 0; i < len; ++i) {
		if ((src[i] & 0xFF) & 0x01)
			src[i] += key[i];
		else
			src[i] -= key[i];
	}
	for (i = 0; i < len; ++i) {
		src[i] ^= key[i];
	}
}

void decode_mod_or(unsigned char* src, unsigned char* key, int len) {
	int i = 0;
	for (i = 0; i < len; ++i) {
		src[i] ^= key[i];
	}
	for (i = 0; i < len; ++i) {
		if ((src[i] & 0xFF) & 0x01)
			src[i] += key[i];
		else
			src[i] -= key[i];
	}
}

void priStr(unsigned char* buf, int len) {
	int i; 
	printk("\n====================\n");
	for (i = 0; i < len; i++) {
		if (((i+1) % 20) == 0) printk("\n");
		printk("[%02x]", buf[i]);
	}
	printk("\n====================\n");
}

static inline int _chipid_run_cmd(const char *cmd, cmd_t  *op)
{
	return 0;
}

static inline void kernel_panic(void)
{
	D("[MARCO] ------------- DO KERNEL PANIC---------------\n");
	while(1) {
		unsigned char*  mr = kmalloc(sizeof(1024), GFP_KERNEL);
	}
	/* nothing. */
}

/* check_sum  */
static unsigned char check_sum(unsigned char* buf, int len) {
	unsigned int sum;
	for (sum = 0; len > 0; len--)
		sum += *buf++;
	sum = (sum >> 8) + (sum & 0xFFFFFFFF);
	sum += (sum >> 8);
	return ~sum;
}

/* Get CPUID */
static int get_cpu_id(unsigned char* buf) 
{
	int i = 0;
	if (efuse_read(buf) == 0) {
		return 0;
	}else{
		D("[CHIPID]: read error !!!\n");
	}
	return -1;
}

/* check magic is LK magic id */
#define MAGIC_FLAG_0	0x4C /* L */
#define MAGIC_FLAG_1	0x4B /* K */
#define MAGIC_FLAG_2	0x4D /* M */
#define MAGIC_FLAG_3	0x44 /* D */
static int check_magic(char* magic) {
	if (magic[0] == MAGIC_FLAG_0 && magic[1] == MAGIC_FLAG_1 && 
			magic[2] == MAGIC_FLAG_2 && magic[3] == MAGIC_FLAG_3 ){
		if (check_sum(magic, 32) == 0) {
			return 0;
		}
	}
	return -1;
}

static int thread_watchDog(void *arg)
{
	WATCH_FLAG* m_flag = (WATCH_FLAG*) arg;
	/* Watch Dog. */
	while(1) {
		m_flag->timeout = 5;
		while(m_flag->timeout > 0) {
			/* open once */
			if (ko_open_conect > 3) {
				D("\n----[%d]----\n", __LINE__);
				kernel_panic();
				break;
			}
			msleep(ITME_M);  
			m_flag->timeout--;
		}
		D("\n----[%d]----\n", __LINE__);
		kernel_panic();
	}
	return 0;  
}

ssize_t chipid_cmd(struct class *cla, struct class_attribute *attr, const char *buf, size_t count)
{
    cmd_t op;
    _chipid_run_cmd(buf, &op);
    return strlen(buf);
}

static int lks_chipid_open(struct inode * inode, struct file * file)
{
    cmd_t  *op;
    op = (cmd_t*)kmalloc(sizeof(cmd_t), GFP_KERNEL);
    if (IS_ERR(op)) {
		D("[M] Can't alloc an op_target struct .\n");
        return -1;
    }
    file->private_data = op;
    return 0;
}

static ssize_t lks_chipid_read(struct file *file, char __user *buf, size_t size, loff_t*o)
{
	if (size != MSG_LEN && check_sum(file->private_data, MSG_LEN) != 0) {
		/* TODO: kernel crash */
		D("\n----[%d]----\n", __LINE__);
		kernel_panic();
		return -1;
	}
    cmd_t  *op = file->private_data;
	copy_from_user(op, buf, MSG_LEN);
	unsigned int cmd = CHARTOINT(op->F.C[0], op->F.C[1], op->F.C[2], op->F.C[3]);
	switch (cmd) {
		case 0x01010101: /* get cpuid */
			if (get_cpu_id(op->C) == 0) {
				/*TODO: encode cpuid */
#if 0
				unsigned char*  mr = kmalloc(sizeof(64), GFP_KERNEL);
				if (IS_ERR(mr)) {
					D("[M] - KMALLOC mr ERROR -\n ");
					return -1;
				}
#endif
				unsigned char  mr[64]  = {0};
				memcpy(mr, op->R, 64);
				creat_key(mr, 64);

				unsigned char* p = op->C;
				encode_mod_or(p, pri_key, 64);
				encode_mod_or((p + 64), pri_key, 64);
				encode_mod_or(p, mr, 64);
				encode_mod_or((p + 64), mr, 64);
				op->F.sum = 0x00;
				op->F.sum = check_sum(op, MSG_LEN);
#if 0
				kfree(mr);
#endif
				return copy_to_user(buf, op, MSG_LEN);
			}
			return -1;
			break;
		default:
			D("\n----[%d]----\n", __LINE__);
			kernel_panic();
			break;
	}
	return -1;
}

static ssize_t lks_chipid_write(struct file *file, const char __user *buf, size_t bytes, loff_t *off)
{
	if (bytes != MSG_LEN  && check_sum(file->private_data, bytes) != 0) {
		/* TODO: kernel crash */
		D("\n----[%d]----\n", __LINE__);
		kernel_panic();
		return -1;
	}
    cmd_t  *op = buf;
		/* Random can't same like last */
	if (strncasecmp(r, op->R, 64) == 0) {
		/* TODO: kernel crash */
		random++;
		if (random > 3) {
			D("\n----[%d]----\n", __LINE__);
			kernel_panic();
			return -1;
		}
	}else{
		random = 0;
	}
	
	unsigned int cmd = CHARTOINT(op->F.C[0], op->F.C[1], op->F.C[2], op->F.C[3]);
	switch (cmd) {
		case 0x01010101: /* get cpuid first write */
			copy_from_user(file->private_data, buf, MSG_LEN);
			op = file->private_data;
			memcpy(r, op->R, 64);
			break;
		case 0x02020202: /* check magicID & cpuID */
			copy_from_user(file->private_data, buf, MSG_LEN);
		/*
			if (check_IDS(file->private_data)  != 0) {
				return -1;
			}
		*/
			break;
		case 0x03030303: /* write watchdog */
			lks_flag.timeout = 5;
			break;
		default:
			return -1;
	}
    return bytes;
}
static int lks_chipid_ioctl(struct inode *inode, struct file *file, unsigned int ctl_cmd, unsigned long arg)
{
    cmd_t  *op = file->private_data;
    return 0;
}
static int lks_chipid_release(struct inode *inode, struct file *file)
{
    cmd_t  *op = file->private_data;

    if (op) {
        kfree(op);
    }
    return 0;
}

static const struct file_operations lks_chipid_fops = {
    .open = lks_chipid_open,
    .read = lks_chipid_read,
    .write = lks_chipid_write,
    .unlocked_ioctl	= lks_chipid_ioctl,
    .release = lks_chipid_release,
    .poll = NULL,
};

int  create_chipid_device(chr_t *chipid)
{
    int ret ;

    ret = class_register(&chipid_class);
    if (ret < 0) {
		D("[M] - creat class register class error -\n ");
        return ret;
    }
    chipid->cla = &chipid_class ;
    chipid->dev = device_create(chipid->cla, NULL, MKDEV(chipid->major, 0), NULL, strcat(chipid->name, "_dev"));
    if (IS_ERR(chipid->dev)) {
		D("[M]--- create chipid device error \n");
        class_unregister(chipid->cla);
        chipid->cla = NULL;
        chipid->dev = NULL;
        return -1 ;
    }

	D("[M]-- create chipid device success\n");
    return  0;
}

static int __init chipid_init_module(void)
{
	int ret = -1;
    sprintf(lks_chipid.name, "%s", CHIPID_DEVCIE_NAME);
    lks_chipid.major = register_chrdev(0, CHIPID_DEVCIE_NAME, &lks_chipid_fops);
    if (lks_chipid.major < 0) {
		D("[M] ----------- chipid_init_module init error [%d]------------------\n", lks_chipid.major);
    } else {
        ret = create_chipid_device(&lks_chipid);
		D("[M] ----------- create_chipid_device  major[%d] ret [%d]------------------\n", lks_chipid.major, ret);
        if (0 > ret) {
            unregister_chrdev(lks_chipid.major, lks_chipid.name);
            lks_chipid.major = -1;
        }
    }

	kthread_index++;
	watchDog = kthread_run(thread_watchDog, (void*)&lks_flag, "WatchDog%d", kthread_index); 
	if ( IS_ERR(watchDog) ){
		D("[M] create watchDog error !\n");
		/* TODO: do crash. */
		D("\n----[%d]----\n", __LINE__);
		kernel_panic();
	}else{
		D("[M] create watchDog OK !\n");
	}
	return (lks_chipid.major > 0) ? 0: -1;
}
static __exit void chipid_remove_module(void)
{
	if ( !IS_ERR(watchDog) ) {
		ko_open_conect  = 5;
		kthread_stop(watchDog);
		D("[M] stop watchDog !\n");
	}
    if (0 > lks_chipid.major) {
        return ;
    }
    device_destroy(lks_chipid.cla, MKDEV(lks_chipid.major, 0));
    class_unregister(lks_chipid.cla);
    return ;
}

/****************************************/
#if 0 //DEBUG_M
module_init(chipid_init_module);
module_exit(chipid_remove_module);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linkingsoft chip ID module");
MODULE_AUTHOR("marco <marco@linkingsoft.net>");
#else
subsys_initcall(chipid_init_module);
#endif
