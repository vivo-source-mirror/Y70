 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2015 - 2016 Goodix, Inc.
  * Authors:  Yulong Cai <caiyulong@goodix.com>
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
  *
  */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#include "goodix_ts_core.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif


#define GOOIDX_INPUT_PHYS		"goodix_ts/input0"
#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"

extern int goodix_cfg_bin_proc_V2(struct goodix_ts_core *core_data, const struct firmware *firmware);
//extern int goodix_cfg_bin_proc_V2(void *data);

extern int goodix_start_cfg_bin(struct goodix_ts_core *ts_core);
extern struct goodix_ext_module goodix_fwu_module_V2;

struct goodix_module goodix_modules_V2;

/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 */
static void  __do_register_ext_module(struct work_struct *work)
{
	struct goodix_ext_module *module =
			container_of(work, struct goodix_ext_module, work);
	struct goodix_ext_module *ext_module;
	struct list_head *insert_point = &goodix_modules_V2.head;
	int ret = -1;

	VTI("__do_register_ext_module IN, goodix_modules_V2.core_exit:%d", goodix_modules_V2.core_exit);

	/* waitting for core layer 
	if (!wait_for_completion_timeout(&goodix_modules_V2.core_comp, 100 * HZ)) {
		VTE("Module [%s] timeout", module->name);
		return;
	}
	*/

	do {
		
		ret = wait_for_completion_interruptible_timeout(&goodix_modules_V2.core_comp,  200 * HZ);
		VTI("the value of wait_for_completion_interruptible_timeout is %d", ret);
		if (ret == 0){
			VTE("Module [%s] timeout", module->name);
			return;
		}
	}while (ret == -ERESTARTSYS);


	/* driver probe failed */
	if (goodix_modules_V2.core_exit) {
		VTE("Can't register ext_module, core exit");
		return;
	}

	VTI("start register ext_module");

	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		VTE("Priority of module [%s] needs to be set",
				module->name);
		return;
	}

	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (ext_module == module) {
				VTI("Module [%s] already exists",
						module->name);
				mutex_unlock(&goodix_modules_V2.mutex);
				return;
			}
		}

		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			/* small value of priority have
			 * higher priority level*/
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		} /* else module will be inserted
		 to goodix_modules_V2->head */
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(goodix_modules_V2.core_data,
					module) < 0) {
			VTE("Module [%s] init error",
					module->name ? module->name : " ");
			mutex_unlock(&goodix_modules_V2.mutex);
			return;
		}
	}

	list_add(&module->list, insert_point->prev);
	goodix_modules_V2.count++;
	mutex_unlock(&goodix_modules_V2.mutex);

	VTI("Module [%s] registered,priority:%u",
			module->name,
			module->priority);
	return;
}

/**
 * goodix_register_ext_module_V2 - interface for external module
 * to register into touch core modules structure
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_V2(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	if (!goodix_modules_V2.initilized) {
		goodix_modules_V2.initilized = true;
		goodix_modules_V2.core_exit = true;
		INIT_LIST_HEAD(&goodix_modules_V2.head);
		mutex_init(&goodix_modules_V2.mutex);
		init_completion(&goodix_modules_V2.core_comp);
	}

/*	if (goodix_modules_V2.core_exit) {
		VTE("Can't register ext_module, core exit");
		return -EFAULT;
	}
*/
	//msleep(500);
	VTI("goodix_register_ext_module_V2 IN");

	INIT_WORK(&module->work, __do_register_ext_module);
	schedule_work(&module->work);

	VTI("goodix_register_ext_module_V2 OUT");


	return 0;
}
EXPORT_SYMBOL_GPL(goodix_register_ext_module_V2);

/**
 * goodix_unregister_ext_module_V2 - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module_V2(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module;
	bool found = false;

	if (!module)
		return -EINVAL;

	if (!goodix_modules_V2.initilized)
		return -EINVAL;

	if (!goodix_modules_V2.core_data)
		return -ENODEV;

	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&goodix_modules_V2.mutex);
		return -EFAULT;
	}

	if (!found) {
		VTE("Module [%s] never registed",
				module->name);
		mutex_unlock(&goodix_modules_V2.mutex);
		return -EFAULT;
	}

	list_del(&module->list);
	mutex_unlock(&goodix_modules_V2.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules_V2.core_data, module);
	goodix_modules_V2.count--;

	VTI("Moudle [%s] unregistered",
			module->name ? module->name : " ");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_unregister_ext_module_V2);

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
	VTI("Kobject released!");
}

#define to_ext_module(kobj)		container_of(kobj,\
					struct goodix_ext_module, kobj)
#define to_ext_attr(attr)		container_of(attr,\
					struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
	.show = goodix_ext_sysfs_show,
	.store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
	.release = goodix_ext_sysfs_release,
	.sysfs_ops = &goodix_ext_ops,
};

struct kobj_type *goodix_get_default_ktype_V2(void)
{
	return &goodix_ext_ktype;
}
EXPORT_SYMBOL_GPL(goodix_get_default_ktype_V2);

struct kobject *goodix_get_default_kobj_V2(void)
{
	struct kobject *kobj = NULL;

	if (goodix_modules_V2.core_data &&
			goodix_modules_V2.core_data->pdev)
		kobj = &goodix_modules_V2.core_data->pdev->dev.kobj;
	return kobj;
}
EXPORT_SYMBOL_GPL(goodix_get_default_kobj_V2);

/* debug fs */
struct debugfs_buf {
	struct debugfs_blob_wrapper buf;
	int pos;
	struct dentry *dentry;
} goodix_dbg_V2;

void goodix_msg_printf_V2(const char *fmt, ...)
{
	va_list args;
	int r;

	if (goodix_dbg_V2.pos < goodix_dbg_V2.buf.size) {
		va_start(args, fmt);
		r = vscnprintf(goodix_dbg_V2.buf.data + goodix_dbg_V2.pos,
			 goodix_dbg_V2.buf.size - 1, fmt, args);
		goodix_dbg_V2.pos += r;
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(goodix_msg_printf_V2);

static int goodix_debugfs_init(void)
{
	struct dentry *r_b;
	goodix_dbg_V2.buf.size = PAGE_SIZE;
	goodix_dbg_V2.pos = 0;
	goodix_dbg_V2.buf.data = kzalloc(goodix_dbg_V2.buf.size, GFP_KERNEL);
	if (goodix_dbg_V2.buf.data == NULL) {
		VTE("Debugfs init failed\n");
		goto exit;
	}
	r_b = debugfs_create_blob("goodix_ts", 0644, NULL, &goodix_dbg_V2.buf);
	if (!r_b) {
		VTE("Debugfs create failed\n");
		return -ENOENT;
	}
	goodix_dbg_V2.dentry = r_b;

exit:
	return 0;
}

static void goodix_debugfs_exit(void)
{
	debugfs_remove(goodix_dbg_V2.dentry);
	goodix_dbg_V2.dentry = NULL;
	VTI("Debugfs module exit\n");
}

/* show external module infomation */
static ssize_t goodix_ts_extmod_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ext_module *module;
	size_t offset = 0;
	int r;

	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(module, &goodix_modules_V2.head, list) {
			r = snprintf(&buf[offset], PAGE_SIZE,
					"priority:%u module:%s\n",
					module->priority, module->name);
			if (r < 0) {
				mutex_unlock(&goodix_modules_V2.mutex);
				return -EINVAL;
			}
			offset += r;
		}
	}

	mutex_unlock(&goodix_modules_V2.mutex);
	return offset;
}

