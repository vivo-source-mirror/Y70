/* vivosrc/vivo_smart_aod.c
 *
 * Samsung Panel smart AOD driver
 *
 * Copyright (c) 2019 vivo
 *
 * Wenlong Zhuang, <wenlong.zhuang@vivo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "vivo_lcm_panel_common.h"

#define SMART_AOD_REG_DUMP_ENABLE 1

#define SMART_AOD_MAX_DISPLAY_SIZE	1300000 /* bits */

#define DSI_REG32_DUMP(_regs) \
	do { \
		unsigned char dump[32] = {0};	\
		int i = 0;	\
		unsigned int len = sizeof(_regs); \
		len = (len + 7) & (~7); \
		len = len > 32 ? 32 : len;\
		memcpy(dump, _regs, sizeof(_regs)); \
		for (i = 0; i < len; i += 8) \
			pr_info("dsi: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", \
				dump[i], dump[1 + i], dump[2 + i], dump[3 + i], dump[4 + i], dump[5 + i], dump[6 + i], dump[7 + i]); \
	} while (0)

enum color_depth bit_to_depth(unsigned int depth_bit)
{
	switch (depth_bit) {
	case 1:
		return AOD_1_BIT;
	case 2:
		return AOD_2_BIT;
	case 4:
		return AOD_4_BIT;
	case 8:
		return AOD_8_BIT;
	default:
		return AOD_2_BIT;
	}
}

bool smart_aod_total_size_check(struct smart_aod_data *aod_data)
{
	unsigned int size = 0;
	int i;

	for (i = 0; i < SMART_AOD_MAX_AREA_NUM; i++) {
		struct aod_area_data *area = &aod_data->area[i];

		if (area->enable) {
			if (area->rgb_enable)
				size += area->width * area->height * area->depth_bit * 3;
			else
				size += area->width * area->height * area->depth_bit;
		}
	}

	return (size <= SMART_AOD_MAX_DISPLAY_SIZE);
}

bool smart_aod_area_check(struct vivo_display_driver_data *vdd, u32 area_id, u32 depth,
			u32 x_start, u32 y_start, u32 width, u32 height)
{
	if (!vdd)
		return false;

	if (x_start + width > vdd->xres || y_start + height > vdd->yres) {
		LCD_ERR("area oversize\n");
		return false;
	}

	if (area_id < 0 || area_id > 5) {
		LCD_ERR("area_id is illegal, area_id is %u\n", area_id);
		return false;
	}

	if ((x_start - 1) % 4) {
		LCD_ERR("byte unaligned, x_start is %u\n", x_start);
		return false;
	}

	if (width % 96) {
		LCD_ERR("byte unaligned, width is %u\n", width);
		return false;
	}

	if (!(depth == 1 || depth == 2 || depth == 4 || depth == 8)) {
		LCD_ERR("color depth is illegal, depth is %u\n ", depth);
		return false;
	}

	return true;
}

