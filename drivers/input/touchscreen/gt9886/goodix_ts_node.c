#include "goodix_ts_core.h"

#define FW_SUBSYS_MAX_NUM			28
#define BBK_USER_DATA_MAX_LEN			30
#define BBK_USER_DATA_ADDR			0xBDB4
#define BBK_SENSOR_NUM_ADDR			0x5473
#define BBK_DRIVER_GROUP_A_NUM_ADDR		0x5477
#define BBK_DRIVER_GROUP_B_NUM_ADDR		0x5478
#define BBK_READ_COOR_ADDR			0x4100
#define BBK_RAW_DATA_BASE_ADDR			0x8FA0
#define BBK_DIFF_DATA_BASE_ADDR			0x9D20
#define BBK_REF_DATA_BASE_ADDR			0xA980
#define BBK_CMD_ADDR				0x3101
#define BBK_CMD_ADDR_SPI    		0x30F1 

#define GSX_KEY_DATA_LEN			37
#define GSX_BUFFER_DATA_LEN			513

struct fw_subsys_info {
	u8 type;
	u32 size;
	u16 flash_addr;
	const u8 *data;
};

struct firmware_info {
	u32 size;
	u16 checksum;
	u8 hw_pid[6];
	u8 hw_vid[3];
	u8 fw_pid[8];
	u8 fw_vid[4];
	u8 subsys_num;
	u8 chip_type;
	u8 protocol_ver;
	u8 reserved[2];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};


struct firmware_data {
	struct firmware_info fw_info;
	const struct firmware *firmware;
};




enum update_status {
	UPSTA_NOTWORK = 0,
	UPSTA_PREPARING,
	UPSTA_UPDATING,
	UPSTA_ABORT,
	UPSTA_SUCCESS,
	UPSTA_FAILED
};

struct fw_update_ctrl {
	enum update_status  status;
	unsigned int progress;
	bool force_update;

	bool allow_reset;
	bool allow_irq;
	bool allow_suspend;
	bool allow_resume;

	struct firmware_data fw_data;
	struct goodix_ts_device *ts_dev;
	struct goodix_ts_core *core_data;

	char fw_name[32];
	struct bin_attribute attr_fwimage;
	bool fw_from_sysfs;
	int mode;
};

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
extern struct goodix_module goodix_modules_V2;
extern struct gesture_module *gsx_gesture_V2;
extern int goodix_fw_update_proc_V2(struct fw_update_ctrl *fwu_ctrl);

extern int goodix_ts_irq_enable_V2(struct goodix_ts_core *core_data,
			bool enable);


void bbk_goodix_cmds_init_V2(struct goodix_ts_cmd *ts_cmd,
					     u8 cmds, u8 cmd_data, u32 reg_addr)
{
	if (reg_addr) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 3;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = cmd_data;
		ts_cmd->cmds[2] = 0 - cmds - cmd_data;
		ts_cmd->initialized = true;
	} else {
		ts_cmd->initialized = false;
	}
}

int bbk_goodix_get_channel_num_V2(u32 *sen_num, u32 *drv_num)
{
	int ret = -1;
	u8 buf[2] = {0};
	struct goodix_ts_device *ts_dev = goodix_modules_V2.core_data->ts_dev;
	
	VTI("===enter===");

	ret = ts_dev->hw_ops->read(ts_dev, BBK_SENSOR_NUM_ADDR, buf, 1);
	if (ret) {
		VTE("Read sen_num fail.");
		return ret;
	}

	*sen_num = buf[0];

	ret = ts_dev->hw_ops->read(ts_dev, BBK_DRIVER_GROUP_A_NUM_ADDR, buf, 2);
	if (ret) {
		VTE("Read drv_num fail.");
		return ret;
	}

	*drv_num = buf[0] + buf[1];
	VTI("sen_num : %d, drv_num : %d", *sen_num, *drv_num);

	return 0;
}

int bbk_goodix_be_normal_V2(void)
{
	int ret = -1;
	struct goodix_ts_cmd normal_cmd;
	struct goodix_ts_device *ts_dev = goodix_modules_V2.core_data->ts_dev;
	unsigned char buf[2] = {0x00, 0x00};

	VTI("===enter===");

	bbk_goodix_cmds_init_V2(&normal_cmd, 0x00, 0, ts_dev->reg.command);
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &normal_cmd);
	if (ret)
		VTE("Send enter normal mode command fail.");
	
	buf[0] = buf[1] = 0x00;
	ret = ts_dev->hw_ops->write(ts_dev, BBK_READ_COOR_ADDR, buf, 2);
	if (ret) {
		VTE("After be_normal, Set zero to interrupt status reg fail.");
		ret = -EINVAL;
	}
	msleep(30);

	return ret;
}