/* show driver infomation */
static ssize_t goodix_ts_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t goodix_ts_chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_version chip_ver;
	int r, cnt = 0;

	cnt += snprintf(buf, PAGE_SIZE,
			"TouchDeviceName:%s\n", ts_dev->name);
	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"PID:%s\nVID:%02x %02x %02x %02x\nSensorID:%02x\n",
					chip_ver.pid, chip_ver.vid[0],
					chip_ver.vid[1], chip_ver.vid[2],
					chip_ver.vid[3], chip_ver.sensor_id);
		}
	}

	return cnt;
}

/* show chip configuration data */
static ssize_t goodix_ts_config_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_config *ncfg = ts_dev->normal_cfg;
	u8 *data;
	int i, r, offset = 0;

	if (ncfg && ncfg->initialized && ncfg->length < PAGE_SIZE) {
		data = kmalloc(ncfg->length, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		r = ts_dev->hw_ops->read(ts_dev, ncfg->reg_base,
				&data[0], ncfg->length);
		if (r < 0) {
			kfree(data);
			return -EINVAL;
		}

		for (i = 0; i < ncfg->length; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x ", data[i]);
		}
		buf[offset++] = '\n';
		buf[offset++] = '\0';
		kfree(data);
		return offset;
	}

	return -EINVAL;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	if (ts_dev->hw_ops->reset)
		ts_dev->hw_ops->reset(ts_dev);
	return count;

}

static ssize_t goodix_ts_read_cfg_show(struct device *dev,
				struct device_attribute *attr,
						char *buf)
{
	struct goodix_ts_core *core_data =
				dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int ret, i, offset;
	char *cfg_buf;

	cfg_buf = kzalloc(4096, GFP_KERNEL);
	disable_irq(core_data->irq);
	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, cfg_buf, 0);
	else
		ret = -EINVAL;
	enable_irq(core_data->irq);

	offset = 0;
	if (ret > 0) {
		for (i = 0; i < ret; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], 4096 - offset,
					"%02x ", cfg_buf[i]);
		}

	}
	kfree(cfg_buf);
	return ret;
}

static int goodix_ts_convert_0x_data(const u8 *buf,
				int buf_size,
				unsigned char *out_buf,
				int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}
	VTI("***m_size:%d", m_size);

	if (m_size <= 1) {
		VTE("cfg file ERROR, valid data count:%d\n", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X') {
			if (temp_index >= m_size) {
				VTE("exchange cfg data error, overflow, temp_index:%d,m_size:%d\n",
						temp_index, m_size);
				return -EINVAL;
			}
			if (buf[i + 1] >= '0' && buf[i + 1] <= '9')
				out_buf[temp_index] = (buf[i + 1] - '0') << 4;
			else if (buf[i + 1] >= 'a' && buf[i + 1] <= 'f')
				out_buf[temp_index] = (buf[i + 1] - 'a' + 10) << 4;
			else if (buf[i + 1] >= 'A' && buf[i + 1] <= 'F')
				out_buf[temp_index] = (buf[i + 1] - 'A' + 10) << 4;

			if (buf[i + 2] >= '0' && buf[i + 2] <= '9')
				out_buf[temp_index] += (buf[i + 2] - '0');
			else if (buf[i + 2] >= 'a' && buf[i + 2] <= 'f')
				out_buf[temp_index] += (buf[i + 2] - 'a' + 10);
			else if (buf[i + 2] >= 'A' && buf[i + 2] <= 'F')
				out_buf[temp_index] += (buf[i + 2] - 'A' + 10);

			temp_index++;
		}
	}
	return 0;
}



static ssize_t goodix_ts_send_cfg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	struct goodix_ts_core *core_data =
				dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en, r;
	const struct firmware *cfg_img;
	struct goodix_ts_config *config = NULL;

	VTI("******IN");

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	VTI("en:%d", en);

	disable_irq(core_data->irq);

	/*request configuration*/
	r = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (r < 0) {
		VTE("cfg file [%s] not available,errno:%d", GOODIX_DEFAULT_CFG_NAME, r);
		goto exit;
	} else
		VTI("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);

	config = kzalloc(sizeof(struct goodix_ts_config), GFP_KERNEL);
	if (config == NULL) {
		VTE("Memory allco err");
		goto exit;
	}
	/*parse cfg data*/
	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
				config->data, &config->length)) {
		VTE("convert config data FAILED");
		goto exit;
	}

	config->reg_base = ts_dev->reg.cfg_addr;
	config->initialized = true;

	if (ts_dev->hw_ops->send_config)
		ts_dev->hw_ops->send_config(ts_dev, config);

exit:
	enable_irq(core_data->irq);
	if (config) {
		kfree(config);
		config = NULL;
	}

	if (cfg_img) {
		release_firmware(cfg_img);
		cfg_img = NULL;
	}
	
	VTI("******OUT");
	return count;
}

/* show irq infomation */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n",
			core_data->irq);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
		atomic_read(&core_data->irq_enabled) ?
		"enabled" : "disabled");
	if (r < 0)
		return -EINVAL;

	desc = irq_to_desc(core_data->irq);
	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
		desc->depth);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
		core_data->irq_trig_cnt);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset,
			"echo 0/1 > irq_info to disable/enable irq");
	if (r < 0)
		return -EINVAL;

	offset += r;
	return offset;
}

/* enable/disable irq */
static ssize_t goodix_ts_irq_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	goodix_ts_irq_enable_V2(core_data, en);
	return count;
}

static ssize_t goodix_ts_fw_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int opt, ret = count;
	const struct firmware *firmware = NULL;

	if (sscanf(buf, "%d", &opt) != 1)
		return -EINVAL;

	if (1 == opt) {
		ret = request_firmware(&firmware, "goodix_firmware.bin", dev);
		if(ret < 0){
			VTE("request_firmware fail");
			ret = -EINVAL;
			goto exit;
		}
		if (bbk_goodix_fw_update_V2(firmware) < 0) {
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	if(firmware){
		release_firmware(firmware);
		firmware = NULL;
	}
	return ret;
}

static ssize_t goodix_ts_flash_udd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, offset = 0;

	u8 temp_data[15] = {0};

	if (bbk_goodix_readUdd_V2_v1(temp_data))
		return -EINVAL;

	for (i = 0; i < 15; i++) {
		offset += snprintf(&buf[offset], PAGE_SIZE - offset,
				 "%02x ", temp_data[i]);
	}

	buf[offset++] = '\n';
	buf[offset++] = '\0';

	return offset;
}