void vivo_smart_aod_init(struct vivo_display_driver_data *vdd, struct smart_aod_data *aod_data)
{
	struct dsi_panel *panel = (struct dsi_panel *)vdd->msm_private;
	struct device_node *np = get_panel_device_node(vdd);
	u32 area_id, depth_bit, x_start, y_start, width, height;
	u32 length = 0, count = 0;
	u32 array[6] = { 0 };
	int ret;

	if (!panel || !np) {
		LCD_ERR("dsi panel or device node is NULL\n");
		return;
	}

	/* area0/area1 is AOD display of clock */
	aod_data->area[0].rgb_enable = 0;
	aod_data->area[0].depth_bit = 2;
	aod_data->area[1].rgb_enable = 0;
	aod_data->area[1].depth_bit = 2;

	/* parse AOD area for fingerprint */
	if (!of_get_property(np, "qcom,smart-aod-fingerprint-area", &length)) {
		LCD_ERR("qcom,smart-aod-fingerprint-area not found\n");
		goto error;
	}

	count = length / sizeof(u32);
	if (count != 6) {
		LCD_ERR("qcom,smart-aod-fingerprint-area invalid parameters\n");
		goto error;
	}

	ret = of_property_read_u32_array(np, "qcom,smart-aod-fingerprint-area", array, 6);
	if (ret) {
		LCD_ERR("[%s] cannot read qcom,smart-aod-fingerprint-area\n", __func__);
		goto error;
	}

	area_id = array[0];
	depth_bit = array[1];
	x_start = array[2];
	y_start = array[3];
	width = array[4];
	height = array[5];

	if (!smart_aod_area_check(vdd, area_id, depth_bit, x_start, y_start, width, height))
		goto error;

	aod_data->area[area_id].enable = 1;
	aod_data->area[area_id].area_id = area_id;
	aod_data->area[area_id].rgb_enable = 1;
	aod_data->area[area_id].depth_bit = depth_bit;
	aod_data->area[area_id].x_start = x_start;
	aod_data->area[area_id].y_start = y_start;
	aod_data->area[area_id].width = width;
	aod_data->area[area_id].height = height;

	LCD_INFO("[%d] %dbit ( %d, %d, %d, %d )\n",
		area_id, depth_bit, x_start, y_start, width, height);
	return;

error:
	/* default area5 is AOD display of fingerprint for resolution 1080x2400 */
	aod_data->area[5].enable = 1;
	aod_data->area[5].area_id = 5;
	aod_data->area[5].rgb_enable = 1; /* default use rgb 4bit */
	aod_data->area[5].depth_bit = 4;
	aod_data->area[5].x_start = 445;
	aod_data->area[5].y_start = 2065;
	aod_data->area[5].width = 192;
	aod_data->area[5].height = 190;
	LCD_INFO("[5] 4bit ( 445, 2065, 192, 190 )\n");
}

