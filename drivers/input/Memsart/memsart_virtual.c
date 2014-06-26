#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/input.h>

#include "memsart_virtual.h"

MODULE_DESCRIPTION("Memsart Virtual Driver!!");
MODULE_AUTHOR("memsart");
MODULE_LICENSE("GPL");


struct memsart_virtual_sensor {
	struct input_dev *input_acc;
	struct input_dev *input_gyro;
	unsigned int sample_interval;
	atomic_t enabled;
};


static int memsart_acc_input_init(struct memsart_virtual_sensor *sensor)
{
	int err = 0;

	//struct input_dev *input;
	//input = input_allocate_device();
	sensor->input_acc = input_allocate_device();
	if (!sensor->input_acc) {
		printk(KERN_ERR "Failed to allocate acc input device\n");
		err = -ENOMEM;
		goto failed;
	}
	sensor->input_acc->name = ACC_NAME;
	set_bit(EV_ABS, sensor->input_acc->evbit);

	input_set_abs_params(sensor->input_acc, ABS_X, -ACC_MAX, ACC_MAX, 0, 0);
	input_set_abs_params(sensor->input_acc, ABS_Y, -ACC_MAX, ACC_MAX, 0, 0);
	input_set_abs_params(sensor->input_acc, ABS_Z, -ACC_MAX, ACC_MAX, 0, 0);

	err = input_register_device(sensor->input_acc);
	if (err) {
		pr_err("Failed to register acc input device\n");
		input_free_device(sensor->input_acc);
		sensor->input_acc = NULL;
		goto failed;
	}

	return 0;

failed:
	return err;
}

static int memsart_gyro_input_init(struct memsart_virtual_sensor *sensor)
{
	int err = 0;

	//struct input_dev *input;
	//input = input_allocate_device();
	sensor->input_gyro = input_allocate_device();
	if (!sensor->input_gyro) {
		printk(KERN_ERR "Failed to allocate acc input device\n");
		err = -ENOMEM;
		goto failed;
	}

	sensor->input_gyro->name = GYRO_NAME;
	set_bit(EV_ABS, sensor->input_gyro->evbit);

	input_set_abs_params(sensor->input_gyro, ABS_X, GYRO_MIN, GYRO_MAX, 0, 0);
	input_set_abs_params(sensor->input_gyro, ABS_Y, GYRO_MIN, GYRO_MAX, 0, 0);
	input_set_abs_params(sensor->input_gyro, ABS_Z, GYRO_MIN, GYRO_MAX, 0, 0);

	err = input_register_device(sensor->input_gyro);
	if (err) {
		pr_err("Failed to register acc input device\n");
		input_free_device(sensor->input_gyro);
		sensor->input_gyro = NULL;
		goto failed;
	}

	return 0;

failed:
	return err;
}

/***************************************************************************************/
/***************************************************************************************/

static int active_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static int active_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct memsart_virtual_sensor *sensor =
	    (struct memsart_virtual_sensor *)input_get_drvdata(input);

	if (atomic_read(&sensor->enabled))
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static int interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct memsart_virtual_sensor *sensor =
	    (struct memsart_virtual_sensor *)input_get_drvdata(input);

	return sprintf(buf, "%d\n", sensor->sample_interval);
}

static int interval_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	return count;
}

static ssize_t wake_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static int data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static int status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(active, S_IRUGO | S_IWUSR | S_IWGRP, active_show, active_set);
static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP, interval_show, interval_set);
static DEVICE_ATTR(data, S_IRUGO, data_show, NULL);
static DEVICE_ATTR(wake, S_IWUSR | S_IWGRP, NULL, wake_set);
static DEVICE_ATTR(status, S_IRUGO, status_show, NULL);

static struct attribute *sysfs_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_interval.attr,
	&dev_attr_data.attr,
	&dev_attr_active.attr,
	&dev_attr_wake.attr,
	NULL
};

static struct attribute_group sysfs_attribute_group = {
	.attrs = sysfs_attributes
};

static int create_virtual_sysfs(struct input_dev *input)
{
	int err = 0;
	err = sysfs_create_group(&input->dev.kobj,
					&sysfs_attribute_group);
					
	return err;
}

static int __init memsart_virtual_driver_init(void)
{
	pr_info("Memsart Virtual Driver init!!!\n");
	
	struct memsart_virtual_sensor *sensor;
	int err = 0;

	sensor = kzalloc(sizeof(struct memsart_virtual_sensor), GFP_KERNEL);
	if (sensor == NULL) {
		pr_err("Failed to allocate memory\n");
		err = -ENOMEM;
		goto failed_alloc;
	}
	
	memsart_acc_input_init(sensor);
	memsart_gyro_input_init(sensor);
	
	err = create_virtual_sysfs(sensor->input_acc);
	if (err)
		goto exit_free_input_acc;
		
	err = create_virtual_sysfs(sensor->input_gyro);
	if (err)
		goto exit_free_input_gyro;
	/*
	err = sysfs_create_group(&sensor->input->dev.kobj,
					&sysfs_attribute_group);
    
	if (err)
		goto exit_free_input;
	*/

	return 0;

//exit_free_input:
exit_free_input_gyro:
	input_free_device(sensor->input_gyro);
exit_free_input_acc:
	input_free_device(sensor->input_acc);
failed_alloc:
	pr_err("Initialized Memsart virtual driver failed\n");
	return err;
}

static void __exit memsart_virtual_driver_exit(void)
{
	printk(KERN_INFO "Memsart Virtual Driver Exit!!!\n");
}

module_init(memsart_virtual_driver_init);
module_exit(memsart_virtual_driver_exit);