static ssize_t goodix_ts_flash_udd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int opt = 0, ret = 0;
	unsigned char temp_buf[15] = {8, 6, 6, 3, 6, 1,
					0, 3, 2, 6, 0, 6, 3, 2, 5};

	if (sscanf(buf, "%d", &opt) != 1)
		return -EINVAL;

	if (1 == opt) {
		ret = bbk_goodix_writeUdd_V2_v1(temp_buf);
		if (ret)
			return -EINVAL;
	}

	return count;
}

static ssize_t goodix_ts_version_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int fw_ver = 0, cfg_ver = 0;

	fw_ver = bbk_goodix_get_fw_version_V2_v1(0);
	if (fw_ver == -1)
		return -EINVAL;

	cfg_ver = bbk_goodix_get_fw_version_V2_v1(1);
	if (cfg_ver == -1)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "fw version:%08x, cfg version:%02x\n",
						fw_ver, cfg_ver);
}

static ssize_t goodix_ts_gesture_point_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int i, ret = 0;
	int opt = 0;
	u16 temp_data[12] = {0};

	if (sscanf(buf, "%d", &opt) != 1)
		return -EINVAL;

	if (1 == opt) {
		ret = bbk_goodix_gesture_point_get_V2_v1(temp_data);
		if (-1 == ret)
			return -EINVAL;
	}

	for (i = 0; i < 6; i++) {
		VTI("[%d](%d,%d)", i, temp_data[i * 2],
				temp_data[i * 2 + 1]);
	}

	return count;
}

static ssize_t touchscreen_dclick_lcd_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1) {
		VTI("invalide number of parameters passed.");
		return -EINVAL;
	}

	VTI("parameter is %d", val);
	goodix_modules_V2.core_data->lcd_state = val;
	bbk_goodix_mode_change_V2(!val);

	return count;
}
static ssize_t touchscreen_dclick_lcd_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 255, "has_lcd_shutoff = %d, 0--lcd on, 1---lcd off\n", goodix_modules_V2.core_data->lcd_state);
}