int bbk_goodix_fw_update_V2(const struct firmware *firmware)
{
	struct fw_update_ctrl *fwu_ctrl;
	static DEFINE_MUTEX(bbk_fwu_lock);
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	VTI("===enter===");

	fwu_ctrl = kzalloc(sizeof(struct fw_update_ctrl), GFP_KERNEL);
	if (!fwu_ctrl) {
		VTE("Failed to alloc memory for fwu_ctrl");
		return -EPERM;
	}

	fwu_ctrl->ts_dev = ts_dev;
	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;
	fwu_ctrl->core_data = core_data;

	mutex_lock(&bbk_fwu_lock);
	fwu_ctrl->fw_data.firmware = firmware;
	
	/* DONT allow reset/irq/suspend/resume during update */
	fwu_ctrl->allow_irq = false;
	fwu_ctrl->allow_suspend = false;
	fwu_ctrl->allow_resume = false;
	fwu_ctrl->allow_reset = false;
	goodix_ts_blocking_notify_V2(NOTIFY_FWUPDATE_START, NULL);
	
	/* ready to update */
	goodix_fw_update_proc_V2(fwu_ctrl);

	goodix_ts_blocking_notify_V2(NOTIFY_FWUPDATE_END, NULL);
	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;
	
	mutex_unlock(&bbk_fwu_lock);

	kfree(fwu_ctrl);

	return 0;
}

int bbk_goodix_readUdd_V2_v1(unsigned char *udd)
{
	int ret = 0;
	struct goodix_ts_device *ts_dev = goodix_modules_V2.core_data->ts_dev;

	VTI("===enter===");

	ret = ts_dev->hw_ops->read(ts_dev, BBK_USER_DATA_ADDR, udd, 15);
	if (ret) {
		VTE("User data i2c read failed");
		ret = -1;
	}

	return ret;
}

int bbk_goodix_writeUdd_V2_v1(unsigned char *udd)
{
	int ret = 0;
	unsigned char buf[BBK_USER_DATA_MAX_LEN] = {0};
	struct goodix_ts_cmd cmd;
	struct goodix_ts_device *ts_dev = goodix_modules_V2.core_data->ts_dev;

	VTI("===enter===");

	memcpy(buf, udd, 15);
	buf[BBK_USER_DATA_MAX_LEN - 2] = 0xAA;
	buf[BBK_USER_DATA_MAX_LEN - 1] = 0x100 - checksum_u8(buf, BBK_USER_DATA_MAX_LEN - 1);

	ret = ts_dev->hw_ops->write(ts_dev, BBK_USER_DATA_ADDR,
					buf, BBK_USER_DATA_MAX_LEN);
	if (ret) {
		VTE("User data i2c write failed");
		return -EPERM;
	}

	bbk_goodix_cmds_init_V2(&cmd, 0x0A, 0, ts_dev->reg.command);
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &cmd);
	if (ret) {
		VTE("Send write udd command error");
		return -EPERM;
	}

	return ret;
}


void bbk_goodix_enter_gesture_V2(void)
{
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int r = 0;
	//struct goodix_ts_cmd finger_dclick_cmd;
	//struct goodix_ts_cmd finger_icon_cmd;
   // int ret = 0;
	VTI("===enter===core_data->suspended:%d", atomic_read(&core_data->suspended));
	
#ifdef CONFIG_PINCTRL
		if (core_data->pinctrl && core_data->pin_sta_active) {
			VTI("select active pinstate");
			r = pinctrl_select_state(core_data->pinctrl,
						core_data->pin_sta_active);
			if (r < 0)
				VTE("Failed to select active pinstate, r:%d", r);
		}
#endif

	if (atomic_read(&core_data->suspended) == 1) {
		if (ts_dev && ts_dev->hw_ops->resume)
			ts_dev->hw_ops->resume(ts_dev);

		goodix_ts_irq_enable_V2(core_data, true);
	}
	enable_irq_wake(core_data->irq);
/*
	if (0 == (vivoTsGetVtsData()->exportSwitch & 0x01)) {
		bbk_goodix_cmds_init_V2(&finger_dclick_cmd, 0x19, 0x01, ts_dev->reg.command);
	} else {
		bbk_goodix_cmds_init_V2(&finger_dclick_cmd, 0x19, 0x00, ts_dev->reg.command);
	}
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &finger_dclick_cmd);
	if (ret) {
		VTE("Send finger dclick command error");
		ret = -1;
	}
	if (1 == (vivoTsGetVtsData()->exportSwitch & 0x01)) {
		bbk_goodix_cmds_init_V2(&finger_icon_cmd, 0x1a, 0x01, ts_dev->reg.command);
		VTD("%02X, %02X, %02X", finger_icon_cmd.cmds[0], finger_icon_cmd.cmds[1], finger_icon_cmd.cmds[2]);
		ret = ts_dev->hw_ops->send_cmd(ts_dev, &finger_icon_cmd);
		if (ret) {
			VTE("Send finger dclick command error");
			ret = -1;
		}	
	}

	bbk_goodix_cmds_init_V2(&finger_dclick_cmd, 0x19, 0x01, ts_dev->reg.command);
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &finger_dclick_cmd);
	if (ret) {
		VTE("Send finger dclick command error");
		ret = -1;
	}
*/
	goodix_ts_suspend_V2(core_data, 0);
}