int vivo_smart_aod_set(struct vivo_display_driver_data *vdd, struct smart_aod_data *aod_data)
{
	unsigned char regs_area[31] = { 0 };
	unsigned char regs_colorbit_info[14] = { 0 };
	unsigned int area_enable;
	int i = 0, ret = 0;
	struct dsi_panel *panel;

	if (!vdd || !vdd->panel_ops.dsi_write_cmd) {
		LCD_ERR("invalid vdd or ops\n");
		return -EINVAL;
	}

	panel = (struct dsi_panel *)vdd->msm_private;
	if (!panel) {
		LCD_ERR("invalid panel\n");
		return -EINVAL;
	}

	regs_area[0] = 0x81;
	regs_colorbit_info[0] = 0x81;

	for (i = 0; i < SMART_AOD_MAX_AREA_NUM; i++) {
		struct aod_area_data *area = &aod_data->area[i];
		unsigned int x_start = area->x_start - 1;
		unsigned int y_start = area->y_start - 1;
		unsigned int y_end = area->y_start + area->height - 1;

		if (area->enable) {
			area_enable |= 1 << (5 - i);
			regs_area[1 + i * 5] = x_start >> 4;
			regs_area[2 + i * 5] = ((x_start & 0x0F) << 4 | (area->width / 96));
			regs_area[3 + i * 5] = y_start >> 4;
			regs_area[4 + i * 5] = ((y_start & 0x0F) << 4) | (y_end >> 8);
			regs_area[5 + i * 5] = y_end & 0xFF;

			/* color_sel */
			if (i < 3)
				regs_colorbit_info[1] |= area->rgb_enable << (6 - i);
			else
				regs_colorbit_info[1] |= area->rgb_enable << (5 - i);

			/* area_depth */
			area->depth = bit_to_depth(area->depth_bit);
			if (i < 4)
				regs_colorbit_info[2] |= area->depth << (6 - i * 2);
			else
				regs_colorbit_info[3] |= (area->depth << (5 - i) * 2) << 4;

			/* set area_color only when mono */
			if (!area->rgb_enable) {
				if (i < 2)
					regs_colorbit_info[4] |= area->color << (4 - 4 * i);
				else if (i < 4)
					regs_colorbit_info[5] |= area->color << ((3 - i) * 4);
				else
					regs_colorbit_info[6] |= area->color << ((5 - i) * 4);
			}

			/* set gray only when area_depth is 1 bit */
			if (area->depth == AOD_1_BIT)
				regs_colorbit_info[7 + i] = area->gray;
		}
	}

	ret = vdd->panel_ops.dsi_write_cmd(panel, (u8[]){ 0x9F, 0xA5, 0xA5 }, 3, 0);
	if (ret)
		goto error;

	/* area enable */
	ret = vdd->panel_ops.dsi_write_cmd(panel, (u8[]){ 0x81, area_enable }, 2, 0);
	if (ret)
		goto error;

	/* area setting */
	ret = vdd->panel_ops.dsi_write_cmd(panel, (u8[]){ 0xB0, 0x01, 0x81 }, 3, 0);
	if (ret)
		goto error;

	ret = vdd->panel_ops.dsi_write_cmd(panel, regs_area, sizeof(regs_area), 0);
	if (ret)
		goto error;

	/* area option */
	ret = vdd->panel_ops.dsi_write_cmd(panel, (u8[]){ 0xB0, 0x01F, 0x81 }, 3, 0);
	if (ret)
		goto error;

	ret = vdd->panel_ops.dsi_write_cmd(panel, regs_colorbit_info, sizeof(regs_colorbit_info), 0);
	if (ret)
		goto error;

	ret = vdd->panel_ops.dsi_write_cmd(panel, (u8[]){ 0x9F, 0x5A, 0x5A }, 3, MIPI_DSI_MSG_LASTCOMMAND);
	if (ret)
		goto error;

#if SMART_AOD_REG_DUMP_ENABLE
	DSI_REG32_DUMP(regs_area);
	DSI_REG32_DUMP(regs_colorbit_info);
#endif

error:
	if (ret)
		LCD_ERR("failed to send smart aod setting cmd, ret=%d\n", ret);
	else
		LCD_INFO("update smart aod display\n");

	return ret;
}

void vivo_smart_aod_update(struct vivo_display_driver_data *vdd)
{
	struct dsi_panel *panel = (struct dsi_panel *)vdd->msm_private;

	if (!panel)
		return;

	mutex_lock(&panel->panel_lock);

	if (!vdd->doze_mode_state)
		goto exit;

	/* limit max 15 fps to update frame, include set display mode */
	if (vdd->smart_aod_update_jiffies)
		do_away_udelay(vdd->smart_aod_update_jiffies, 66700);

	vdd->smart_aod_update_jiffies = get_jiffies_64();

	if (vivo_panel_mode_switch_pending(vdd)) {
		LCD_INFO("can not update when mode switch pending\n");
		goto exit;
	}

	if (vdd->smart_aod_update) {
		vivo_smart_aod_set(vdd, &vdd->smart_aod);
		vdd->smart_aod_update = false;
	}

exit:
	mutex_unlock(&panel->panel_lock);
}

void vivo_smart_aod_display_on(struct drm_device *dev, struct vivo_display_driver_data *vdd)
{
	struct dsi_panel *panel = (struct dsi_panel *)vdd->msm_private;
	int ret = 0;
	char *envp[2] = {"SMART_AOD=DOZE_DISPLAY", NULL};

	if (!panel)
		return;

	mutex_lock(&panel->panel_lock);

	usleep_range(34000, 34100);

	ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_AOD_ON_DISPLAY_ON);

	if (ret)
		LCD_ERR("failed to send aod display on cmd, ret=%d\n", ret);

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);

	mutex_unlock(&panel->panel_lock);
}