static DEVICE_ATTR(extmod_info, S_IRUGO, goodix_ts_extmod_show, NULL);
static DEVICE_ATTR(driver_info, S_IRUGO, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(config_data, S_IRUGO, goodix_ts_config_data_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, S_IWUSR | S_IWGRP, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, S_IRUGO, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
		goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(bbk_fw_update, S_IWUSR | S_IWGRP,
			NULL, goodix_ts_fw_update_store);
static DEVICE_ATTR(bbk_flash_udd, S_IRUGO | S_IWUSR | S_IWGRP,
		goodix_ts_flash_udd_show, goodix_ts_flash_udd_store);
static DEVICE_ATTR(bbk_version_info, S_IRUGO, goodix_ts_version_info_show, NULL);
static DEVICE_ATTR(bbk_gesture_point, S_IWUSR | S_IWGRP,
			NULL, goodix_ts_gesture_point_store);
static DEVICE_ATTR(dclick_lcd_state, 0644,
			touchscreen_dclick_lcd_state_show, touchscreen_dclick_lcd_state_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_extmod_info.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_config_data.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_bbk_fw_update.attr,
	&dev_attr_bbk_flash_udd.attr,
	&dev_attr_bbk_version_info.attr,
	&dev_attr_bbk_gesture_point.attr,
	&dev_attr_dclick_lcd_state.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * goodix_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_register_notifier_V2(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(goodix_ts_register_notifier_V2);

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier_V2(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(goodix_ts_unregister_notifier_V2);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify_V2(enum ts_notify_event evt, void *v)
{
	return blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
}
EXPORT_SYMBOL_GPL(goodix_ts_blocking_notify_V2);



/**
 * goodix_ts_input_report - report touch event to input subsystem
 *
 * @dev: input device pointer
 * @touch_data: touch data pointer
 * return: 0 ok, <0 failed
 */
static int goodix_ts_input_report(struct goodix_ts_core *core_data,
		struct goodix_touch_data *touch_data)
{
	struct goodix_ts_coords *coords = &touch_data->coords[0];
	struct input_dev *dev = core_data->input_dev;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	unsigned int touch_num = touch_data->touch_num;
	static u32 pre_fin;
	static u8 pre_key;
	int i, id;

	/*first touch down and last touch up condition*/
	if (touch_num != 0 && pre_fin == 0x0000) {
		/*first touch down event*/
		input_report_key(dev, BTN_TOUCH, 1);
		core_data->ts_dev->int_state = true;
	} else if (touch_num == 0 && pre_fin != 0x0000) {
		/*no finger exist*/
		input_report_key(dev, BTN_TOUCH, 0);
		core_data->ts_dev->int_state = false;
	}

	/*report key, include tp's key and pen's key */
	if (unlikely(touch_data->have_key)) {
		for (i = 0; i < ts_bdata->panel_max_key; i++) {
			input_report_key(dev, ts_bdata->panel_key_map[i],
							touch_data->key_value & (1 << i));
		}
		pre_key = touch_data->key_value;
		/*VTI("$$$$$$pre_key:0x%02x",pre_key);*/
	} else if (pre_key != 0x00) {
		/*VTI("******no key, by pre_key is not ZERO! pre_key:0x%02x", pre_key);*/
		for (i = 0; i < ts_bdata->panel_max_key; i++) {
			if (pre_key & (1 << i)) {
				input_report_key(dev, ts_bdata->panel_key_map[i], 0);
				pre_key &= ~(1 << i);
				/*VTI("******report i:%d, key:%d leave", i, ts_bdata->panel_key_map[i]);*/
			}
		}
		/*VTI("******after, pre_key:0x%02x", pre_key);*/
	}

#if 1
	/*protocol B*/

	/*report pen*/
	if (touch_num >= 1 && touch_data->pen_down) {
		touch_num -= 1;

		input_mt_slot(dev, ts_bdata->panel_max_id * 2);
		input_report_abs(dev, ABS_MT_TRACKING_ID, touch_data->pen_coords[0].id);
		input_report_abs(dev, ABS_MT_TOOL_TYPE, MT_TOOL_PEN);


		input_report_abs(dev, ABS_MT_POSITION_X, touch_data->pen_coords[0].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, touch_data->pen_coords[0].y);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, touch_data->pen_coords[0].w);
		input_report_abs(dev, ABS_MT_PRESSURE, touch_data->pen_coords[0].p);

		pre_fin |= 1 << 20;
		VTD("!!!!!!report pen  DOWN,%d,%d,%d",
				touch_data->pen_coords[0].x,
				touch_data->pen_coords[0].y,
				touch_data->pen_coords[0].p);
	} else {
		if (pre_fin & (1 << 20)) {
			input_mt_slot(dev, ts_bdata->panel_max_id * 2);
			input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
			pre_fin &= ~(1 << 20);
			VTI("!!!!!!report pen LEAVE");
		}
	}

	/*report finger*/
	/*report up*/
	id = coords->id;
	for (i = 0; i < ts_bdata->panel_max_id * 2; i++){
		if(touch_num && i == id){
			id = (++coords)->id;
		}else if (pre_fin & (1 << i)) {/* release touch */
			input_mt_slot(dev, i);
			input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, 0);
			input_report_key(dev, BTN_TOOL_FINGER, 0);
			input_report_key(dev, BTN_TOUCH, 0);
			//vivoTsInputReport(VTS_TOUCH_UP, i, 0, 0, 0);
			pre_fin &= ~(1 << i);
			VTI("report leave:%d", i);
		}
	}

	/*report down*/
	coords = &touch_data->coords[0];
	id = coords->id;
	for (i = 0; i < ts_bdata->panel_max_id * 2; i++) {
		if (touch_num && i == id) { /* this is a valid touch down event */

			input_mt_slot(dev, id);
			input_report_abs(dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(dev, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
			input_report_key(dev, BTN_TOOL_FINGER, 1);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
			input_report_abs(dev, ABS_MT_POSITION_X, coords->x);
			input_report_abs(dev, ABS_MT_POSITION_Y, coords->y);
			input_report_abs(dev, ABS_MT_TOUCH_MAJOR, coords->w);
			input_report_abs(dev, ABS_MT_PRESSURE, coords->p);

			VTI("[%d %04d %04d]down[%d][%d]\n", id, coords->x, coords->y, coords->w, coords->p);
			//vivoTsInputReport(VTS_TOUCH_DOWN, id, coords->x, coords->y, coords->w);
			core_data->fingerPressRecord[id][0] = coords->x;
			core_data->fingerPressRecord[id][1] = coords->y;
			core_data->fingerPressRecord[id][2] = coords->w;
			core_data->fingerPressRecord[id][3] = coords->p;
			pre_fin |= 1 << i;
			id = (++coords)->id;
		}
	}
#endif

#if 0
	/*report pen use protocl A*/
	if (touch_data->pen_down) {
		/*input_report_key(dev, BTN_TOOL_PEN, 1);*/
		/*input_report_key(dev, BTN_TOUCH, 1);*/

		input_report_abs(dev, ABS_MT_POSITION_X, touch_data->pen_coords[0].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, touch_data->pen_coords[0].y);

		input_report_abs(dev, ABS_MT_PRESSURE, touch_data->pen_coords[0].p);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, touch_data->pen_coords[0].w);
		/*input_report_abs(dev, ABS_MT_TRACKING_ID, touch_data->pen_coords[0].id);*/
		input_report_abs(dev, ABS_MT_TOOL_TYPE, 1);

		input_mt_sync(dev);
	} else {
		if (pre_fin & (1 << 10) && touch_num == 0) {
			/*input_report_key(dev, BTN_TOOL_PEN, 0);*/
			/*input_report_key(dev, BTN_TOUCH, 0);*/

			pre_fin &= ~(1 << 10);
		}
	}

	/* report abs */
	id = coords->id;
	for (i = 0; i < ts_bdata->panel_max_id; i++) {
		if (touch_num && i == id) { /* this is a valid touch down event */

			/*input_report_key(dev, BTN_TOUCH, 1);*/
			/*input_report_abs(dev, ABS_MT_TRACKING_ID, id);*/

			input_report_abs(dev, ABS_MT_POSITION_X, coords->x);
			input_report_abs(dev, ABS_MT_POSITION_Y, coords->y);
			input_report_abs(dev, ABS_MT_TOUCH_MAJOR, coords->w);
			input_report_abs(dev, ABS_MT_PRESSURE, coords->p);
			input_mt_sync(dev);

			pre_fin |= 1 << i;
			id = (++coords)->id;
		} else {
			if (pre_fin & (1 << i)) {
				/*input_mt_sync(dev);*/

				pre_fin &= ~(1 << i);
			}
		}
	}
#endif
	//vts_report_point_sync(vtsdev);
	input_sync(dev);
	return 0;
}

struct goodix_ts_core *irq_core_data_V2;

/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static void goodix_ts_threadirq_func(struct goodix_ts_core *data)
{
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	//struct goodix_ext_module *ext_module;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	int r;

	core_data->irq_trig_cnt++;
	/* inform external module 
	list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
		if (!ext_module->funcs || !ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		if (r == EVT_CANCEL_IRQEVT)
			//return IRQ_HANDLED;
			return;
	}*/

	/* read touch data from touch device */
	r = ts_dev->hw_ops->event_handler(core_data, ts_dev, ts_event);
	if (likely(r >= 0)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			/* report touch */
			goodix_ts_input_report(core_data,
					&ts_event->event_data.touch_data);
		}
	}

	//return IRQ_HANDLED;
	return;
	
}

static irqreturn_t goodix_enter_irq(int irq, void *data)
{
	irq_core_data_V2 = (struct goodix_ts_core *)data;

	goodix_ts_threadirq_func(irq_core_data_V2);

	return IRQ_HANDLED;
}


/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_setup_V2(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r;

	/* if ts_bdata-> irq is invalid */
	if (ts_bdata->irq <= 0) 
		core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	else 
		core_data->irq = ts_bdata->irq;

	VTI("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
	r = request_threaded_irq(
			core_data->irq, NULL,
			goodix_enter_irq,
			ts_bdata->irq_flags | IRQF_ONESHOT,
			GOODIX_CORE_DRIVER_NAME,
			core_data);

	if (r < 0)
		VTE("Failed to requeset threaded irq:%d", r);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return r;
}

/**
 * goodix_ts_irq_enable_V2 - Enable/Disable a irq
 * @core_data: pointer to touch core data
 * enable: enable or disable irq
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_enable_V2(struct goodix_ts_core *core_data,
			bool enable)
{
	if (enable) {
		if (!atomic_cmpxchg(&core_data->irq_enabled, 0, 1)) {
			enable_irq(core_data->irq);
			VTD("Irq enabled");
		}
	} else {
		if (atomic_cmpxchg(&core_data->irq_enabled, 1, 0)) {
			disable_irq(core_data->irq);
			VTD("Irq disabled");
		}
	}

	return 0;
}
EXPORT_SYMBOL(goodix_ts_irq_enable_V2);
/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
	struct device *dev = NULL;
	struct goodix_ts_board_data *ts_bdata;
	int r = 0;

	VTI("Power init");

	/* dev:i2c client device or spi slave device*/
	dev =  core_data->ts_dev->dev;
	ts_bdata = board_data(core_data);

	if (gpio_is_valid(ts_bdata->power_gpio)) {
		r = gpio_request(ts_bdata->power_gpio, GOODIX_CORE_DRIVER_NAME);
		if (r) {
			VTE("gpio request fail!");
			return r;
		}
	} else {
		if (ts_bdata->avdd_name) {
			core_data->avdd = devm_regulator_get(dev,
				 	ts_bdata->avdd_name);
			if (IS_ERR_OR_NULL(core_data->avdd)) {
				r = PTR_ERR(core_data->avdd);
				VTE("Failed to get regulator avdd:%d", r);
				core_data->avdd = NULL;
				return r;
			}
		}
	}
	
	if (ts_bdata->dvdd_name) {
		core_data->dvdd = devm_regulator_get(dev,
				 ts_bdata->dvdd_name);
		if (IS_ERR_OR_NULL(core_data->dvdd)) {
			r = PTR_ERR(core_data->dvdd);
			VTE("Failed to get regulator dvdd:%d", r);
			core_data->dvdd = NULL;
			return r;
		}
	} else {
		if (gpio_is_valid(ts_bdata->dvdd_gpio)) {
			r = gpio_request(ts_bdata->dvdd_gpio, GOODIX_CORE_DRIVER_NAME);
			if (r) {
				VTE("dvdd_gpio request failed!");
				return r;
			}
		}
	}

	return r;
}

/**
 * goodix_ts_power_on_V2 - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on_V2(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata =
			board_data(core_data);
	int r;

	VTI("Device power on");
	if (core_data->power_on)
		return 0;

	if (gpio_is_valid(ts_bdata->power_gpio)) {		
		r = gpio_direction_output(ts_bdata->power_gpio, 1); /* 3V */
		if (r) {
			VTE("unable to set direction out to 1 for gpio [%d]\n", ts_bdata->power_gpio);
		}

	} else {
		if (regulator_count_voltages(core_data->avdd) > 0) {
			r = regulator_set_voltage(core_data->avdd, 3000000,
								3000000);
			if (r) {
				VTI(" avdd regulator set_vtg:bus_reg vcc_ana failed r=%d", r);
				regulator_put(core_data->avdd);
				return r;
			}
		}
		r = regulator_enable(core_data->avdd);
		if (r) {
			VTI("Regulator avdd enable failed r=%d", r);
			return r;
		}

	}

	if (core_data->dvdd) {
		r = regulator_enable(core_data->dvdd);
		if (!r) {
			VTI("regulator enable SUCCESS");
			if (ts_bdata->power_on_delay_us)
				usleep_range(ts_bdata->power_on_delay_us,
						ts_bdata->power_on_delay_us);
		} 
	}else {
			if (gpio_is_valid(ts_bdata->dvdd_gpio)) {
				r = gpio_direction_output(ts_bdata->dvdd_gpio, 1);
				if (r) {
					VTE("unable to set direction out to 1 for gpio [%d]\n", ts_bdata->dvdd_gpio);
				}else {
					VTI("regulator dvdd_gpio enable SUCCESS");
					if (ts_bdata->power_on_delay_us)
						usleep_range(ts_bdata->power_on_delay_us,
							ts_bdata->power_on_delay_us);
				}
			}
		}
	
	/* set reset gpio high */
	if(gpio_is_valid(ts_bdata->reset_gpio)){
		VTI("reset high++++++++++");
		usleep_range(100, 110);
		gpio_direction_output(ts_bdata->reset_gpio, 1);
	}

	core_data->power_on = 1;
	return 0;
}

/**
 * goodix_ts_power_off_V2 - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off_V2(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata =
			board_data(core_data);
	int r;

	VTI("Device power off");
	if (!core_data->power_on)
		return 0;
	
	if(gpio_is_valid(ts_bdata->reset_gpio)){
		/* set reset gpio low */
		VTI("reset low---------------------");
		gpio_direction_output(ts_bdata->reset_gpio, 0);
		usleep_range(100, 110);
	}
	
	if (gpio_is_valid(ts_bdata->power_gpio)) {		
		r = gpio_direction_output(ts_bdata->power_gpio, 0); /*3V */
		if (r) {
			VTE("unable to set direction out to 1 for gpio [%d]\n", ts_bdata->power_gpio);
		}

	} else {

		r = regulator_disable(core_data->avdd);
		if (!r) {
			VTI(" avdd regulator disable SUCCESS");

		}
		else {
			VTI(" avdd regulator disable failed");
			return r;
		}
	}

	if (core_data->dvdd) {
		r = regulator_disable(core_data->dvdd);
		if (!r) {
			VTI("regulator disable SUCCESS");
			if (ts_bdata->power_off_delay_us)
				usleep_range(ts_bdata->power_off_delay_us,
						ts_bdata->power_off_delay_us);
		} 
	}else {
			if (gpio_is_valid(ts_bdata->dvdd_gpio)) {
				r = gpio_direction_output(ts_bdata->dvdd_gpio, 0); /*1.8V */
				if (r) {
					VTE("unable to set direction out to 0 for dvdd_gpio [%d]\n", ts_bdata->dvdd_gpio);
				}
		  }
	 }


	core_data->power_on = 0;
	return 0;
}