void bbk_goodix_set_ic_enter_gesture_V2(void)
{
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;
	//struct goodix_ts_device *ts_dev = core_data->ts_dev;
	//struct goodix_ts_cmd finger_dclick_cmd;
	//struct goodix_ts_cmd finger_icon_cmd;
	struct goodix_ext_module *ext_module;
	int ret = 0;

	VTI("===enter===core_data->suspended:%d", atomic_read(&core_data->suspended));
	
	enable_irq_wake(core_data->irq);
/*
	if (0 == (vivoTsGetVtsData()->exportSwitch & 0x01)) {
		bbk_goodix_cmds_init_V2(&finger_dclick_cmd, 0x19, 0x01, ts_dev->reg.command);
	} else {
		bbk_goodix_cmds_init_V2(&finger_dclick_cmd, 0x19, 0x00, ts_dev->reg.command);
	}
	

	VTD("%02X, %02X, %02X", finger_dclick_cmd.cmds[0], finger_dclick_cmd.cmds[1], finger_dclick_cmd.cmds[2]);
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &finger_dclick_cmd);
	if (ret) {
		VTE("Send finger dclick command error");
	}
	mdelay(20);
	
	if (1 == (vivoTsGetVtsData()->exportSwitch & 0x01)) {
		bbk_goodix_cmds_init_V2(&finger_icon_cmd, 0x1a, 0x01, ts_dev->reg.command);
		VTD("%02X, %02X, %02X", finger_icon_cmd.cmds[0], finger_icon_cmd.cmds[1], finger_icon_cmd.cmds[2]);
		ret = ts_dev->hw_ops->send_cmd(ts_dev, &finger_icon_cmd);
		if (ret) {
			VTE("Send finger dclick command error");
		}	
	}
	mdelay(20);
*/
	mutex_lock(&goodix_modules_V2.mutex);
	if (!list_empty(&goodix_modules_V2.head)) {
		list_for_each_entry(ext_module, &goodix_modules_V2.head, list) {
			if (!ext_module->funcs || !ext_module->funcs->before_suspend)
				continue;

			ret = ext_module->funcs->before_suspend(core_data, ext_module);
			if (ret == EVT_CANCEL_SUSPEND) {
				VTI("Canceled by module:%s", ext_module->name);
			}
		}
	}
	mutex_unlock(&goodix_modules_V2.mutex);
}

int touch_state = 0;

int bbk_goodix_mode_change_V2(int which)
{
	int ret = 0;
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;

	VTI("====start====, which:%d (0:Normal, 1:Sleep)", which);
	switch (which) {
	case VTS_ST_NORMAL:
		ret = goodix_ts_resume_V2(core_data);
		if (ret) {
			VTE("Change normal mode fail.");
			ret = -1;
		}
		touch_state = VTS_ST_NORMAL;
		break;
	case VTS_ST_SLEEP:
		ret = goodix_ts_suspend_V2(core_data, 1);
		if (ret) {
			VTE("Change sleep mode fail.");
			ret = -1;
		}
		touch_state = VTS_ST_SLEEP;
		break;
	default:
		VTE("Invalid mode change params");
		ret = -1;
		break;
	}
	return ret;
}

int bbk_goodix_get_fw_version_V2_v1(int which)
{
	u8 ver_buf[4] = {0};
	u8 *cfg_buf;
	int ret = -1;
	struct goodix_ts_core *core_data = goodix_modules_V2.core_data;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	switch (which) {
	case 0:
		ret = ts_dev->hw_ops->read(ts_dev, ts_dev->reg.vid,
					ver_buf, ts_dev->reg.vid_len);
		if (ret) {
			VTE("Read fw version fail.");
			return -EPERM;
		}
		VTI("VID:%02x %02x %02x %02x", ver_buf[0], ver_buf[1],
					ver_buf[2], ver_buf[3]);
		ret = (ver_buf[0] << 24) | (ver_buf[1] << 16) | (ver_buf[2] << 8) | ver_buf[3];
		break;
	case 1:
		cfg_buf = kzalloc(4096, GFP_KERNEL);
		if (!cfg_buf) {
			VTE("Alloc config buffer mem fail.");
			return -EPERM;
		}

		disable_irq(core_data->irq);
		if (ts_dev->hw_ops->read_config) {
			ret = ts_dev->hw_ops->read_config(ts_dev, cfg_buf, 0);
			if (ret > 0) {
				VTI("config version : %d", cfg_buf[0]);
				ret = cfg_buf[0];
			} else {
				VTE("Read config version fail.");
				ret = -1;
			}
		} else {
			ret = -1;
		}
		enable_irq(core_data->irq);

		kfree(cfg_buf);
		break;
	default:
		VTE("Invalid get fw version params");
		break;
	}

	return ret;
}

