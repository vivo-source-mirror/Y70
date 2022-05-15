/*
 * Goodix GTX5 Gesture Dirver
 *
 * Copyright (C) 2015 - 2016 Goodix, Inc.
 * Authors:  Wang Yafei <wangyafei@goodix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <asm/atomic.h>
#include "goodix_ts_core.h"

#define GSX_REG_GESTURE_DATA			0x4100
#define GSX_REG_GESTURE_BUFFER_DATA		0x4128
#define GSX_REG_GESTURE				0x6F68

#define GSX_GESTURE_CMD				0x08

#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_KEY_DATA_LEN	37
#define GSX_BUFFER_DATA_LEN     513

/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: store valied gesture type,each bit stand for a gesture
 * @gesture_data: gesture data
 * @gesture_ts_cmd: gesture command data
*/
struct gesture_module {
	atomic_t registered;
	unsigned int kobj_initialized;
	rwlock_t rwlock;
	unsigned char gesture_type[32];
	unsigned char gesture_data[GSX_KEY_DATA_LEN];
	unsigned char gesture_buffer_data[GSX_BUFFER_DATA_LEN];
	struct goodix_ext_module module;
	struct goodix_ts_cmd cmd;
};

struct gesture_module *gsx_gesture_V2; /*allocated in gesture init module*/


/**
 * gsx_gesture_V2_type_show - show valid gesture type
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >=0 - succeed,< 0 - failed
 */
static ssize_t gsx_gesture_V2_type_show(struct goodix_ext_module *module,
				char *buf)
{
	int count = 0, i, ret = 0;
	unsigned char *type;

	if (atomic_read(&gsx_gesture_V2->registered) != 1) {
		VTI("Gesture module not register!");
		return -EPERM;
	}
	type = kzalloc(257, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture_V2->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture_V2->gesture_type, i)) {
			type[count] = i;
			count++;
		}
	}
	type[count] = '\0';
	if (count > 0) {
		/* TODO 这里使用scnprintf需要确认一下是否有效 */
		ret = scnprintf(buf, PAGE_SIZE, "%s", type);
	}
	read_unlock(&gsx_gesture_V2->rwlock);

	kfree(type);
	return ret;
}

/**
 * gsx_gesture_V2_type_store - set vailed gesture
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to valid gesture type
 * @count: length of buf
 * Returns >0 - valid gestures, < 0 - failed
 */
static ssize_t gsx_gesture_V2_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		VTE("Parameter error");
		return -EINVAL;
	}

	write_lock(&gsx_gesture_V2->rwlock);
	memset(gsx_gesture_V2->gesture_type, 0, 32);
	for (i = 0; i < count; i++)
		gsx_gesture_V2->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&gsx_gesture_V2->rwlock);

	return count;
}

static ssize_t gsx_gesture_V2_enable_show(struct goodix_ext_module *module,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&gsx_gesture_V2->registered));
}

static ssize_t gsx_gesture_V2_enable_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	unsigned int tmp;
	int ret;

	if (sscanf(buf, "%u", &tmp) != 1) {
		VTI("Parameter illegal");
		return -EINVAL;
	}
	VTD("Tmp value =%d", tmp);

	if (tmp == 1) {
		if (atomic_read(&gsx_gesture_V2->registered)) {
			VTD("Gesture module has aready registered");
			return count;
		}
		ret = goodix_register_ext_module_V2(&gsx_gesture_V2->module);
		if (!ret) {
			VTI("Gesture module registered!");
			atomic_set(&gsx_gesture_V2->registered, 1);
		} else {
			atomic_set(&gsx_gesture_V2->registered, 0);
			VTE("Gesture module register failed");
		}
	} else if (tmp == 0) {
		if (!atomic_read(&gsx_gesture_V2->registered)) {
			VTD("Gesture module has aready unregistered");
			return count;
		}
		VTD("Start unregistered gesture module");
		ret = goodix_unregister_ext_module_V2(&gsx_gesture_V2->module);
		if (!ret) {
			atomic_set(&gsx_gesture_V2->registered, 0);
			VTI("Gesture module unregistered success");
		} else {
			atomic_set(&gsx_gesture_V2->registered, 1);
			VTI("Gesture module unregistered failed");
		}
	} else {
		VTE("Parameter error!");
		return -EINVAL;
	}
	return count;
}