#ifdef CONFIG_PINCTRL
/**
 * goodix_ts_pinctrl_init - Get pinctrl handler and pinctrl_state
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_pinctrl_init(struct goodix_ts_core *core_data)
{
	int r = 0;

	/* get pinctrl handler from of node */
	core_data->pinctrl = devm_pinctrl_get(core_data->ts_dev->dev);
    //VTI("-----pinctrl device----%s",core_data->ts_dev->dev);
	if (IS_ERR_OR_NULL(core_data->pinctrl)) {
		VTE("Failed to get pinctrl handler");
		return PTR_ERR(core_data->pinctrl);
	}

	/* active state */
	core_data->pin_sta_active = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_sta_active)) {
		r = PTR_ERR(core_data->pin_sta_active);
		VTE("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_ACTIVE, r);
		goto exit_pinctrl_put;
	}

	/* suspend state */
	core_data->pin_sta_suspend = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_sta_suspend)) {
		r = PTR_ERR(core_data->pin_sta_suspend);
		VTE("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_SUSPEND, r);
		goto exit_pinctrl_put;
	}

	return 0;
exit_pinctrl_put:
	devm_pinctrl_put(core_data->pinctrl);
	core_data->pinctrl = NULL;
	return r;
}
#endif

/**
 * goodix_ts_gpio_setup - Request gpio resources from GPIO subsysten
 *	reset_gpio and irq_gpio number are obtained from goodix_ts_device
 *  which created in hardware layer driver. e.g.goodix_xx_i2c.c
 *	A goodix_ts_device should set those two fileds to right value
 *	before registed to touch core driver.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_gpio_setup(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata =
			board_data(core_data);
	int r = 0;

	VTI("GPIO setup,reset-gpio:%d, irq-gpio:%d",
	ts_bdata->reset_gpio, ts_bdata->irq_gpio);
	/*
	 * after kenerl3.13, gpio_ api is deprecated, new
	 * driver should use gpiod_ api.
	 */
	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->reset_gpio,
			GPIOF_OUT_INIT_LOW,
			"ts_reset_gpio");
	if (r < 0) {
		VTE("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->irq_gpio,
			GPIOF_IN,
			"ts_irq_gpio");
	if (r < 0) {
		VTE("Failed to request irq gpio, r:%d", r);
		return r;
	}

	return 0;
}