int bbk_goodix_gesture_point_get_V2_v1(u16 *data)
{
	int i;
	u8 gesture_type = gsx_gesture_V2->gesture_data[2];
	int ges_num = 0;

	switch (gesture_type) {
	case 0xBA://VTS_EVENT_GESTURE_PATTERN_UP
	case 0xAB://VTS_EVENT_GESTURE_PATTERN_DOWN
		ges_num = 2;
		data[0] = (gsx_gesture_V2->gesture_buffer_data[1] << 8) | gsx_gesture_V2->gesture_buffer_data[0];
		data[1] = (gsx_gesture_V2->gesture_buffer_data[3] << 8) | gsx_gesture_V2->gesture_buffer_data[2];
		data[2] = (gsx_gesture_V2->gesture_buffer_data[5] << 8) | gsx_gesture_V2->gesture_buffer_data[4];
		data[3] = (gsx_gesture_V2->gesture_buffer_data[7] << 8) | gsx_gesture_V2->gesture_buffer_data[6];
		break;
	case 0x65://VTS_EVENT_GESTURE_PATTERN_E
	case 0x40://VTS_EVENT_GESTURE_PATTERN_A
	case 0x66://VTS_EVENT_GESTURE_PATTERN_F
	case 0x6F://VTS_EVENT_GESTURE_PATTERN_O
		ges_num = 6;
		for (i = 0; i < 2 * ges_num; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];
		break;
	case 0x77://VTS_EVENT_GESTURE_PATTERN_W
		ges_num = 5;
		for (i = 0; i < 2 * ges_num; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];
		break;
	case 0x63://VTS_EVENT_GESTURE_PATTERN_C
		ges_num = 6;
		for (i = 0; i < 10; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];

		data[10] = data[8];
		data[11] = data[9];
		break;
	default:
		ges_num = 0;
		break;
	}

	for (i = 0; i < 12; i++)
		VTI("data[i]:%d", data[i]);

	return ges_num;
}



int bbk_goodix_gesture_point_get_V2(struct goodix_ts_core *core_data, u16 *data)
{
	int i;
	u8 gesture_type = gsx_gesture_V2->gesture_data[2];
	int ges_num = 0;
	u16 coordinate_x[9];
	u16 coordinate_y[9];
	u16 temp_x, temp_y;

	switch (gesture_type) {
	case 0xBA://VTS_EVENT_GESTURE_PATTERN_UP
	case 0xAB://VTS_EVENT_GESTURE_PATTERN_DOWN
		ges_num = 2;
		data[0] = (gsx_gesture_V2->gesture_buffer_data[1] << 8) | gsx_gesture_V2->gesture_buffer_data[0];
		data[1] = (gsx_gesture_V2->gesture_buffer_data[3] << 8) | gsx_gesture_V2->gesture_buffer_data[2];
		data[2] = (gsx_gesture_V2->gesture_buffer_data[5] << 8) | gsx_gesture_V2->gesture_buffer_data[4];
		data[3] = (gsx_gesture_V2->gesture_buffer_data[7] << 8) | gsx_gesture_V2->gesture_buffer_data[6];
		break;
	case 0x65://VTS_EVENT_GESTURE_PATTERN_E
	case 0x40://VTS_EVENT_GESTURE_PATTERN_A
	case 0x66://VTS_EVENT_GESTURE_PATTERN_F
	case 0x6F://VTS_EVENT_GESTURE_PATTERN_O
		ges_num = 6;
		for (i = 0; i < 2 * ges_num; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];
		break;
	case 0x77://VTS_EVENT_GESTURE_PATTERN_W
		ges_num = 5;
		for (i = 0; i < 2 * ges_num; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];
		break;
	case 0x63://VTS_EVENT_GESTURE_PATTERN_C
		ges_num = 6;
		for (i = 0; i < 10; i++)
			data[i] = (gsx_gesture_V2->gesture_buffer_data[2 * i + 1] << 8) | gsx_gesture_V2->gesture_buffer_data[2 * i];

		data[10] = data[8];
		data[11] = data[9];
		break;
	default:
		ges_num = 0;
		break;
	}

	for (i = 0; i < ges_num; i++){
		temp_x = data[2*i] ;
		temp_y = data[2*i+1];
		VTI("x[i]:%d, y[i]:%d", coordinate_x[i], coordinate_y[i]);
	}

	return ges_num;
}