/**
 * gsx_gesture_V2_data_show - show gesture data read frome IC
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >0 - gesture data length,< 0 - failed
 */

static ssize_t gsx_gesture_V2_data_show(struct goodix_ext_module *module,
				char *buf)
{
	int count = GSX_KEY_DATA_LEN;

	if (atomic_read(&gsx_gesture_V2->registered) != 1) {
		VTI("Gesture module not register!");
		return -EPERM;
	}
	if (!buf) {
		VTI("Parameter error!");
		return -EPERM;
	}
	read_lock(&gsx_gesture_V2->rwlock);

	count = scnprintf(buf, PAGE_SIZE, "Previous gesture type:0x%x\n",
			  gsx_gesture_V2->gesture_data[2]);
	read_unlock(&gsx_gesture_V2->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs_V2[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_V2_type_show,
		gsx_gesture_V2_type_store),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_V2_enable_show,
		gsx_gesture_V2_enable_store),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_V2_data_show, NULL)
};

static int gsx_gesture_V2_init(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	int i, ret;
	struct goodix_ts_device *ts_dev = NULL;
	if(core_data)
		ts_dev = core_data->ts_dev;
	else
		return 0;

	if (!core_data || !ts_dev->hw_ops->write || !ts_dev->hw_ops->read) {
		VTE("Register gesture module failed, ts_core unsupported");
		goto exit_gesture_init;
	}

	gsx_gesture_V2->cmd.cmd_reg = GSX_REG_GESTURE;
	gsx_gesture_V2->cmd.length = 3;
	gsx_gesture_V2->cmd.cmds[0] = GSX_GESTURE_CMD;
	gsx_gesture_V2->cmd.cmds[1] = 0x0;
	gsx_gesture_V2->cmd.cmds[2] = 0 - GSX_GESTURE_CMD;
	gsx_gesture_V2->cmd.initialized = 1;

	memset(gsx_gesture_V2->gesture_type, 0, 32);
	memset(gsx_gesture_V2->gesture_data, 0xff, GSX_KEY_DATA_LEN);
	memset(gsx_gesture_V2->gesture_buffer_data, 0xff, GSX_BUFFER_DATA_LEN);

	VTD("Set gesture type manually");
	memset(gsx_gesture_V2->gesture_type, 0xff, 32);
	/*gsx_gesture_V2->gesture_type[34/8] |= (0x1 << 34%8);*/	/* 0x22 double click */
	/*gsx_gesture_V2->gesture_type[170/8] |= (0x1 << 170%8);*/	/* 0xaa up swip */
	/*gsx_gesture_V2->gesture_type[187/8] |= (0x1 << 187%8);*/	/* 0xbb right swip */
	/*gsx_gesture_V2->gesture_type[171/8] |= (0x1 << 171%8);*/	/* 0xab down swip */
	/*gsx_gesture_V2->gesture_type[186/8] |= (0x1 << 186%8);*/	/* 0xba left swip */

	if (gsx_gesture_V2->kobj_initialized)
		goto exit_gesture_init;

	ret = kobject_init_and_add(&module->kobj, goodix_get_default_ktype_V2(),
			&core_data->pdev->dev.kobj, "gesture");

	if (ret) {
		VTE("Create gesture sysfs node error!");
		goto exit_gesture_init;
	}

	for (i = 0; i < sizeof(gesture_attrs_V2)/sizeof(gesture_attrs_V2[0]); i++) {
		if (sysfs_create_file(&module->kobj,
				&gesture_attrs_V2[i].attr)) {
			VTE("Create sysfs attr file error");
			kobject_put(&module->kobj);
			goto exit_gesture_init;
		}
	}
	gsx_gesture_V2->kobj_initialized = 1;

exit_gesture_init:
	return 0;
}
static int gsx_gesture_V2_exit(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	/*if (gsx_gesture_V2->kobj_initialized)
		kobject_put(&module->kobj);
	gsx_gesture_V2->kobj_initialized = 0;*/
	atomic_set(&gsx_gesture_V2->registered, 0);
	return 0;
}