/**
 * goodix_input_set_params - set input parameters
 */
static void goodix_ts_set_input_params(struct input_dev *input_dev,
		struct goodix_ts_board_data *ts_bdata)
{
	int i;

	if (ts_bdata->swap_axis)
		swap(ts_bdata->panel_max_x, ts_bdata->panel_max_y);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,
			0, ts_bdata->panel_max_id, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			0, ts_bdata->panel_max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			0, ts_bdata->panel_max_p, 0, 0);

	if (ts_bdata->panel_max_key) {
		for (i = 0; i < ts_bdata->panel_max_key; i++)
			input_set_capability(input_dev, EV_KEY,
					ts_bdata->panel_key_map[i]);
	}
}

/**
 * goodix_ts_input_dev_config_V2 - Requset and config a input device
 *  then register it to input sybsystem.
 *  NOTE that some hardware layer may provide a input device
 *  (ts_dev->input_dev not NULL).
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_input_dev_config_V2(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *input_dev = NULL;
	int r;

	input_dev = devm_input_allocate_device(&core_data->pdev->dev);
	if (!input_dev) {
		VTE("Failed to allocated input device");
		return -ENOMEM;
	}

	core_data->input_dev = input_dev;
	input_set_drvdata(input_dev, core_data);

	input_dev->name = GOODIX_CORE_DRIVER_NAME;
	input_dev->phys = GOOIDX_INPUT_PHYS;
	input_dev->id.product = 0xDEAD;
	input_dev->id.vendor = 0xBEEF;
	input_dev->id.version = 10427;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* set input parameters */
	goodix_ts_set_input_params(input_dev, ts_bdata);

	/*set ABS_MT_TOOL_TYPE*/
	input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
						0, 1, 0, 0);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	/*input_mt_init_slots(input_dev, ts_bdata->panel_max_id,
			INPUT_MT_DIRECT);*/
	input_mt_init_slots(input_dev,
			ts_bdata->panel_max_id * 2 + 1,
			INPUT_MT_DIRECT);
#else
	/*input_mt_init_slots(input_dev, ts_bdata->panel_max_id);*/
	input_mt_init_slots(input_dev,
			ts_bdata->panel_max_id * 2 + 1);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_POWER);

	r = input_register_device(input_dev);
	if (r < 0) {
		VTE("Unable to register input device");
		return r;
	}

	return 0;
}

/**
 * goodix_ts_hw_init_V2 - Hardware initilize
 *  poweron - hardware reset - sendconfig
 * @core_data: pointer to touch core data
 * return: 0 intilize ok, <0 failed
 */
int goodix_ts_hw_init_V2(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_hw_ops *hw_ops =
		ts_hw_ops(core_data);
	int r;

	/* reset touch device */
	if (hw_ops->reset) {
		r = hw_ops->reset(core_data->ts_dev);
		if (r < 0)
			goto exit;
	}

	/* init */
	if (hw_ops->init) {
		r = hw_ops->init(core_data->ts_dev);
		if (r < 0) {
			core_data->hw_err = true;
			goto exit;
		}
	}

exit:
	/* if bus communication error occured then
	 * exit driver binding, other errors will
	 * be ignored */
	if (r != -EBUS)
		r = 0;
	return r;
}

static void goodix_update_firmware_work(struct work_struct *work)
{
	int r;
	const struct firmware *firmware;
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;

	VTI("xiekui goodix_update_firmware_work in");
	r = request_firmware(&firmware, "TP-FW-PD2038-LCMID32-VER0x606030022.bin", core_data->ts_dev->dev);
	if(r < 0){
		VTE("request_firmware fail");
		return;
	}
	goodix_cfg_bin_proc_V2(core_data, firmware);
}

/**
 * goodix_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void goodix_ts_esd_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct goodix_ts_esd *ts_esd = container_of(dwork,
			struct goodix_ts_esd, esd_work);
	struct goodix_ts_core *core = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(core);
	int r = 0;
	u8 data = 0xaa;

	if (ts_esd->esd_on == false)
		return;

	VTD("goodix_ts_esd_work function is running");

	if (hw_ops->check_hw)
		r = hw_ops->check_hw(core->ts_dev);
	if (r < 0) {
		goodix_ts_power_off_V2(core);
		msleep(100);
		goodix_ts_power_on_V2(core);
		if (hw_ops->reset)
			hw_ops->reset(core->ts_dev);

		/*init static esd*/
		if (core->ts_dev->ic_type == IC_TYPE_NANJING) {
			r = hw_ops->write(core->ts_dev,
					0x8043, &data, 1);
			if (r < 0)
				VTE("nanjing esd reset, init static esd FAILED, i2c wirte ERROR");
		}

		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			VTE("esd reset, init dynamic esd FAILED, i2c write ERROR");

	} else {
		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			VTE("esd init watch dog FAILED, i2c write ERROR");
	}

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on)
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	if (core->ts_dev->reg.esd == 0)
		return;

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on == false) {
		ts_esd->esd_on = true;
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
		mutex_unlock(&ts_esd->esd_mutex);
		VTI("Esd on");
		return;
	}
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on == true) {
		ts_esd->esd_on = false;
		cancel_delayed_work(&ts_esd->esd_work);
		mutex_unlock(&ts_esd->esd_mutex);
		VTI("Esd off");
		return;
	}
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int goodix_esd_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct goodix_ts_esd *ts_esd = container_of(nb,
			struct goodix_ts_esd, esd_notifier);

	switch (action) {
	case NOTIFY_FWUPDATE_START:
	case NOTIFY_SUSPEND:
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_END:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts_core);
		break;
	}

	return 0;
}

/**
 * goodix_ts_esd_init_V2 - initialize esd protection
 */
int goodix_ts_esd_init_V2(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;
	struct goodix_ts_device *dev = core->ts_dev;
	u8 data = 0xaa;
	int r;

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	mutex_init(&ts_esd->esd_mutex);
	ts_esd->ts_core = core;
	ts_esd->esd_on = false;
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier_V2(&ts_esd->esd_notifier);

	if (core->ts_dev->board_data->esd_default_on &&
			dev->hw_ops->check_hw &&
			dev->reg.esd != 0) {
		/*init static esd*/
		if (dev->ic_type == IC_TYPE_NANJING) {
			r = dev->hw_ops->write_trans(core->ts_dev,
				0x8043, &data, 1);
			if (r < 0)
				VTE("static ESD init ERROR, i2c write failed");
		}

		/*init dynamic esd*/
		r = dev->hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			VTE("dynamic ESD init ERROR, i2c write failed");

		goodix_ts_esd_on(core);
	}
	return 0;
}

/**
 * goodix_ts_suspend_V2 - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
int goodix_ts_suspend_V2(struct goodix_ts_core *core_data, int is_sleep)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int r;

	VTI("Suspend start");

	/*
	 * notify suspend event, inform the esd protector
	 * and charger detector to turn off the work
	 */
	goodix_ts_blocking_notify_V2(NOTIFY_SUSPEND, NULL);

	/* inform external module */
	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (!ext_module->funcs || !ext_module->funcs->before_suspend)
				continue;

			if (is_sleep) {
				if (strcmp(ext_module->name, "Goodix_gsx_gesture_V2") == 0)
					continue;
			}

			r = ext_module->funcs->before_suspend(core_data, ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules_V2.mutex);
				VTI("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules_V2.mutex);

	/* disable irq */
	goodix_ts_irq_enable_V2(core_data, false);

	/* let touch ic work in sleep mode */
	if (ts_dev && ts_dev->hw_ops->suspend)
		ts_dev->hw_ops->suspend(ts_dev);
	atomic_set(&core_data->suspended, 1);
	atomic_set(&core_data->gestured, 0);

#ifdef CONFIG_PINCTRL
if(core_data->power_off_sleep){//only when power off in sleep ,should select suspend pinctrl state  
	if (core_data->pinctrl && core_data->pin_sta_suspend) {
		VTI("select suspend pinstate");
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_sta_suspend);
		if (r < 0)
			VTE("Failed to select active pinstate, r:%d", r);
	}
}
#endif

	/* inform exteranl modules */
	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (!ext_module->funcs || !ext_module->funcs->after_suspend)
				continue;

			r = ext_module->funcs->after_suspend(core_data, ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules_V2.mutex);
				VTI("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules_V2.mutex);

out:
	/* release all the touch IDs */
	core_data->ts_event.event_data.touch_data.touch_num = 0;
	goodix_ts_input_report(core_data,
			&core_data->ts_event.event_data.touch_data);
	VTI("Suspend end");
	return 0;
}

/**
 * goodix_ts_resume_V2 - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
int goodix_ts_resume_V2(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev =
				core_data->ts_dev;
	int r;

	VTI("Resume start");
	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (!ext_module->funcs || !ext_module->funcs->before_resume)
				continue;

			r = ext_module->funcs->before_resume(core_data, ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules_V2.mutex);
				VTI("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules_V2.mutex);

#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl && core_data->pin_sta_active) {
		VTI("select active pinstate");
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			VTE("Failed to select active pinstate, r:%d", r);
	}
#endif

	atomic_set(&core_data->suspended, 0);
	atomic_set(&core_data->gestured, 0);

	/* resume device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (!ext_module->funcs || !ext_module->funcs->after_resume)
				continue;

			r = ext_module->funcs->after_resume(core_data, ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules_V2.mutex);
				VTI("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules_V2.mutex);
	if(ts_dev)
	ts_dev->goodix_sensor_test = 0;
	goodix_ts_irq_enable_V2(core_data, true);
out:
	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	goodix_ts_blocking_notify_V2(NOTIFY_RESUME, NULL);
	VTD("Resume end");
	return 0;
}

#ifdef CONFIG_FB
/**
 * goodix_ts_fb_notifier_callback_V2 - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int goodix_ts_fb_notifier_callback_V2(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct fb_event *fb_event = data;

	if (fb_event && fb_event->data && core_data) {
		if (event == FB_EARLY_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == FB_EVENT_BLANK) {
			int *blank = fb_event->data;
			if (*blank == FB_BLANK_UNBLANK)
				goodix_ts_resume_V2(core_data);
			else if (*blank == FB_BLANK_POWERDOWN)
				goodix_ts_suspend_V2(core_data, 1);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
 * goodix_ts_earlysuspend - Early suspend function
 * Called by kernel during system suspend phrase
 */
static void goodix_ts_earlysuspend(struct early_suspend *h)
{
	struct goodix_ts_core *core_data =
		container_of(h, struct goodix_ts_core,
			 early_suspend);

	goodix_ts_suspend_V2(core_data, 1);
}
/**
 * goodix_ts_lateresume - Late resume function
 * Called by kernel during system wakeup
 */
static void goodix_ts_lateresume(struct early_suspend *h)
{
	struct goodix_ts_core *core_data =
		container_of(h, struct goodix_ts_core,
			 early_suspend);

	goodix_ts_resume_V2(core_data);
}
#endif

#ifdef CONFIG_PM
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_suspend_V2(core_data, 1);
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_resume_V2(core_data);
}
#endif
#endif

/**
 * goodix_generic_noti_callback_V2 - generic notifier callback
 *  for goodix touch notification event.
 */
int goodix_generic_noti_callback_V2(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *ts_core = container_of(self,
			struct goodix_ts_core, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(ts_core);
	int r;

	switch (action) {
	case NOTIFY_FWUPDATE_END:
		if (ts_core->hw_err && hw_ops->init) {
			/* Firmware has been updated, we need to reinit
			 * the chip, read the sensor ID and send the
			 * correct config data based on sensor ID.
			 * The input parameters also needs to be updated.*/
			r = hw_ops->init(ts_core->ts_dev);
			if (r < 0)
				goto exit;

			goodix_ts_set_input_params(ts_core->input_dev,
					ts_core->ts_dev->board_data);
			ts_core->hw_err = false;
		}
		break;
	}

exit:
	return 0;

}


extern int touch_state;
extern void bbk_goodix_cmds_init_V2(struct goodix_ts_cmd *ts_cmd,
					     u8 cmds, u8 cmd_data, u32 reg_addr);

int get_faceDect_state(void) {
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;
	VTI("22 enter in get_faceDect_state");
	return core_data->face_detect_cmd;
}

#define GOODIX_FW_STATE_ADDR          0x3101
#define GOODIX_FW_STATE_ADDR_SPI      0x3F01

#define GOODIX_FW_STATE_BIT           (0x01 << 3)

#define FP_STATE_ADDR      0x3102
#define FP_STATE_ADDR_SPI  0x30f2
#define  GT9886_TP_TX_BIT	0x6EA0
#define  GT9886_TP_RX_BIT	0x6EA2
#define  GT9886_TX_BYTE_NUM	2
#define  GT9886_RX_BYTE_NUM	5

u8 get_bit_value(u8 *buf, u8 bit_num)
{
	u8 ret = 0;
	ret = (*buf & (0x01<<bit_num));
	return ret;
}

/**
 * goodix_ts_probe - called by kernel when a Goodix touch
 *  platform driver is added.
 */

static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_ts_device *ts_device;
	struct kobject *kobj;
	int r;
	int i2c_read_max_size =0;
	u8 read_val = 0;
	VTI("goodix_ts_probe IN======1");

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops ||
			!ts_device->board_data) {
		VTE("Invalid touch device");
		return -ENODEV;
	}

	VTI("goodix_ts_probe IN======2");
	core_data = devm_kzalloc(&pdev->dev, sizeof(struct goodix_ts_core),
						GFP_KERNEL);
	if (!core_data) {
		VTE("Failed to allocate memory for core data");
		return -ENOMEM;
	}
	VTI("goodix_ts_probe IN======3");
	/*init i2c_set_doze_mode para*/
	ts_device->doze_mode_set_count = 0;
	mutex_init(&ts_device->doze_mode_lock);
	/* i2c reset mutex */
	mutex_init(&ts_device->i2c_reset_mutex);
	/*i2c access mutex */
	mutex_init(&ts_device->i2c_access_mutex);
	
	/* touch core layer is a platform driver */
	ts_device->goodix_sensor_test = 0;
	core_data->pdev = pdev;
	core_data->ts_dev = ts_device;
	platform_set_drvdata(pdev, core_data);
	//input_set_drvdata(core_data->input_dev, core_data);
	core_data->cfg_group_parsed = false;
	core_data->charge_sta = false;
	core_data->power_off_sleep = false;
	goodix_modules_V2.core_data = core_data;
	
	VTI("goodix_ts_probe IN======4");
	r = goodix_ts_power_init(core_data);
	if (r < 0)
		goto err_free_vts;//debug 
	VTI("goodix_ts_probe IN======5");
	/* get GPIO resource */
	r = goodix_ts_gpio_setup(core_data);
	if (r < 0)
		goto err_free_vts;

	r = goodix_ts_power_on_V2(core_data);
	if (r < 0)
		goto err_free_vts;//debug