#if 0
static void report_gesture_key(struct input_dev *dev, char keycode)
{
	switch (keycode) {
	case 0x11: /* click */
		input_report_key(dev, KEY_F7, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F7, 0);
		input_sync(dev);
		break;
	case 0x22: /* double click */
		input_report_key(dev, KEY_F6, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F6, 0);
		input_sync(dev);
		break;
	case 0xaa: /* up swip */
		input_report_key(dev, KEY_F2, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F2, 0);
		input_sync(dev);
		break;
	case 0xbb: /* right swip */
		input_report_key(dev, KEY_F5, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F5, 0);
		input_sync(dev);
		break;
	case 0xab: /* down swip */
		input_report_key(dev, KEY_F3, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F3, 0);
		input_sync(dev);
		break;
	case 0xba: /* left swip */
		input_report_key(dev, KEY_F4, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F4, 0);
		input_sync(dev);
		break;
	default:
		break;
	}
}
#endif

/**
 * gsx_gesture_V2_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
extern int touch_state; 
extern int bbk_goodix_gesture_point_get_V2(struct goodix_ts_core *core_data, u16 *data);

/**
 * gsx_gesture_V2_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_V2_before_suspend(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = core_data->ts_dev->hw_ops;
	struct goodix_ts_cmd *gesture_cmd = &gsx_gesture_V2->cmd;
	if (!gesture_cmd->initialized || hw_ops == NULL) {
		VTE("Uninitialized gesture command or hw_ops");
		return 0;
	}

	/*for (i = 0; i <  GOODIX_BUS_RETRY_TIMES; i++) {

		ret = hw_ops->send_cmd(core_data->ts_dev, gesture_cmd);
		if (!ret) {
			VTI("Set IC in gesture mode");
			ret = 0;
			break;
		}
		usleep_range(5000, 5010);
	}*/ 
	if (touch_state == VTS_ST_GESTURE) {
		ret = hw_ops->send_cmd(core_data->ts_dev, gesture_cmd);
		if (ret != 0) {
			VTE("Send gesture command error");
			return 0;
		} else {
			VTI("Set IC in gesture mode");
			atomic_set(&core_data->gestured, 1);
			atomic_set(&core_data->suspended, 0);
			return EVT_CANCEL_SUSPEND;
		}	
	}
	return EVT_CANCEL_SUSPEND;
}

/**
 * gsx_gesture_V2_before_resume - execute gesture resume routine
 * This functions is excuted to make ic out doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCLED  stop execute
 *
static int gsx_gesture_V2_before_resume(struct goodix_ts_core *core_data,
			struct goodix_ext_module *module)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	ts_dev->hw_ops->reset(ts_dev);
	return 0;
}
*/

static struct goodix_ext_module_funcs gsx_gesture_V2_funcs = {
	.irq_event = NULL,
	.init = gsx_gesture_V2_init,
	.exit = gsx_gesture_V2_exit,
	.before_suspend = gsx_gesture_V2_before_suspend
};

int goodix_gsx_gesture_V2_init(void)
{
	/* initialize core_data->ts_dev->gesture_cmd*/
	int result;
	VTI("gesture module init");
	gsx_gesture_V2 = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture_V2)
		result = -ENOMEM;
	gsx_gesture_V2->module.funcs = &gsx_gesture_V2_funcs;
	gsx_gesture_V2->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture_V2->module.name = "Goodix_gsx_gesture_V2";
	gsx_gesture_V2->module.priv_data = gsx_gesture_V2;
	gsx_gesture_V2->kobj_initialized = 0;
	atomic_set(&gsx_gesture_V2->registered, 0);
	rwlock_init(&gsx_gesture_V2->rwlock);
	result = goodix_register_ext_module_V2(&(gsx_gesture_V2->module));
	if (result == 0)
		atomic_set(&gsx_gesture_V2->registered, 1);

	return result;
}

void goodix_gsx_gesture_V2_exit(void)
{
	VTI("gesture module exit");
	if (gsx_gesture_V2->kobj_initialized)
		kobject_put(&gsx_gesture_V2->module.kobj);
	kfree(gsx_gesture_V2);
	return;
}