#ifdef CONFIG_PINCTRL
	/* Pinctrl handle is optional. */
	r = goodix_ts_pinctrl_init(core_data);
	if (!r && core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			VTE("Failed to select active pinstate, r:%d", r);
	}
#endif

	/*create sysfs files*/
	kobj = kobject_create_and_add("touchscreen", NULL);
	if (kobj == NULL) {
		r = -ENOMEM;
		VTE("add object fail!");
		goto err_free_vts;
	}
	sysfs_create_group(kobj, &sysfs_group);
	VTI("goodix_ts_probe IN======6");

	goodix_modules_V2.core_data->i2c_addr_buf = kzalloc(GOODIX_I2C_ADDR_BUFFER_SIZE, GFP_DMA|GFP_KERNEL);
	if (!goodix_modules_V2.core_data->i2c_addr_buf) {
		VTE("Failed to allocate memory for core data1 !");
		r = -ENOMEM;
		goto err_free_vts;
	}
	goodix_modules_V2.core_data->i2c_get_buf = kzalloc(GOODIX_I2C_GET_BUFFER_SIZE, GFP_DMA|GFP_KERNEL);
	if (!goodix_modules_V2.core_data->i2c_get_buf) {
		VTE("Failed to allocate memory for core data2 !");
		r = -ENOMEM;
		goto err_free_addr_buf;
	}
	goodix_modules_V2.core_data->i2c_put_buf = kzalloc(GOODIX_I2C_PUT_BUFFER_SIZE, GFP_DMA|GFP_KERNEL);
	if (!goodix_modules_V2.core_data->i2c_put_buf) {
		VTE("Failed to allocate memory for core data3 !");
		r = -ENOMEM;
		goto err_free_get_buf;
	}
	VTI("goodix_ts_probe IN======7");

	i2c_read_max_size = ts_device->board_data->i2c_read_max_size;
	goodix_modules_V2.core_data->i2c_max_buf = kzalloc(i2c_read_max_size, GFP_DMA|GFP_KERNEL);
	if (!goodix_modules_V2.core_data->i2c_max_buf) {
		VTE("Failed to allocate memory for core data4 !");
		r = -ENOMEM;
		goto err_free_i2c_read;
	}

	if(core_data->ts_dev->hw_ops && core_data->ts_dev->hw_ops->read_trans)
		r = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, 0x3100, &read_val, 1);
	else
		r = -1;
	//453c  452c
	if (0 == r) {
		VTI("***read firmware_version SUCCESS");
		VTI("30f0:%02X",read_val);
	} else {
		VTE("***read firmware_version FAILED");
		goto err_i2c;
	}

	goodix_modules_V2.core_exit = false;
	complete_all(&goodix_modules_V2.core_comp);

	VTI("goodix_ts_probe complete");
	INIT_DELAYED_WORK(&core_data->work, goodix_update_firmware_work);
	schedule_delayed_work(&core_data->work, 2 * HZ);

	return r;

err_i2c:
	kfree(goodix_modules_V2.core_data->i2c_put_buf);//free 
	goodix_modules_V2.core_data->i2c_put_buf = NULL;

err_free_i2c_read:
	kfree(goodix_modules_V2.core_data->i2c_max_buf);//free 
	goodix_modules_V2.core_data->i2c_max_buf = NULL;
err_free_get_buf:
	kfree(goodix_modules_V2.core_data->i2c_get_buf);//free 
	goodix_modules_V2.core_data->i2c_get_buf = NULL;

err_free_addr_buf:
	kfree(goodix_modules_V2.core_data->i2c_addr_buf);//free 
	goodix_modules_V2.core_data->i2c_addr_buf = NULL;	

err_free_vts:
	devm_kfree(&pdev->dev,core_data);

	return r;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	if (goodix_fwu_module_V2.priv_data) {
		kfree(goodix_fwu_module_V2.priv_data);
		goodix_fwu_module_V2.priv_data = NULL;
	}
	if (!goodix_modules_V2.core_data->i2c_put_buf) {
		kfree(goodix_modules_V2.core_data->i2c_put_buf);
		goodix_modules_V2.core_data->i2c_put_buf = NULL;
	}
	if (!goodix_modules_V2.core_data->i2c_get_buf) {
		kfree(goodix_modules_V2.core_data->i2c_get_buf);
		goodix_modules_V2.core_data->i2c_get_buf = NULL;
	}
	if (!goodix_modules_V2.core_data->i2c_addr_buf) {
		kfree(goodix_modules_V2.core_data->i2c_addr_buf);
		goodix_modules_V2.core_data->i2c_addr_buf = NULL;
	}
	if(goodix_modules_V2.core_data->i2c_max_buf) {
		kfree(goodix_modules_V2.core_data->i2c_max_buf);
		goodix_modules_V2.core_data->i2c_max_buf = NULL;
	}
	goodix_ts_power_off_V2(core_data);
	goodix_debugfs_exit();
	goodix_ts_sysfs_exit(core_data);
	//free vts & core_data
	devm_kfree(&pdev->dev,core_data);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
#endif
};
#endif

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &dev_pm_ops,
#endif
	},
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = ts_core_ids,
};


int goodix_ts_core_V2_init(void)
{
	if (!goodix_modules_V2.initilized) {
		goodix_modules_V2.initilized = true;
		goodix_modules_V2.core_exit = true;
		INIT_LIST_HEAD(&goodix_modules_V2.head);
		mutex_init(&goodix_modules_V2.mutex);
		init_completion(&goodix_modules_V2.core_comp);
	}

	goodix_debugfs_init();
	return platform_driver_register(&goodix_ts_driver);
}

void goodix_ts_core_V2_exit(void)
{
	VTI("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
	return;
}
