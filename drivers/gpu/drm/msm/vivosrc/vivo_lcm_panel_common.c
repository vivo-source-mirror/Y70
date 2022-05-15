/*
 * =================================================================
 *
 *
 *	Description:  vivo display common file
 *
 *	Author: keli
 *	Company:  vivo
 *
 * ================================================================
 */
/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "vivo_lcm_panel_common.h"
#include "sde_trace.h"

struct vivo_display_driver_data vivo_date;
int silent_reboot;
static int lcm_especial_id;
static int lcm_software_id;
atomic_t after_hbm_count;
extern char dsi_display_primary[MAX_CMDLINE_PARAM_LEN];
extern char dsi_display_secondary[MAX_CMDLINE_PARAM_LEN];

void do_away_udelay(u64 old_jiffies, unsigned int delay_us)
{
	u64 away_jiffies;
	unsigned int away_us;

	away_jiffies = get_jiffies_64() - old_jiffies;
	away_us = jiffies64_to_nsecs(away_jiffies) / 1000;

	if (away_us < delay_us) {
		usleep_range((delay_us - away_us), (delay_us - away_us + 800));
		LCD_INFO("away_jiffies = %d; need_us = %d, delay_us = %d\n",
					away_jiffies, delay_us - away_us, delay_us);
	}
}

struct vivo_display_driver_data *vivo_get_vdisp(void)
{
	struct vivo_display_driver_data *vdisp = &vivo_date;
	return vdisp;
}

static __init int set_bbk_silent_mode(char *str)
{
	if (strcmp(str, "1") == 0)
		silent_reboot = 1;
	LCD_INFO("bbk silent_reboot is %d\n", silent_reboot);
	return 0;
}
early_param("boot_silent", set_bbk_silent_mode);

static __init int set_bbk_internal_mode(char *str)
{
	if (strcmp(str, "1") == 0)
		vivo_date.lcm_internal_id = 1;
	LCD_INFO("bbk lcm_internal_id is %d\n", vivo_date.lcm_internal_id);
	return 0;
}
early_param("lcm_internal", set_bbk_internal_mode);

static int dsi_wait_one_vblank(struct vivo_display_driver_data *vdisp)
{
	struct panel_ud_hbm_sync *ud_hbm_sync = &vdisp->ud_hbm_sync;
	int ret = 0;

	ud_hbm_sync->te_con = 1;
	ret = wait_event_timeout(ud_hbm_sync->te_wq, !ud_hbm_sync->te_con, msecs_to_jiffies(28));
	if (!ret) {
		ud_hbm_sync->te_con_use_vblank = 1;
		LCD_ERR("timeout\n");
	}

	return ret;
}

static int dsi_panel_update_crc_compensate(struct dsi_panel *panel,
	u32 bl_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp = NULL;

	if (!panel) {
		LCD_ERR("dsi crc compensate invalid params\n");
		return -EINVAL;
	}

	if (!panel->panel_initialized) {
		LCD_ERR("panel not ready for hbm update\n");
		return -EINVAL;
	}

	vdisp = panel->panel_private;

	if (panel->bl_config.bl_max_level == 1023)
		bl_lvl = bl_lvl * 2;

	if (bl_lvl > 0 && bl_lvl < 51) {
		if (vdisp->color_gamut_compensate_state != 4) {
			if (vdisp->colour_gamut_state == 0)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_P3_LEVEL3);
			else if (vdisp->colour_gamut_state == 1)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_SRGB_LEVEL3);
			vdisp->color_gamut_compensate_state = 4;
			LCD_ERR("msm-dsi color_gamut_compensate_state is %d\n", vdisp->color_gamut_compensate_state);
		}
	} else if (bl_lvl > 50 && bl_lvl < 251) {
		if (vdisp->color_gamut_compensate_state != 3) {
			if (vdisp->colour_gamut_state == 0)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_P3_LEVEL2);
			else if (vdisp->colour_gamut_state == 1)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_SRGB_LEVEL2);
			vdisp->color_gamut_compensate_state = 3;
			LCD_ERR("msm-dsi color_gamut_compensate_state is %d\n", vdisp->color_gamut_compensate_state);
		}
	} else if (bl_lvl > 250 && bl_lvl < 551) {
		if (vdisp->color_gamut_compensate_state != 2) {
			if (vdisp->colour_gamut_state == 0)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_P3_LEVEL1);
			else if (vdisp->colour_gamut_state == 1)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_SRGB_LEVEL1);
			vdisp->color_gamut_compensate_state = 2;
			LCD_ERR("msm-dsi color_gamut_compensate_state is %d\n", vdisp->color_gamut_compensate_state);
		}
	} else if (bl_lvl > 550) {
		if (vdisp->color_gamut_compensate_state != 1) {
			if (vdisp->colour_gamut_state == 0)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_P3_LEVEL0);
			else if (vdisp->colour_gamut_state == 1)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_COMPENSATE_SRGB_LEVEL0);
			vdisp->color_gamut_compensate_state = 1;
			LCD_ERR("msm-dsi color_gamut_compensate_state is %d\n", vdisp->color_gamut_compensate_state);
		}
	}
	if (rc < 0) {
		LCD_ERR("dsi failed to send DSI_CMD_SET_CRC_compensate cmds, rc = %d, bl_lvl = %d\n",
			rc, bl_lvl);
	}
	return rc;
}

int dsi_panel_update_cabc(struct dsi_panel *panel, u32 cabc_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp = NULL;

	if (!panel) {
		LCD_ERR("dsi hbm invalid params\n");
		return -EINVAL;
	}
	vdisp = panel->panel_private;

	if ((panel->bl_config.bl_level < 428 && panel->bl_config.bl_max_level == 2047) || (panel->bl_config.bl_level < 856 && panel->bl_config.bl_max_level == 4095)) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_OFF);
		vdisp->tft_cabc_state = 0;
		vdisp->is_cabc_recover = 0;
	} else {
		if (cabc_lvl == 3)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL3);
		else if (cabc_lvl == 2)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL2);
		else if (cabc_lvl == 1)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL1);
		else if (cabc_lvl == 0)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_OFF);
		}

	if (rc < 0) {
		LCD_ERR("[%s] failed to send DSI_CMD_SET_CABC cmds, rc = %d, cabc_lvl = %d\n",
			panel->name, rc, cabc_lvl);
	}
	return rc;
}

int dsi_panel_update_acl(struct dsi_panel *panel, u32 acl_lvl)
{
	int rc = 0;

	if (!panel) {
		LCD_ERR("dsi acl invalid params\n");
		return -EINVAL;
	}

	if (acl_lvl == 3)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL3);
	else if (acl_lvl == 2)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL2);
	else if (acl_lvl == 1)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_ON_LEVEL1);
	else if (acl_lvl == 0)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ACL_OFF);

	if (rc < 0)
		LCD_ERR("[%s] failed to send DSI_CMD_SET_ACL cmds, rc = %d, acl_lvl = %d\n",
				panel->name, rc, acl_lvl);

	return rc;
}

static int dsi_panel_update_sre(struct dsi_panel *panel, u32 level)
{
	int ret = 0, val;
	struct mipi_dsi_device *dsi;

	if (!panel) {
		LCD_ERR("dsi sre invalid params\n");
		return -EINVAL;
	}

	dsi = &panel->mipi_device;
	if (!level) {
		ret = mipi_dsi_dcs_write(dsi, 0x92, (u8[]){level}, 1);
	} else {
		val = (0x79 + level) & 0xFF;
		ret = mipi_dsi_dcs_write(dsi, 0x92, (u8[]){val}, 1);
	}

	if (ret < 0)
		LCD_ERR("failed to set lcm sre, rc = %d, level = %d\n", ret, level);

	return ret;
}

int dsi_panel_update_seed_crc(struct dsi_panel *panel,
	u32 crc_lvl)
{
	int rc = 0;

	if (!panel) {
		LCD_ERR("dsi seed crc invalid params\n");
		return -EINVAL;
	}

	LCD_INFO("[%s] seed crc level = %d\n", panel->name, crc_lvl);

	if (crc_lvl == 0) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_ON_P3);
	} else if (crc_lvl == 1) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_ON_SRGB);
	} else if (crc_lvl == 2) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_CRC_OFF);
	}
	if (rc)
		LCD_ERR("[%s] failed to send crc cmd, rc=%d\n",
			   panel->name, rc);
	return rc;
}

static int dsi_panel_update_seed_ore(struct dsi_panel *panel,
	u32 ore_lvl)
{
	int rc = 0;
	struct mipi_dsi_device *dsi;
	struct vivo_display_driver_data *vdisp = NULL;

	if (!panel) {
		LCD_ERR("dsi seed crc invalid params\n");
		return -EINVAL;
	}
	vdisp = panel->panel_private;
	dsi = &panel->mipi_device;

	if (ore_lvl > 255)
		ore_lvl = 255;

	if (ore_lvl == 0) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_ORE_OFF);
		if (rc)
			goto error;
		vdisp->oled_ore_on = 0;
	} else {
		if (!vdisp->oled_ore_on) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SEED_ORE_ON);
			if (rc)
				goto error;
			vdisp->oled_ore_on = 1;
		}

		ore_lvl = ore_lvl & 0xFF;

		rc = mipi_dsi_dcs_write(dsi, 0xF0, (u8[]){ 0x5A, 0x5A }, 2);
		if (rc)
			goto error;
		rc = mipi_dsi_dcs_write(dsi, 0x87, (u8[]){ 0x80, ore_lvl }, 2);
		if (rc)
			goto error;
		rc = mipi_dsi_dcs_write(dsi, 0xF0, (u8[]){ 0xA5, 0xA5 }, 2);
		if (rc)
			goto error;
	}

error:
	if (rc)
		LCD_ERR("[%s] failed to send ore cmd, rc=%d\n",
			   panel->name, rc);
	return rc;
}

static int dsi_panel_update_elvss_dimming(struct dsi_panel *panel, int enable)
{
	int rc = 0;
	int elvss_dimming_state;
	struct vivo_display_driver_data *vdisp = NULL;

	struct mipi_dsi_device *dsi;

	if (!panel) {
		LCD_ERR("dsi elvss invalid params\n");
		return -EINVAL;
	}

	vdisp = panel->panel_private;

	if (!vdisp->lcm_especial_id && !vdisp->smart_aod.is_support) {
		LCD_ERR("dsi elvss lcm_especial_id = %d\n", vdisp->lcm_especial_id);
		return -EINVAL;
	}

	dsi = &panel->mipi_device;
	if (vdisp->is_ddic_power_aod) {
		if (enable)
			rc = dsi_panel_tx_cmd_set(panel,  DSI_CMD_SET_ELVSS_DIM_ON);
		else
			rc = dsi_panel_tx_cmd_set(panel,  DSI_CMD_SET_ELVSS_DIM_OFF);
	} else {
		if (enable)
			elvss_dimming_state = vdisp->lcm_especial_id | 0x80;
		else
			elvss_dimming_state = vdisp->lcm_especial_id & 0x7F;

		rc = mipi_dsi_dcs_write(dsi, 0xF0, (u8[]){ 0x5A, 0x5A }, 2);
		rc = mipi_dsi_dcs_write(dsi, 0xB0, (u8[]){ 0x08 }, 1);
		rc = mipi_dsi_dcs_write(dsi, 0xB7, (u8[]){ elvss_dimming_state }, 1);
		rc = mipi_dsi_dcs_write(dsi, 0xF0, (u8[]){ 0xA5, 0xA5 }, 2);
	}

	if (rc < 0)
		LCD_ERR("failed to update elvss_dimming_state:0x%x\n", elvss_dimming_state);

	return rc;
}

/*
	Compensation according to different color gamut
*/
void compensate_seed_for_dc_dimming(struct dsi_panel *panel, u32 bl_lvl)
{
	struct vivo_display_driver_data *vdisp  = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("Invaild vdisp data pointer\n");
		return;
	}

	if (vdisp->colour_gamut_state < 0 || vdisp->colour_gamut_state > 2 || panel == NULL) {
		LCD_ERR("%s:param error(%d,%p)", __func__, vdisp->colour_gamut_state, panel);
		return;
	}

	if (0 < bl_lvl && bl_lvl < 100) {
		if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 2)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_P3_CRC_DC_DIMMING_SEED_PARAM_LEVEL1);
		else
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SRGB_DC_DIMMING_SEED_PARAM_LEVEL1);
	} else if (100 <= bl_lvl && bl_lvl <= 199) {
		if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 2)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_P3_CRC_DC_DIMMING_SEED_PARAM_LEVEL2);
		else
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SRGB_DC_DIMMING_SEED_PARAM_LEVEL2);
	} else if (199 < bl_lvl && bl_lvl <= 275) {
		if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 2)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_P3_CRC_DC_DIMMING_SEED_PARAM_LEVEL3);
		else
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_SRGB_DC_DIMMING_SEED_PARAM_LEVEL3);
	} else {
		LCD_ERR("%s:error,bl_lvl=%d", __func__, bl_lvl);
	}

	LCD_INFO("%s:colour_gamut_state=%d,bl_lvl=%d", __func__, vdisp->colour_gamut_state, bl_lvl);
}

/*
 * 1.close dimming(note:recovry dimmming when backlight update)
 * 2.set brightness
 * 3.wait te for sync
*/
void dsi_panel_set_dc_dimming_brightness(u32 brightness, u32 state)
{
	int ret;
	struct panel_ud_hbm_sync *ud_hbm_sync;
	struct dsi_panel *panel;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	u8 wait_te_cnt = 0;

	if (!vdisp) {
		LCD_ERR("Invaild vdisp data pointer\n");
		return;
	}

	if (!vdisp->dc_enable) {
		/*LCD_ERR("dc dimming not enable\n");*/
		return;
	}

	panel = (struct dsi_panel *)vdisp->msm_private;
	if (!panel) {
		LCD_ERR("Invaild panel pointer\n");
		return;
	}

	ud_hbm_sync = &vdisp->ud_hbm_sync;
	if (!ud_hbm_sync) {
		LCD_ERR("Invaild ud_hbm_sync pointer\n");
		return;
	}

	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
		LCD_ERR("%s:Invalid panel_initialized\n", __func__);
		mutex_unlock(&panel->panel_lock);
		return;
	}
	compensate_seed_for_dc_dimming(panel, panel->bl_config.bl_level);
	if ((state & UD_FLAG_DC_DIMMING_DROP_OPEN)
		|| (state & UD_FLAG_DC_DIMMING_DROP_CLOSE)) {
		mutex_unlock(&panel->panel_lock);
		return;
	}
	if (state & UD_FLAG_DC_DIMMING_OPEN)
		wait_te_cnt = 0;
	else
		wait_te_cnt = vdisp->dc_dimming_wait_te_cnt;

	LCD_INFO("%s:brightness=%u,state=%d,wait_te_cnt=%d", __func__, brightness, state, wait_te_cnt);
	vdisp->dc_dimming_brightness = brightness;
	panel->bl_config.bl_level = brightness;

	/* close dimming(note:recovry dimmming when backlight update) */
	ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_OFF);
	if (ret)
		LCD_ERR("%s:close dimming error", __func__);
	vdisp->oled_dimming_enable = 0;

	/* wait two te for sync */
	if (panel->panel_mode == DSI_OP_CMD_MODE)
		enable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
	dsi_wait_one_vblank(vdisp);
	dsi_wait_one_vblank(vdisp);

	ret = dsi_panel_update_backlight(panel, brightness);
	if (ret)
		LCD_ERR("%s:set brightness failed\n", __func__);

	if (wait_te_cnt > 0) {
		while (wait_te_cnt--) {
			if (!dsi_wait_one_vblank(vdisp)) {
				LCD_ERR("%s:wait te timeout,break", __func__);
				break;
			}
			LCD_ERR("%s:wait_te_cnt=%d", __func__, wait_te_cnt);
		}
	}
	if (panel->panel_mode == DSI_OP_CMD_MODE)
		disable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
	mutex_unlock(&panel->panel_lock);
}

static int dsi_panel_update_hbm(struct dsi_panel *panel,
	u32 hbm_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp = NULL;

	if (!panel) {
		LCD_ERR("dsi hbm invalid params\n");
		return -EINVAL;
	}

	if (!panel->panel_initialized) {
		LCD_ERR("panel not ready for hbm update\n");
		return -EINVAL;
	}

	vdisp = panel->panel_private;

	if (hbm_lvl == 0) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF);
		dsi_panel_update_backlight(panel, panel->bl_config.bl_level);
	} else if (hbm_lvl == 1) {
		rc = dsi_panel_update_elvss_dimming(panel, 1);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_LEVEL1);
	} else {
		rc = dsi_panel_update_elvss_dimming(panel, 1);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_LEVEL2);
	}

	if (rc)
		LCD_ERR("[%s] failed to send hbm cmd, rc=%d\n",
			   panel->name, rc);

	if (hbm_lvl)
		vdisp->oled_hbm_on = 1;
	else
		vdisp->oled_hbm_on = 0;

	return rc;
}

static int dsi_panel_update_hbm_ud_fingerprint(struct dsi_panel *panel,
	u32 hbm_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp  = NULL;

	if (!panel) {
		LCD_ERR("dsi hbm invalid params\n");
		return -EINVAL;
	}

	vdisp = panel->panel_private;
	SDE_ATRACE_BEGIN("dsi_panel_update_hbm_ud_fingerprint");

	if (!panel->panel_initialized) {
		LCD_ERR("panel not ready for hbm\n");
		return -EINVAL;
	}

	if (hbm_lvl == 4) {
		if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMMING_OFF_HBM_UD_ON);
			if (rc)
				goto error;

			atomic_set(&after_hbm_count, 2); /* have already wait one vblank irq */
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_OFF);
			if (rc)
				goto error;

			rc = dsi_panel_update_elvss_dimming(panel, 0);
			if (rc)
				goto error;

			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_UD_FINGERPRINT);
			if (rc)
				goto error;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PS_OFF);
			if (rc)
				LCD_ERR("[%s] line: %d, rc=%d\n", panel->name, __LINE__, rc);

			atomic_set(&after_hbm_count, 1);
		}
		panel->ud_hbm_state = UD_HBM_ON;
		vdisp->oled_dimming_enable = 0;
		panel->oled_hbm_state = PANEL_HBM_UD_ON;
	} else if (hbm_lvl == 5) {
		if (panel->panel_mode == DSI_OP_VIDEO_MODE)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_UD_OFF_NO_LASTCOMMAND);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT);
		if (rc)
			goto error;

		dsi_panel_update_backlight(panel, panel->bl_config.bl_level);

		atomic_set(&after_hbm_count, 0);
		panel->ud_hbm_state = UD_HBM_OFF;
		vdisp->ud_fingerprint_hbm_off_state = 1;
		panel->oled_hbm_state = PANEL_HBM_UD_OFF;
	}

	LCD_INFO("oled hbm state=%d\n", panel->oled_hbm_state);
error:
	if (rc)
		LCD_ERR("[%s] failed to send ud fingerprint hbm cmd, hbm_lvl = %d, rc=%d\n",
			   panel->name, hbm_lvl, rc);
	SDE_ATRACE_END("dsi_panel_update_hbm_ud_fingerprint");
	return rc;
}

static int dsi_panel_update_hbm_aod_ud_fingerprint(struct dsi_panel *panel,
	u32 hbm_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!panel) {
		LCD_ERR("dsi hbm invalid params\n");
		return -EINVAL;
	}

	if (!panel->panel_initialized) {
		LCD_ERR("panel not ready for ud hbm\n");
		return -EINVAL;
	}

	if (hbm_lvl == 6) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_AOD_OFF);
		if (rc)
			goto error;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISPLAY_ON);
		if (rc)
			goto error;
		rc = dsi_panel_update_elvss_dimming(panel, 0);
		if (rc)
			goto error;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_UD_FINGERPRINT);
		if (rc)
			goto error;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PS_OFF);
		if (rc)
			LCD_ERR("[%s] failed CMD_SET_PSOFF, rc=%d\n", panel->name, rc);

		atomic_set(&after_hbm_count, 1);
		panel->doze_mode_state = 0;
		panel->ud_hbm_state = UD_HBM_ON;
	} else if (hbm_lvl == 7) {
		if (!panel->ud_hbm_state) {
			LCD_ERR("hbm is not on, no need to set hbm off\n");
			dsi_panel_update_backlight(panel, vdisp->aod_bl_lvl);
			goto error;
		}

		if (panel->panel_mode == DSI_OP_VIDEO_MODE)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_UD_OFF_NO_LASTCOMMAND);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT);
		if (rc)
			goto error;

		rc = dsi_panel_update_backlight(panel, vdisp->aod_bl_lvl);
		LCD_ERR("dsi just set hbm off wait enter aod or nomal mode\n");
		atomic_set(&after_hbm_count, 0);
		panel->ud_hbm_state = UD_HBM_OFF;
		panel->doze_mode_state = 0;
	} else if (hbm_lvl == 8) {
		u32 brightness;

		if (!panel->ud_hbm_state) {
			if (panel->bl_config.bl_level)
				brightness = panel->bl_config.bl_level;
			else
				brightness = vdisp->ud_hbm_off_first_bl;

			if (!IS_ERR_OR_NULL(vdisp->panel_ops.update_backlight))
				rc = vdisp->panel_ops.update_backlight(panel, brightness);

			goto error;
		}

		if (panel->panel_mode == DSI_OP_VIDEO_MODE)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_UD_OFF_NO_LASTCOMMAND);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT);
		if (rc)
			goto error;

		if (panel->bl_config.bl_level)
			brightness = panel->bl_config.bl_level;
		else
			brightness = vdisp->ud_hbm_off_first_bl;
		if (!IS_ERR_OR_NULL(vdisp->panel_ops.update_backlight))
			rc = vdisp->panel_ops.update_backlight(panel, brightness);
		if (rc)
			goto error;

		atomic_set(&after_hbm_count, 0);
		panel->ud_hbm_state = UD_HBM_OFF;
		panel->doze_mode_state = 0;
		vdisp->ud_hbm_sync.ud_flag = 2;
	}
error:
	if (rc)
		LCD_ERR("[%s] failed to send aod ud fingerprint hbm cmd, rc = %d\n",
				panel->name, rc);
	return rc;
}

static int dsi_panel_set_hbm_ud_fingerprint(u32 hbm_lvl)
{
	int rc = 0;
	struct panel_ud_hbm_sync *ud_hbm_sync;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp)
		return -EINVAL;

	panel = (struct dsi_panel *)vdisp->msm_private;

	if (!panel) {
		LCD_ERR("dsi ud hbm invalid params\n");
		return -EINVAL;
	}

	ud_hbm_sync = &vdisp->ud_hbm_sync;
	if (!ud_hbm_sync) {
		LCD_ERR("dsi get_ud_hbm_sync fail\n");
		return -EINVAL;
	}

	if ((panel->bl_config.bl_level == 0) || (vdisp->silent_reboot == 1))
		return rc;

	SDE_ATRACE_BEGIN("dsi_panel_set_hbm");
	if ((hbm_lvl == 4) && (panel->ud_hbm_state == UD_HBM_OFF)) {
		if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMMING_OFF_HBM_UD_ON_SYNC);
			if (rc)
				LCD_ERR("set hbm on error");
			atomic_set(&after_hbm_count, 2); /* have already wait one vblank irq */
		} else {
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				enable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
			dsi_wait_one_vblank(vdisp);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_OFF);
			if (rc)
				LCD_ERR("set DSI_CMD_SET_DIMING_OFF error");

			rc = dsi_panel_update_elvss_dimming(panel, 0);
			if (rc)
				LCD_ERR("dsi_panel_update_elvss_dimming error\n");

			dsi_wait_one_vblank(vdisp);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_UD_FINGERPRINT);
			if (rc)
				LCD_ERR("dsi set hbm on error--\n");
			atomic_set(&after_hbm_count, 1);
			dsi_wait_one_vblank(vdisp);
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				disable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
		}
	} else if ((hbm_lvl == 2) && (panel->ud_hbm_state == UD_HBM_OFF)) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PS_OFF);
		if (rc)
			LCD_ERR("set DSI_CMD_SET_PS_OFF error");
		panel->ud_hbm_state = UD_HBM_ON;
		vdisp->oled_dimming_enable = 0;
		panel->oled_hbm_state = PANEL_HBM_UD_ON;
	} else if ((hbm_lvl == 5) && (panel->ud_hbm_state == UD_HBM_ON)) {
		if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT);
			if (rc)
			LCD_ERR("set DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT error");
			atomic_set(&after_hbm_count, 0);
		} else {
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				enable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
			dsi_wait_one_vblank(vdisp);
			dsi_wait_one_vblank(vdisp);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT);
			if (rc)
				LCD_ERR("set DSI_CMD_SET_HBM_OFF_UD_FINGERPRINT error");

			atomic_set(&after_hbm_count, 0);
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				disable_irq(gpio_to_irq(vdisp->ud_hbm_sync.panel_te_gpio));
		}
	} else if ((hbm_lvl == 3) && (panel->ud_hbm_state == UD_HBM_ON)) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PS_ON);
		if (rc)
			LCD_ERR("set DSI_CMD_SET_PS_ON error\n");

		rc = dsi_panel_update_backlight(panel, panel->bl_config.bl_level);
		if (rc)
			LCD_ERR("set dsi_panel_update_backlight error");
		panel->ud_hbm_state = UD_HBM_OFF;
		vdisp->ud_fingerprint_hbm_off_state = 1;
		panel->oled_hbm_state = PANEL_HBM_UD_OFF;
		vdisp->force_dimmng_control = 1;
	}
	SDE_ATRACE_END("dsi_panel_set_hbm");
	return rc;
}

static void dsi_panel_ud_hbm_on(struct work_struct *work)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp)
		return;

	panel = (struct dsi_panel *)vdisp->msm_private;
	mutex_lock(&panel->panel_lock);
	vdisp->ud_hbm_sync.ud_flag = 1;
	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for ud hbm\n");
		complete_all(&vdisp->ud_hbm_sync.ud_hbm_gate);
		mutex_unlock(&panel->panel_lock);
		return;
	}
	dsi_panel_set_hbm_ud_fingerprint(4);
	complete_all(&vdisp->ud_hbm_sync.ud_hbm_gate);
	dsi_panel_set_hbm_ud_fingerprint(2);
	mutex_unlock(&panel->panel_lock);
}

static void dsi_panel_ud_hbm_off(struct work_struct *work)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp)
		return;
	panel = (struct dsi_panel *)vdisp->msm_private;
	mutex_lock(&panel->panel_lock);
	vdisp->ud_hbm_sync.ud_flag = 2;
	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for ud hbm\n");
		complete_all(&vdisp->ud_hbm_sync.ud_hbm_gate);
		mutex_unlock(&panel->panel_lock);
		return;
	}
	dsi_panel_set_hbm_ud_fingerprint(5);
	complete_all(&vdisp->ud_hbm_sync.ud_hbm_gate);
	dsi_panel_set_hbm_ud_fingerprint(3);
	mutex_unlock(&panel->panel_lock);
}

static void dsi_panel_sre_dimming_set(struct dsi_panel *panel, int dimming_lvl)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	int i;
	int step = vdisp->lcm_sre_level / dimming_lvl;
	int sre_lvl = vdisp->lcm_sre_level;

	if (vdisp->lcm_sre_max_level < dimming_lvl) {
		dimming_lvl = vdisp->lcm_sre_max_level;
		step = 1;
	}

	if (vdisp->lcm_sre_level < dimming_lvl)
		goto exit;

	for (i = 0; i < dimming_lvl; i++) {
		sre_lvl -= step;
		if (sre_lvl > 0) {
			vdisp->lcm_sre_level = sre_lvl;
			dsi_panel_update_sre(panel, sre_lvl);
			usleep_range(33000, 33100);
			LCD_INFO("force close sre_lvl=%d,step=%d,dimming_lvl=%d\n", sre_lvl, step, dimming_lvl);
		}
	}

exit:
	vdisp->lcm_sre_level = 0;
	dsi_panel_update_sre(panel, 0);
	LCD_INFO(">>>>>>force close sre\n");
}

static int vivo_adjust_cabc(int bl_lvl, bool enable)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct dsi_panel *panel = vdisp->msm_private;

	if (!panel)
		return -EINVAL;

	if (vdisp->is_cabc_recover && bl_lvl < 856 && !enable) {
		//when brightness down to 50 nit,close cabcs
		dsi_panel_update_cabc(panel, 0);
		LCD_INFO("dsi_stroe_data %d, recover %d\n", vdisp->g_cabc_state, vdisp->is_cabc_recover);
	}

	if (!vdisp->is_cabc_recover && bl_lvl >= 856 && enable) {
		vdisp->tft_cabc_state = vdisp->g_cabc_state;//restore data
		vdisp->is_cabc_recover = 1;//recover only once
		dsi_panel_update_cabc(panel, vdisp->g_cabc_state);
		LCD_INFO("dsi_restroe_data %d, recover %d\n", vdisp->g_cabc_state, vdisp->is_cabc_recover);
	}
	return 0;
}

int vivo_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	struct vivo_display_driver_data *vdisp = NULL;

	vdisp = panel->panel_private;
	LCD_INFO("[%s]set backlight bl_lvl = %d, panel->pre_bkg = %d\n",
			panel->name, bl_lvl, panel->pre_bkg);
	vdisp->userspace_bl_lvl = bl_lvl;
	/* lcd panel backlight control */
	if (vdisp->panel_type == DSI_PANEL_TFT) {
		if (bl_lvl && vdisp->lcm_sre_level > 0 &&
			bl_lvl < vdisp->sre_min_brightness)
			dsi_panel_sre_dimming_set(panel, 16);

		rc = vivo_adjust_cabc(bl_lvl, false);

		if (panel->pre_bkg == 0 && bl_lvl > 0) {
			dsi_panel_update_backlight(panel, bl_lvl);
			panel->pre_bkg = bl_lvl;
			vivo_adjust_cabc(bl_lvl, true);
			LCD_INFO("dsi open backlight\n");
			return 0;
		}

		if (bl_lvl == 0) {
			struct mipi_dsi_device *dsi = &panel->mipi_device;

			if (panel->pre_bkg == 0)
				return 0;

			/* set dimming off */
			rc = mipi_dsi_dcs_write(dsi, 0x53, (u8[]){0x24}, 1);
			LCD_INFO("dsi close backlight\n");
		}

		dsi_panel_update_backlight(panel, bl_lvl);
		panel->pre_bkg = bl_lvl;
		vivo_adjust_cabc(bl_lvl, true);
		return 0;
	}

	if (vdisp->dc_enable) {
		if (bl_lvl < vdisp->dc_dimming && bl_lvl > 0) {
			/* set seed */
			compensate_seed_for_dc_dimming(panel, bl_lvl);
			LCD_ERR("%s:bl_lvl(%d) < dc_dimming(%d) ramain backlight.", __func__, bl_lvl, vdisp->dc_dimming);
			bl_lvl = vdisp->dc_dimming;
			panel->bl_config.bl_level = bl_lvl;
		}
	}

	/* oled panel backlight control */
	if (panel->doze_mode_state) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HLPM_HIGHT_BRIGHTNESS);
		if (rc)
			LCD_ERR("[%s] failed CMD_SET_HLPM_HIGHT, rc=%d\n", panel->name, rc);
		panel->oled_hlpm_state = 1;
		panel->bl_config.bl_level = 0;
		LCD_INFO("set backlight0 to hlpmmode bl_level = %d ..\n", panel->bl_config.bl_level);
		return rc;
	}

	if (panel->pre_bkg == 0 && bl_lvl > 0) {
		LCD_INFO("hbm for resume state %d%d%d..\n", vdisp->ud_hbm_sync.ud_flag, panel->ud_hbm_state, panel->oled_hbm_state);
		if (vdisp->ud_hbm_sync.ud_flag == 2 && panel->ud_hbm_state == UD_HBM_ON) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISPLAY_ON);
			if (rc)
				 LCD_ERR("[%s] failed CMD_SET_DISPLAYON, rc=%d\n", panel->name, rc);
			dsi_panel_update_hbm_ud_fingerprint(panel, 5);
		} else if (vdisp->ud_hbm_sync.ud_flag == 1) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISPLAY_ON);
			if (rc)
				LCD_ERR("[%s] failed SET_DISPLAY, rc=%d\n", panel->name, rc);
			dsi_panel_update_hbm_ud_fingerprint(panel, 4);
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISPLAY_ON);
			if (rc)
				LCD_ERR("[%s] failed CMD_SET_DISPLAY, rc=%d\n", panel->name, rc);

			if (vdisp->smart_aod.is_support && vdisp->colour_gamut_state)
				dsi_panel_update_seed_crc(panel, vdisp->colour_gamut_state);

			dsi_panel_update_backlight(panel, bl_lvl);
		}

		panel->pre_bkg = bl_lvl;

		if (panel->oled_hbm_state > PANEL_HBM_OFF && panel->oled_hbm_state < PANEL_HBM_UD_ON && bl_lvl >= vdisp->sre_threshold) {
			if (panel->oled_hbm_state == PANEL_HBM_SRE_LEVEL1)
				dsi_panel_update_hbm(panel, 1);
			else
				dsi_panel_update_hbm(panel, 2);

			vdisp->oled_hbm_on = 1;
		}
		if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 1)
				dsi_panel_update_crc_compensate(panel, bl_lvl);

		dsi_panel_update_acl(panel, vdisp->oled_acl_state);
		LCD_INFO("dsi open backlight\n");
		return rc;
	}
	/* it can not be set backlight when UDfingerprint HBM on */
	if (panel->ud_hbm_state == UD_HBM_ON && bl_lvl > 0) {
		LCD_INFO("dsi it can not set backlight while UDFingerprint HBM on.\n");
		return rc;
	}
	if (bl_lvl != 0 && vdisp->oled_dimming_enable == 0) {
		if (vdisp->force_dimmng_control) {
			pr_info("[%s] dsi backlight set dimming enable\n", panel->name);
			if (vdisp->ud_fingerprint_hbm_off_state == 1) {
				usleep_range(20000, 20100);
				vdisp->ud_fingerprint_hbm_off_state = 0;
			}
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_SPEED);
			usleep_range(20000, 20100);
			if (rc)
				LCD_ERR("[%s] failed SET_DIM, rc=%d\n", panel->name, rc);
			vdisp->oled_dimming_enable = 1;
		}
	}

	if (bl_lvl == 0) {
		pr_info("[%s] dsi close backlight\n", panel->name);
		vdisp->oled_dimming_enable = 0;
		vdisp->force_dimmng_control = 1;
		vdisp->color_gamut_compensate_state = 0;
		if (panel->pre_bkg == 0)
			return rc;
	}

	panel->pre_bkg = bl_lvl;
	if (panel->oled_hbm_state > PANEL_HBM_OFF && panel->oled_hbm_state < PANEL_HBM_UD_ON && bl_lvl >= vdisp->sre_threshold) {
		dsi_panel_update_backlight(panel, bl_lvl);
		if (panel->oled_hbm_state == PANEL_HBM_SRE_LEVEL1)
			dsi_panel_update_hbm(panel, 1);
		else
			dsi_panel_update_hbm(panel, 2);
		vdisp->oled_hbm_on = 1;
	} else if (panel->oled_hbm_state != PANEL_HBM_OFF && bl_lvl < vdisp->sre_threshold && vdisp->oled_hbm_on == 1) {
		dsi_panel_update_hbm(panel, 0);
		dsi_panel_update_backlight(panel, bl_lvl);
		vdisp->oled_hbm_on = 0;
	} else {
		if ((panel->ud_hbm_state == UD_HBM_ON) && (bl_lvl == 0))
			dsi_panel_update_hbm_ud_fingerprint(panel, 5);
		else
			dsi_panel_update_backlight(panel, bl_lvl);

		if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 1)
			dsi_panel_update_crc_compensate(panel, bl_lvl);
	}
	return rc;
}

struct panel_ud_hbm_sync *get_ud_hbm_sync(void)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("get_ud_hbm_sync vdisp is NULL\n");
		return NULL;
	}

	return &vdisp->ud_hbm_sync;
}

void dsi_panel_hbm_ud_ctrl(bool on)
{
	int const hbm_timeout = msecs_to_jiffies(8*30);
	struct panel_ud_hbm_sync *ud_hbm_sync;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("dsi_panel_hbm_ud_ctrl vdisp is NULL\n");
		return;
	}

	if (!vdisp->is_ud_fingerprint) {
		LCD_ERR("dsi ud not support\n");
		return;
	}

	ud_hbm_sync = &vdisp->ud_hbm_sync;
	if (!ud_hbm_sync) {
		LCD_ERR("dsi get_ud_hbm_sync fail\n");
		return;
	}

	if (on) {
		reinit_completion(&ud_hbm_sync->ud_hbm_gate);
		queue_work(ud_hbm_sync->ud_hbm_workq, &ud_hbm_sync->ud_hbm_on_work);
		if (!wait_for_completion_timeout(&ud_hbm_sync->ud_hbm_gate, hbm_timeout))
			LCD_ERR("ud--dsi--wait hbm gate failed when hbm on\n");
	} else {
		reinit_completion(&ud_hbm_sync->ud_hbm_gate);
		queue_work(ud_hbm_sync->ud_hbm_workq, &ud_hbm_sync->ud_hbm_off_work);
		if (!wait_for_completion_timeout(&ud_hbm_sync->ud_hbm_gate, hbm_timeout))
			LCD_ERR("ud--dsi--wait hbm gate failed when hbm off\n");
	}
}

/* show lcm id */
static ssize_t lcm_id_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->panel_id);
}

static struct kobj_attribute lcm_id_attribute =
__ATTR(lcm_id, 0444, lcm_id_show, NULL);

/* ALPM mode begin */
static ssize_t oled_alpmmode_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_alpmmode_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", panel->oled_alpm_state);
}

static ssize_t oled_alpmmode_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	int rc = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_alpmmode_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;
	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
	  LCD_ERR("dsi Invalid panel panel_initialized for alpmmode\n");
	  mutex_unlock(&panel->panel_lock);
	  return count;
	}

	if (!panel->doze_mode_state) {
	  LCD_ERR("func: %s when panel no in doze mode state\n", __func__);
	  mutex_unlock(&panel->panel_lock);
	  return count;
	}

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
	  LCD_ERR("Invalid input for oled_alpmmode_store\n");

	panel->oled_alpm_state = temp;

	/* Any panel specific low power commands/config */

	if (panel->oled_alpm_state)
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ALPM_HIGHT_BRIGHTNESS);
	else
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ALPM_LOW_BRIGHTNESS);

	if (rc)
	  LCD_ERR("[%s] failed to send DSI_CMD_SET_ALPM cmds, rc=%d\n",
			 panel->name, rc);

	mutex_unlock(&panel->panel_lock);

	return count;
}

static struct kobj_attribute oled_alpmmode_attribute =
__ATTR(oled_alpmmode, 0644, oled_alpmmode_show, oled_alpmmode_store);
/* ALPM mode end */

/* HLPM mode begin */
static ssize_t oled_hlpmmode_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_hlpmmode_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;
	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", panel->oled_hlpm_state);
}

static ssize_t oled_hlpmmode_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	int rc = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_hlpmmode_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;
	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for hlpmmode store\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (!panel->doze_mode_state) {
		LCD_ERR("func: %s when panel no in doze mode state\n", __func__);
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
	  LCD_ERR("Invalid input for oled_alpmmode_store\n");

	panel->oled_hlpm_state = temp;
	LCD_ERR("dsi oled_hlpmmode_store %d\n", panel->oled_hlpm_state);
	/* Any panel specific low power commands/config */
	if (panel->oled_hlpm_state)
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HLPM_HIGHT_BRIGHTNESS);
	else
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HLPM_LOW_BRIGHTNESS);

	if (rc)
	  LCD_ERR("[%s] failed to send DSI_CMD_SET_HLPM cmds, rc=%d\n",
			 panel->name, rc);

	mutex_unlock(&panel->panel_lock);
	return count;
}
static struct kobj_attribute oled_hlpmmode_attribute =
__ATTR(oled_hlpmmode, 0644, oled_hlpmmode_show, oled_hlpmmode_store);
/* HLPM mode end */

/* show dimming speed state */
static ssize_t dimming_speed_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("dimming_speed_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}
	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->force_dimmng_control);
}

static ssize_t dimming_speed_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	int rc = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("dimming_speed_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;
	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for dimming_speed_store\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
	  LCD_ERR("Invalid input for diming_speed_store\n");

	if (!panel->bl_config.bl_level) {
		LCD_ERR("sde Invalid panel backlight state for dimming\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}
	LCD_INFO("mdss dimming state is %d\n", vdisp->force_dimmng_control);

	vdisp->force_dimmng_control = temp;
	if (vdisp->force_dimmng_control) {
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_SPEED);
	} else {
	  rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMING_OFF);
	  usleep_range(10000, 10300);
	}
	if (rc)
	  LCD_ERR("[%s] line: %d, rc=%d\n", panel->name, __LINE__, rc);
	mutex_unlock(&panel->panel_lock);

	return count;
}

static struct kobj_attribute diming_speed_attribute =
__ATTR(dimming_speed, 0644, dimming_speed_show, dimming_speed_store);

/* hbm setting begin */
static ssize_t oled_hbm_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_hbm_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;
	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", panel->oled_hbm_state);
}

static ssize_t oled_hbm_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_hbm_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 0, &temp);
	if (ret)
	  LCD_ERR("Invalid input for oled_hbm_store\n");

	LCD_INFO("temp = 0x%x", temp);
	mutex_lock(&panel->panel_lock);
	if (temp == 0 || temp == 1 || temp == 2) {
		if (panel->oled_hbm_state == PANEL_HBM_AOD_UD_ON ||
				panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF ||
				panel->ud_hbm_state == UD_HBM_ON) {
			mutex_unlock(&panel->panel_lock);
			LCD_ERR("sre cannot update hbm state ...\n");
			return count;
		}
	}
	if ((temp & HBM_UD_OFF_SET_BACKLIGHT_FLAG) == HBM_UD_OFF_SET_BACKLIGHT_FLAG) {
		vdisp->ud_hbm_off_first_bl = (temp & 0xFFFF00) >> 8;
		temp = temp & 0xFF;
	} else {
		vdisp->ud_hbm_off_first_bl = 0;
	}
	panel->oled_hbm_state = temp;

	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for hbm\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		vdisp->unset.oled_hbm_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	LCD_INFO("dsi oled hbm level = %d and current backlight = %d\n",
		  panel->oled_hbm_state, panel->bl_config.bl_level);

	if (vdisp->silent_reboot == 1) {
		LCD_ERR("set hbm when silent reboot\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->bl_config.bl_level == 0 && panel->oled_hbm_state == PANEL_HBM_UD_ON) {
		LCD_ERR("dsi can't set hbm 4 while backlight 0,it will be send again when open backlight\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->bl_config.bl_level == 0 && panel->panel_power_mode_state == SDE_MODE_DPMS_OFF) {
		LCD_ERR("dsi Invalid panel power state for HBM\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if ((panel->oled_hbm_state == PANEL_HBM_AOD_UD_ON || panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF
		|| panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF_PERF_V3) && (panel->bl_config.bl_level > 0 || panel->panel_power_mode_state == SDE_MODE_DPMS_OFF)) {
		LCD_ERR("dsi can't set hbm 6 or 7 or 8 while not in doze. bl_level = %d, panel_power_mode_state = %d\n",
				panel->bl_config.bl_level,  panel->panel_power_mode_state);
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->oled_hbm_state == PANEL_HBM_UD_ON || panel->oled_hbm_state == PANEL_HBM_UD_OFF) {
		dsi_panel_update_hbm_ud_fingerprint(panel, panel->oled_hbm_state);
		LCD_ERR("dsi panel cmd send oled hbm state = %d\n", panel->oled_hbm_state);
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->oled_hbm_state == PANEL_HBM_AOD_UD_ON || panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF
		|| panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF_PERF_V3) {
		dsi_panel_update_hbm_aod_ud_fingerprint(panel, panel->oled_hbm_state);
		LCD_ERR("dsi panel cmd send oled hbm state = %d\n", panel->oled_hbm_state);
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->bl_config.bl_level < vdisp->sre_threshold-1) {
		LCD_ERR("dsi Invalid backlight value for normal hbm\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (panel->ud_hbm_state == UD_HBM_ON) {
		LCD_ERR("dsi can not set hbm while UDFingerPrint HBM on. ud_hbm_state = %d\n", panel->ud_hbm_state);
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	dsi_panel_update_hbm(panel, panel->oled_hbm_state);
	mutex_unlock(&panel->panel_lock);
	LCD_ERR("dsi panel cmd send oled hbm state = %d\n", panel->oled_hbm_state);
	return count;
}

static struct kobj_attribute oled_hbm_attribute =
__ATTR(oled_hbm, 0644, oled_hbm_show, oled_hbm_store);

/* show oled acl mode state */
static ssize_t oled_acl_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_acl_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->oled_acl_state);
}

static ssize_t oled_acl_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp, change = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("oled_acl_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		LCD_ERR("Invalid input for oled_acl_store\n");

	mutex_lock(&panel->panel_lock);
	if (vdisp->oled_acl_state != temp) {
		vdisp->oled_acl_state = temp;
		change = 1;
	}

	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for acl\n");
		vdisp->oled_acl_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (!panel->bl_config.bl_level) {
		LCD_ERR("sde Invalid panel backlight state for acl\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		vdisp->unset.oled_acl_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (change) {
		dsi_panel_update_acl(panel, vdisp->oled_acl_state);
		LCD_INFO("msm-dsi oled_cal_state = %d\n", vdisp->oled_acl_state);
	}

	mutex_unlock(&panel->panel_lock);

	return count;
}

static struct kobj_attribute oled_acl_attribute =
__ATTR(oled_acl, 0644, oled_acl_show, oled_acl_store);

/* show tft cabc mode state */
static ssize_t tft_cabc_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("sre_check_enable_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->tft_cabc_state);
}

static ssize_t tft_cabc_store(struct kobject *kobj,
						struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp, change = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("tft_cabc_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret) {
		LCD_ERR("Invalid input for tft_cabc_state\n");
	}

	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		mutex_unlock(&panel->panel_lock);
		LCD_ERR("msm-dsi Invalid panel panel_initialized for cabc\n");
		return count;
	}

	if (!panel->bl_config.bl_level) {
		mutex_unlock(&panel->panel_lock);
		LCD_ERR("Invalid panel backlight state for cabc\n");
		return count;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		vdisp->unset.tft_cabc_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vdisp->tft_cabc_state != temp) {
		vdisp->tft_cabc_state = temp;
		change = 1;
	}
	vdisp->g_cabc_state = vdisp->tft_cabc_state; //save state
	if (change) {
		dsi_panel_update_cabc(panel, vdisp->tft_cabc_state);
		LCD_INFO("msm-dsi tft_cabc_state = %d\n", vdisp->tft_cabc_state);
	}

	mutex_unlock(&panel->panel_lock);

	return count;
}

static struct kobj_attribute tft_cabc_attribute =
__ATTR(lcm_cabc, 0644, tft_cabc_show, tft_cabc_store);

/* SRE enable check begin */
static ssize_t vivo_sre_check_enable_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("sre_check_enable_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x", vdisp->vivo_sre_enable);
}

static ssize_t vivo_sre_check_enable_store(struct kobject *kobj,
					  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("sre_check_enable_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		LCD_ERR("dsi Invalid input for sre enable\n");
	vdisp->vivo_sre_enable = temp;

	return count;
}

static struct kobj_attribute vivo_sre_check_enable_attribute =
__ATTR(sre_enable, 0644, vivo_sre_check_enable_show, vivo_sre_check_enable_store);
/* SRE enable check end */

/* show oled colour gamut mode state */
static ssize_t colour_gamut_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("sre_check_enable_show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->colour_gamut_state);
}

static ssize_t colour_gamut_store(struct kobject *kobj,
						struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp, change = 0;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("colour_gamut_store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		LCD_ERR("Invalid input for colour_gamut_store\n");

	mutex_lock(&panel->panel_lock);

	if (vdisp->colour_gamut_state != temp) {
		vdisp->colour_gamut_state = temp;
		change = 1;
	}

	if (!panel->panel_initialized) {
		LCD_ERR("dsi Invalid panel panel_initialized for colour_gamut\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vdisp->colour_gamut_state == 0 || vdisp->colour_gamut_state == 1 || vdisp->colour_gamut_state == 2) {
		if (change)
			dsi_panel_update_seed_crc(panel, vdisp->colour_gamut_state);
		LCD_ERR("dsi set colour_gamut_state = %d\n", vdisp->colour_gamut_state);
	} else {
		LCD_ERR("invalid value for colour gamut. state = %d\n", vdisp->colour_gamut_state);
	}

	mutex_unlock(&panel->panel_lock);
	return count;
}

static struct kobj_attribute colour_gamut_attribute =
__ATTR(colour_gamut, 0644, colour_gamut_show, colour_gamut_store);

/* show oled seed ore mode state */
static ssize_t oled_ore_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("ore show vdisp is NULL\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, NODE_BUFERR_COUNT, "%02x\n", vdisp->oled_ore_state);
}

static ssize_t oled_ore_store(struct kobject *kobj,
						struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("ore store vdisp is NULL\n");
		return count;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		LCD_ERR("Invalid input for colour_gamut_store\n");

	mutex_lock(&panel->panel_lock);

	vdisp->oled_ore_state = temp;

	if (panel->ud_hbm_state == UD_HBM_ON) {
		mutex_unlock(&panel->panel_lock);
		LCD_ERR("dsi Invalid panel hbm state for oled_ore\n");
		return count;
	}

	if (!panel->panel_initialized) {
		mutex_unlock(&panel->panel_lock);
		vdisp->unset.oled_ore_state = true;
		LCD_ERR("dsi Invalid panel panel_initialized for oled_ore\n");
		return count;
	}

	if (!panel->bl_config.bl_level) {
		LCD_ERR("dsi Invalid panel backlight state for ore\n");
		vdisp->unset.oled_ore_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		vdisp->unset.oled_ore_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	dsi_panel_update_seed_ore(panel, vdisp->oled_ore_state);
	LCD_INFO("dsi set oled_ore level = %d\n", vdisp->oled_ore_state);

	mutex_unlock(&panel->panel_lock);
	return count;
}

static struct kobj_attribute oled_ore_attribute =
__ATTR(oled_ore, 0644, oled_ore_show, oled_ore_store);

static ssize_t lcm_sre_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	return snprintf(buf, 4, "%02x\n", vdisp->lcm_sre_level);
}

static ssize_t lcm_sre_store(struct kobject *kobj,
			     struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, level, change = 0;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct dsi_panel *panel = (struct dsi_panel *)vdisp->msm_private;

	if (!panel) {
		LCD_ERR("invalid panel\n");
		return count;
	}

	ret = kstrtoint(buf, 10, &level);
	if (ret || level < 0) {
		LCD_ERR("Invalid input ret = %d; level = %d\n", ret, level);
		return count;
	}

	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		mutex_unlock(&panel->panel_lock);
		vdisp->unset.lcm_sre_state = true;
		LCD_ERR("Invalid panel panel_initialized\n");
		return count;
	}

	if (panel->pre_bkg < vdisp->sre_min_brightness) {
		vdisp->lcm_sre_level = 0;
		mutex_unlock(&panel->panel_lock);
		LCD_ERR("Invalid panel backlight level(%d) < sre_min_brightness(%d)\n",
			panel->bl_config.bl_level, vdisp->sre_min_brightness);
		return count;
	}

	if (vdisp->lcm_sre_level != level) {
		if (level > vdisp->lcm_sre_max_level)
			vdisp->lcm_sre_level = vdisp->lcm_sre_max_level;
		else
			vdisp->lcm_sre_level = level;

		change = 1;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		vdisp->unset.lcm_sre_state = true;
		mutex_unlock(&panel->panel_lock);
		return count;
	}

	if (change)
		dsi_panel_update_sre(panel, vdisp->lcm_sre_level);

	mutex_unlock(&panel->panel_lock);

	LCD_INFO("set lcm sre level=%d, max level=%d\n", level, vdisp->lcm_sre_max_level);

	return count;
}

static struct kobj_attribute lcm_sre_attribute =
__ATTR(lcm_sre, 0644, lcm_sre_show, lcm_sre_store);

/* show lcm sre max level */
static ssize_t lcm_sre_max_level_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	return snprintf(buf, 4, "%02x\n", vdisp->lcm_sre_max_level);
}

static struct kobj_attribute lcm_sre_max_attribute =
__ATTR(lcm_sre_max_level, 0444, lcm_sre_max_level_show, NULL);

static ssize_t lcm_vdispi_high_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	unsigned int vdispi_high = 0;

	return snprintf(buf, 4, "%02x\n", vdispi_high);
}
static struct kobj_attribute lcm_vdispi_high_attribute =
__ATTR(lcm_vdispi_high, 0444, lcm_vdispi_high_show, NULL);

static ssize_t lcm_esd_check_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct dsi_panel *panel = (struct dsi_panel *)vdisp->msm_private;

	if (!panel) {
		LCD_ERR("invalid panel\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, 4, "%d\n", panel->esd_config.esd_check_screendown);
}

static ssize_t lcm_esd_check_store(struct kobject *kobj,
			     struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct dsi_panel *panel = (struct dsi_panel *)vdisp->msm_private;

	if (!panel) {
		LCD_ERR("invalid panel\n");
		return count;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret || val < 0) {
		LCD_ERR("Invalid input ret = %d; val = %d\n", ret, val);
		return count;
	}

	/* check screen face down from IR sensor */
	panel->esd_config.esd_check_screendown = val;
	LCD_INFO("dsi: esd_check_screendown=%d\n", val);

	return count;
}
static struct kobj_attribute lcm_esd_check_attribute =
__ATTR(vivo_esd_check_ps, 0644, lcm_esd_check_show, lcm_esd_check_store);

static ssize_t lcm_esd_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct dsi_panel *panel = (struct dsi_panel *)vdisp->msm_private;

	if (!panel) {
		LCD_ERR("invalid panel\n");
		return NODE_BUFERR_COUNT;
	}

	return snprintf(buf, 4, "%d\n", panel->esd_config.esd_enabled);
}
static struct kobj_attribute lcm_esd_enable_attribute =
__ATTR(vivo_esd_check_enable, 0444, lcm_esd_enable_show, NULL);

/* dc diming function */
static ssize_t dc_dimming_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp =
		container_of(kobj, struct vivo_display_driver_data, disp_kobject);

	if (!vdisp) {
		LCD_ERR("lcm_highbrightness_show vdisp is NULL\n");
		return snprintf(buf, 5, "%d\n", 0xffff);
	}
	return snprintf(buf, PAGE_SIZE, "%04x\n", vdisp->dc_dimming);
}

static ssize_t dc_dimming_store(struct kobject *kobj,
						struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, temp;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp =
		container_of(kobj, struct vivo_display_driver_data, disp_kobject);

	if (!vdisp) {
		LCD_ERR("dc dimming store vdisp is NULL\n");
		return -EINVAL;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	ret = kstrtoint(buf, 10, &temp);
	if (ret || temp < 0 || temp > panel->bl_config.bl_max_level) {
		LCD_ERR("%s:param error\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	vdisp->dc_dimming = temp;
	mutex_unlock(&panel->panel_lock);

	LCD_ERR("%s:dc_dimming = %d", __func__, vdisp->dc_dimming);
	return count;
}
static struct kobj_attribute dc_dimming_attribute =
	__ATTR(dc_dimming, 0644, dc_dimming_show, dc_dimming_store);

/* get bl level for dc dimming */
static ssize_t bl_level_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp =
		container_of(kobj, struct vivo_display_driver_data, disp_kobject);

	if (!vdisp) {
		LCD_ERR("bl level show vdisp is NULL\n");
		return snprintf(buf, PAGE_SIZE, "%04x\n", 0xffff);
	}

	panel = (struct dsi_panel *)vdisp->msm_private;

	return snprintf(buf, PAGE_SIZE, "%04x\n", panel->bl_config.bl_level);
}
static struct kobj_attribute bl_level_attribute =
	__ATTR(bl_level, 0644, bl_level_show, NULL);

/* smart aod of 1bit ram panel start */
static ssize_t aod_disp_info_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp =
		container_of(kobj, struct vivo_display_driver_data, disp_kobject);
	struct smart_aod_data *aod_data;
	u32 area_id, depth, x_start, y_start, width, height;
	const char *start = buf;
	int ret, i;
	bool is_change = false;
	int max_str_len = 255;

	if (!vdisp) {
		LCD_ERR("vivo display is NULL\n");
		return count;
	}

	panel = (struct dsi_panel *)vdisp->msm_private;
	if (!panel) {
		LCD_ERR("dsi panel is NULL\n");
		return count;
	}

	aod_data = &vdisp->smart_aod;
	if (!aod_data->is_support) {
		LCD_ERR("not support smart aod\n");
		return count;
	}

#define SET_AOD_PARAM(_aod, _id, _name, _v) \
		do { \
			if (_aod->area[_id]._name != _v) { \
				_aod->area[_id]._name = _v; \
				is_change = true; \
			} \
		} while (0)

	for (i = 0; i < SMART_AOD_MAX_AREA_NUM; i++) {
		ret = sscanf(start, "%u %u %u %u %u %u", &area_id, &depth, &x_start, &y_start, &width, &height);
		if (ret < 0) {
			LCD_ERR("read failed\n");
			return ret;
		}

		if (!smart_aod_area_check(vdisp, area_id, depth, x_start, y_start, width, height)) {
			LCD_ERR("area parameters invaild\n");
			return count;
		}

		LCD_INFO("[%u] %ubit ( %u, %u, %u, %u )\n", area_id, depth, x_start, y_start, width, height);

		SET_AOD_PARAM(aod_data, area_id, enable, 1);
		SET_AOD_PARAM(aod_data, area_id, area_id, area_id);
		SET_AOD_PARAM(aod_data, area_id, x_start, x_start);
		SET_AOD_PARAM(aod_data, area_id, y_start, y_start);
		SET_AOD_PARAM(aod_data, area_id, width, width);
		SET_AOD_PARAM(aod_data, area_id, height, height);
		SET_AOD_PARAM(aod_data, area_id, depth_bit, depth);

		start = strnchr(start, max_str_len, ',');
		if (start)
			start += 1;
		else
			break;
	}

	/* clear other area except fingerprint area */
	while (++i < SMART_AOD_MAX_AREA_NUM - 1) {
		SET_AOD_PARAM(aod_data, i, enable, 0);
	}

	if (!smart_aod_total_size_check(aod_data)) {
		LCD_ERR("smart aod display oversize\n");
		return count;
	}

	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		LCD_ERR("%s: Invalid panel panel_initialized\n", __func__);
		goto exit;
	}

	if (!panel->doze_mode_state) {
		LCD_ERR("%s: panel not on aod mode\n", __func__);
		goto exit;
	}

	if (is_change)
		vdisp->smart_aod_update = true;
	else
		LCD_INFO("no update smart aod for no change\n");

exit:
	mutex_unlock(&panel->panel_lock);

	return count;
}

static ssize_t aod_disp_info_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct vivo_display_driver_data *vdisp =
		container_of(kobj, struct vivo_display_driver_data, disp_kobject);
	struct smart_aod_data *aod_data;
	ssize_t count = 0;
	int i = 0;

	if (!vdisp) {
		LCD_ERR("vivo display is NULL\n");
		return count;
	}

	aod_data = &vdisp->smart_aod;
	if (!aod_data->is_support) {
		LCD_ERR("not support smart aod\n");
		return count;
	}

	for (i == 0; i < SMART_AOD_MAX_AREA_NUM; i++) {
		struct aod_area_data *area = &aod_data->area[i];

		if (area->enable) {
			count += snprintf(buf + count, PAGE_SIZE, "[%u] %ubit ( %u %u %u %u )\n",
					area->area_id, area->depth_bit, area->x_start, area->y_start, area->width, area->height);
		}
	}

	return count;
}
/* smart aod of 1bit ram panel end */

static struct kobj_attribute aod_disp_info_attribute =
	__ATTR(aod_disp_info, 0644, aod_disp_info_show, aod_disp_info_store);

static struct attribute *oled_disp_lcm_sys_attrs[] = {
	&lcm_id_attribute.attr,
	&oled_alpmmode_attribute.attr,
	&oled_hlpmmode_attribute.attr,
	&diming_speed_attribute.attr,
	&oled_hbm_attribute.attr,
	&oled_acl_attribute.attr,
	&vivo_sre_check_enable_attribute.attr,
	&colour_gamut_attribute.attr,
	&oled_ore_attribute.attr,
	 &aod_disp_info_attribute.attr,
	&dc_dimming_attribute.attr,
	&bl_level_attribute.attr,
	NULL
};

static struct attribute *tft_disp_lcm_sys_attrs[] = {
	&lcm_id_attribute.attr,
	&vivo_sre_check_enable_attribute.attr,
	&lcm_sre_attribute.attr,
	&lcm_sre_max_attribute.attr,
	&tft_cabc_attribute.attr,
	&lcm_vdispi_high_attribute.attr,
	&lcm_esd_check_attribute.attr,
	&lcm_esd_enable_attribute.attr,
	NULL
};

static ssize_t disp_lcm_object_show(struct kobject *k, struct attribute *attr, char *buf)
{
	struct kobj_attribute *kobj_attr;
	int ret = -EIO;

	kobj_attr = container_of(attr, struct kobj_attribute, attr);

	if (kobj_attr->show)
		ret = kobj_attr->show(k, kobj_attr, buf);

	return ret;
}

static ssize_t disp_lcm_object_store(struct kobject *k, struct attribute *attr,
				const char *buf, size_t count)
{
	struct kobj_attribute *kobj_attr;
	int ret = -EIO;

	kobj_attr = container_of(attr, struct kobj_attribute, attr);

	if (kobj_attr->store)
		ret = kobj_attr->store(k, kobj_attr, buf, count);

	return ret;
}

static void disp_lcm_object_release(struct kobject *kobj)
{
  /* nothing to do temply */
}

static const struct sysfs_ops disp_lcm_object_sysfs_ops = {
	.show = disp_lcm_object_show,
	.store = disp_lcm_object_store,
};

static struct kobj_type disp_lcm_object_type = {
	.sysfs_ops  = &disp_lcm_object_sysfs_ops,
	.release	  = disp_lcm_object_release,
	.default_attrs = oled_disp_lcm_sys_attrs,
};

static int disp_creat_sys_file(struct vivo_display_driver_data *vdisp)
{
	memset(&vdisp->disp_kobject, 0x00, sizeof(vdisp->disp_kobject));

	if (vdisp->panel_type == DSI_PANEL_OLED)
		disp_lcm_object_type.default_attrs = oled_disp_lcm_sys_attrs;
	else if (vdisp->panel_type == DSI_PANEL_TFT)
		disp_lcm_object_type.default_attrs = tft_disp_lcm_sys_attrs;
	else
		disp_lcm_object_type.default_attrs = oled_disp_lcm_sys_attrs;

	if (kobject_init_and_add(&vdisp->disp_kobject, &disp_lcm_object_type, NULL, "lcm")) {
	  kobject_put(&vdisp->disp_kobject);
	  return -ENOMEM;
	}
	kobject_uevent(&vdisp->disp_kobject, KOBJ_ADD);
	vdisp->creat_lcm_id = 1;

	return 0;
}

static void vivo_panel_parse_dt(struct vivo_display_driver_data *vdisp)
{
	int rc;
	int val;
	u64 tmp64 = 0;
	struct device_node *np;
	const char *trig;
	static const char *project_name_tmp;
	struct device_node *timings_np;

	if (!vdisp)
		return;
	np = vivo_get_panel_of(vdisp);

	if (!np)
		return;

	project_name_tmp = of_get_property(np, "qcom,mdss-dsi-project-name", NULL);
	if (!project_name_tmp) {
		LCD_INFO("%d, Project name not specified\n", __LINE__);
	} else {
		strlcpy(&vdisp->project_name[0], project_name_tmp, MDSS_MAX_PANEL_LEN);
		LCD_INFO("Project Name = %s\n", vdisp->project_name);
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-id", &vdisp->panel_id);
	if (rc)
		LCD_ERR("%s:%d panel id dt parse failed\n", __func__, __LINE__);
	else
		LCD_ERR("Panel ID = %d\n", vdisp->panel_id);

	vdisp->sre_enabled = of_property_read_bool(np, "qcom,mdss-panel-sre");
	if (vdisp->sre_enabled)
		vdisp->vivo_sre_enable = 1;


	vdisp->dynamic_dsi_timing_enable = of_property_read_bool(np, "qcom,dynamic-dsi-timing-enable");
	if (vdisp->dynamic_dsi_timing_enable) {
		rc = of_property_read_u64(np,
			"qcom,mdss-dsi-panel-clockrate", &tmp64);
		if (rc == -EOVERFLOW) {
			tmp64 = 0;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-panel-clockrate", (u32 *)&tmp64);
		}
		vdisp->default_bitrate = !rc ? tmp64 : 0;
		rc = of_property_read_u64(np,
			"qcom,mdss-dsi-panel-intent-clockrate", &tmp64);
		if (rc == -EOVERFLOW) {
			tmp64 = 0;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-panel-intent-clockrate", (u32 *)&tmp64);
		}
		vdisp->intent_bitrate = !rc ? tmp64 : 0;
		LCD_INFO("dynamic clockrate %llu and %llu\n", vdisp->default_bitrate, vdisp->intent_bitrate);
	} else {
		LCD_INFO("dynamic clockrate not suport\n");
	}

	trig = of_get_property(np, "qcom,vivo-dsi-panel-type", NULL);
	if (trig) {
		if (!strcmp(trig, "oled"))
			vdisp->panel_type = DSI_PANEL_OLED;
		else if (!strcmp(trig, "tft"))
			vdisp->panel_type = DSI_PANEL_TFT;
		else
			vdisp->panel_type = DSI_PANEL_OLED;
		LCD_INFO("dsi panel type is %d\n", vdisp->panel_type);
	} else {
		vdisp->panel_type = DSI_PANEL_OLED;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-lcm-sre-max-level", &vdisp->lcm_sre_max_level);
	if (!vdisp->lcm_sre_max_level)
		vdisp->lcm_sre_max_level = 2;

	vdisp->vivo_sre_enable = of_property_read_bool(np, "qcom,mdss-panel-sre");
	vdisp->is_ud_fingerprint = of_property_read_bool(np, "qcom,ud-fingerprint-support");
	vdisp->is_ddic_power_aod = of_property_read_bool(np, "qcom,aod-power-ddic");
	/*esd check enable*/
	vdisp->is_real_esd_check = of_property_read_bool(np, "qcom,real-esd-check-enabled");

	/*dc dimming parse*/
	vdisp->dc_enable = of_property_read_bool(np, "qcom,dc-dimming-support");
	if (vdisp->dc_enable) {
		rc = of_property_read_u32(np, "qcom,dc-dimming-wait-te-count", &vdisp->dc_dimming_wait_te_cnt);
		if (rc)
			vdisp->dc_dimming_wait_te_cnt = 0;

		rc = of_property_read_u32(np, "qcom,dc-dimming-close-dither-threshold", &vdisp->close_dither_threshold);
		if (rc)
			vdisp->close_dither_threshold = 115;
	}
	LCD_INFO("dc dimming feature = <%s,%d,%d,%d>\n", vdisp->dc_enable ? "enable" : "disable", vdisp->dc_dimming_pre_wait_te_cnt, vdisp->dc_dimming_wait_te_cnt,
			vdisp->close_dither_threshold);


	/* resolution parse */
	timings_np = of_get_child_by_name(np, "qcom,mdss-dsi-display-timings");
	if (timings_np) {
		struct device_node *child_np = of_get_next_child(timings_np, NULL);
		if (child_np) {
			rc = of_property_read_u32(child_np, "qcom,mdss-dsi-panel-width", &val);
			if (!rc)
				vdisp->xres = val;

			rc = of_property_read_u32(child_np, "qcom,mdss-dsi-panel-height", &val);
			if (!rc)
				vdisp->yres = val;
		}
	}

	if (vdisp->xres == 0 || vdisp->yres == 0) {
		vdisp->xres = 1080;
		vdisp->yres = 2400;
		LCD_ERR("parse resoluton failed, set default: %dx%d\n", vdisp->xres, vdisp->yres);
	} else {
		LCD_INFO("panel resolution: %dx%d\n", vdisp->xres, vdisp->yres);
	}

	/* smart aod parse */
	vdisp->smart_aod.is_support = of_property_read_bool(np, "qcom,smart-aod-support");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-aod-bl-level", &val);
	if (!rc)
		vdisp->aod_bl_lvl = val;
	else
		vdisp->aod_bl_lvl = 200;
	LCD_INFO("aod bl_level=%d", vdisp->aod_bl_lvl);
}

static void vivo_panel_parse_cmdline(struct vivo_display_driver_data *vdisp, unsigned int display_type)
{
	char *boot_str = NULL;
	char *str = NULL;
	unsigned long value;
	char *str1 = ":lcm_software_id=0xFF";
	char *str2 = ":lcm_especial_id=0xFF";

	if (display_type == DSI_PRIMARY)
		boot_str = dsi_display_primary;
	else
		boot_str = dsi_display_secondary;

	/*  add for lcm software id*/
	str = strnstr(boot_str, ":lcm_software_id=0x", strlen(boot_str));
	if (str) {
		strlcpy(str1, str, strlen(str1)+1);
		if (kstrtol(str1 + strlen(":lcm_software_id=0x"), INT_BASE_16,
					(unsigned long *)&value)) {
			LCD_ERR("dsi invalid lcm_software_id: %s.\n",
					boot_str);
		}
		lcm_software_id = value;
		LCD_INFO("dsi lcm_software_id = 0x%x\n", lcm_software_id);
	}
	/*  add end */

	/*  add for lcm especial id*/
	str = strnstr(boot_str, ":lcm_especial_id=0x", strlen(boot_str));
	if (str) {
		strlcpy(str2, str, strlen(str2)+1);
		if (kstrtol(str2 + strlen(":lcm_especial_id=0x"), INT_BASE_16,
					(unsigned long *)&value)) {
			LCD_ERR("dsi invalid lcm_especial_id: %s.\n",
					boot_str);
		}
		lcm_especial_id = value;
		LCD_INFO("dsi lcm_especial_id = 0x%x\n", lcm_especial_id);
	}
	/*  add end */
}

void dsi_panel_for_ud_te_irq_handler(struct dsi_display *display)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp->is_ud_fingerprint)
		return;
	if (vdisp->ud_hbm_sync.te_con) {
		LCD_INFO("dsi-ud---[%s] te_con = %d\n", __func__,
				vdisp->ud_hbm_sync.te_con);
		vdisp->ud_hbm_sync.te_con = 0;
		wake_up_all(&vdisp->ud_hbm_sync.te_wq);
	}
	if (vdisp->ud_hbm_sync.te_con_use_vblank)
		vdisp->ud_hbm_sync.te_con_use_vblank = 0;
}

static void dsi_panel_vblank_irq_handler(struct vivo_display_driver_data *vdisp)
{
	struct panel_ud_hbm_sync *ud_hbm_sync = &vdisp->ud_hbm_sync;

	if (!vdisp)
		return;

	if (vdisp->is_ud_fingerprint) {
		if (ud_hbm_sync->te_con && ud_hbm_sync->te_con_use_vblank) {
			LCD_INFO("dsi-ud---[%s] te_con = %d\n", __func__, ud_hbm_sync->te_con);
			LCD_INFO("yue debug");
			ud_hbm_sync->te_con = 0;
			wake_up_all(&ud_hbm_sync->te_wq);
		}

		if (!ud_hbm_sync->te_con_use_vblank)
			ud_hbm_sync->te_con_use_vblank = 1;
	}
}

void dsi_panel_ud_work_deinit(struct vivo_display_driver_data *vdisp)
{
	if (!vdisp->is_ud_fingerprint)
		return;

	cancel_work_sync(&vdisp->ud_hbm_sync.ud_hbm_on_work);
	cancel_work_sync(&vdisp->ud_hbm_sync.ud_hbm_off_work);
	if (vdisp->ud_hbm_sync.ud_hbm_workq)
		destroy_workqueue(vdisp->ud_hbm_sync.ud_hbm_workq);
}

void dsi_panel_ud_register_te_irq(struct dsi_display *display)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp->is_ud_fingerprint)
		return;
	vdisp->ud_hbm_sync.panel_te_gpio = display->disp_te_gpio;
}

void dsi_panel_ud_te_irq_by_rd_ptr(int irq_idx)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct panel_ud_hbm_sync *ud_hbm_sync = NULL;

	if (!vdisp->is_ud_fingerprint)
		return;
	ud_hbm_sync = &vdisp->ud_hbm_sync;
	if ((irq_idx == 12) && (ud_hbm_sync != NULL)) {
		ud_hbm_sync->te_time = ktime_get();
		if (ud_hbm_sync->te_con && ud_hbm_sync->te_con_use_vblank) {
			LCD_INFO("dsi-ud---[%s] te_con = %d\n", __func__, ud_hbm_sync->te_con);
			ud_hbm_sync->te_con = 0;
			wake_up_all(&ud_hbm_sync->te_wq);
		}
	}
}

static void dsi_panel_update_hbm_aod_ud(void)
{
	int rc;
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("is_panel_backlight_on vdisp is NULL\n");
		return;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	pr_info("%s: enter, bl=%d\n", __func__, panel->bl_config.bl_level);

	mutex_lock(&panel->panel_lock);

	if (!panel) {
		pr_err("%s:panel is null", panel);
		mutex_unlock(&panel->panel_lock);
		return;
	}

	if (panel->bl_config.bl_level == 0) {
		rc = dsi_panel_update_backlight(panel, panel->bl_config.bl_max_level);
		if (rc)
			goto error;

		if (panel->doze_mode_state == 1) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_AOD_OFF);
			if (rc) {
				pr_err("[%s] failed to send aod-off cmd, rc = %d\n", __func__, rc);
				goto error;
			}
		}

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISPLAY_ON);
		if (rc) {
			pr_err("[%s] failed to send display-on cmd, rc = %d\n", __func__, rc);
			goto error;
		}

		rc = dsi_panel_update_elvss_dimming(panel, 0);
		if (rc)
			goto error;

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_HBM_ON_UD_FINGERPRINT);
		if (rc) {
			pr_err("[%s] failed to send hbm cmd, rc = %d\n", __func__, rc);
			goto error;
		}

		/* start
		add by yuejingqin for pd2038 for the senario that trigger the ud_hbm when pressing the fingerprint again after fingerprint unlock fail*/
		if (vdisp->smart_aod.is_support && panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DIMMING_OFF_HBM_UD_ON);
			if (rc) {
				pr_err("[%s] failed to send hbm cmd, rc = %d\n", __func__, rc);
				goto error;
			}
		}
		/*end*/

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PS_OFF);
		if (rc) {
			pr_err("[%s] failed to send ps-off cmd, rc = %d\n", __func__, rc);
			goto error;
		}

		atomic_set(&after_hbm_count, 1);
		panel->doze_mode_state = 0;
		panel->ud_hbm_state = UD_HBM_ON;
		panel->oled_hbm_state = PANEL_HBM_AOD_UD_ON;
	}

error:
	mutex_unlock(&panel->panel_lock);

	pr_info("%s: exit\n", __func__);
}

int dsi_panel_update_hbm_aod_ud_fingerprint_for_tp(void)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	struct panel_ud_hbm_sync *ud_hbm_sync;

	if (!vdisp) {
		LCD_ERR("vdisp is NULL\n");
		return -EINVAL;
	}

	panel = (struct dsi_panel *)vdisp->msm_private;

	pr_info("%s: enter\n", __func__);

	mutex_lock(&panel->panel_lock);
	if (panel->panel_power_mode_state == SDE_MODE_DPMS_OFF) {
		pr_err("open hbm failed,bl_level=%d,panel_power_mode_state=%d",
			panel->bl_config.bl_level, panel->panel_power_mode_state);
		/*xxb add for smart aod panel start*/
		vdisp->oled_hbm_state = PANEL_HBM_POWER_OFF;
		/*xxb add for smart aod panel start*/
		panel->oled_hbm_state = PANEL_HBM_UD_ON;
		vdisp->unset.oled_hbm_state = true;
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}

	if (!panel || !panel->panel_initialized) {
		pr_err("panel is null or panel not initialized\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}

	ud_hbm_sync = &vdisp->ud_hbm_sync;
	if (!ud_hbm_sync) {
		pr_err("dsi get_ud_hbm_sync fail\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}

	if (vivo_panel_unset_mode_state(vdisp)) {
		LCD_ERR("dsi error unset mode state\n");
		panel->oled_hbm_state = PANEL_HBM_UD_ON;
		vdisp->unset.oled_hbm_state = true;
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);
	dsi_panel_update_hbm_aod_ud();

	pr_info("%s: exit\n", __func__);

	return 0;
}
EXPORT_SYMBOL(dsi_panel_update_hbm_aod_ud_fingerprint_for_tp);

static void vivo_panel_ud_init(struct vivo_display_driver_data *vdisp)
{
	if (!vdisp) {
		LCD_ERR("vivo_panel_ud_init vdisp is NULL\n");
		return;
	}

	vdisp->ud_hbm_sync.te_time = ktime_get();
	vdisp->ud_hbm_sync.te_con = 0;
	vdisp->ud_hbm_sync.te_con_use_vblank = 0;
	vdisp->ud_hbm_sync.ud_flag = 2;
	init_waitqueue_head(&vdisp->ud_hbm_sync.te_wq);
	vdisp->ud_hbm_sync.ud_hbm_workq = create_singlethread_workqueue("dsi_ud_hbm_workq");
	if (!vdisp->ud_hbm_sync.ud_hbm_workq) {
		LCD_ERR("dsi-ud--create dsi_ud_hbm_workq error\n");
		return;
	}

	INIT_WORK(&vdisp->ud_hbm_sync.ud_hbm_on_work, dsi_panel_ud_hbm_on);
	INIT_WORK(&vdisp->ud_hbm_sync.ud_hbm_off_work, dsi_panel_ud_hbm_off);
	init_completion(&vdisp->ud_hbm_sync.ud_hbm_gate);
}

bool is_panel_backlight_on(void)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("is_panel_backlight_on vdisp is NULL\n");
		return PANEL_BACKLIGHT_OFF;
	}
	panel = (struct dsi_panel *)vdisp->msm_private;

	if (panel && panel->bl_config.bl_level && panel->panel_initialized)
		return PANEL_BACKLIGHT_ON;
	else
		return PANEL_BACKLIGHT_OFF;
}

static unsigned int dsi_report_lcm_id(struct vivo_display_driver_data *vdisp)
{
	LCD_INFO("software id from UEFI is 0x%02x, panel id is 0x%02x\n",
		lcm_software_id, vdisp->panel_id);

	return vdisp->panel_id;
}

/* add lcm report id interface for TP use */
unsigned int mdss_report_lcm_id(void)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("mdss_report_lcm_id vdisp is NULL\n");
		return 0xFF;
	}

	dsi_report_lcm_id(vdisp);
	return vdisp->panel_id;
}
EXPORT_SYMBOL(mdss_report_lcm_id);

void vivo_panel_init(struct dsi_panel *panel)
{
	struct vivo_display_driver_data *vdisp = NULL;

	LCD_INFO("vivo_panel_init\n");
	vdisp = &vivo_date;
	panel->panel_private = vdisp;
	vdisp->msm_private = panel;

	/* parse display dtsi node */
	vivo_panel_parse_dt(vdisp);
	/*  parse display cmdline */
	vivo_panel_parse_cmdline(vdisp, DSI_PRIMARY);

	vdisp->silent_reboot = silent_reboot;

	vdisp->lcm_especial_id = lcm_especial_id;

	vdisp->force_dimmng_control = 1;
	vdisp->sre_min_brightness = panel->bl_config.bl_max_level - 1;
	vdisp->sre_threshold = panel->bl_config.brightness_max_level - 1;
	vdisp->is_cabc_recover = -1;
	vdisp->g_cabc_state = -1;
	vdisp->panel_ops.vblank_irq_handler = NULL;

	if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
		vdisp->ud_hbm_sync.te_con_use_vblank = 1;
		vdisp->panel_ops.vblank_irq_handler = dsi_panel_vblank_irq_handler;
	}

	if (vdisp->is_ud_fingerprint)
		vivo_panel_ud_init(vdisp);
	disp_creat_sys_file(vdisp);

	if (vdisp->smart_aod.is_support)
		vivo_smart_aod_init(vdisp, &vdisp->smart_aod);

}

void vivo_panel_deinit(struct dsi_panel *panel)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	dsi_panel_ud_work_deinit(vdisp);
}

int dsi_panel_unset_state_reset(struct dsi_panel *panel)
{
	struct vivo_display_driver_data *vdisp = panel->panel_private;
	int rc = 0;

	if (!vdisp)
		return -ENODEV;

	if (vdisp->panel_type == DSI_PANEL_TFT) {
		if (vdisp->unset.tft_cabc_state) {
			rc = dsi_panel_update_cabc(panel, vdisp->tft_cabc_state);
			if (rc)
				goto error;

			vdisp->unset.tft_cabc_state = false;
		}

		if (vdisp->unset.lcm_sre_state) {
			rc = dsi_panel_update_sre(panel, vdisp->lcm_sre_level);
			if (rc)
				goto error;

			vdisp->unset.lcm_sre_state = false;
		}
	} else { /* oled */
		if (vdisp->unset.oled_acl_state) {
			rc = dsi_panel_update_acl(panel, vdisp->oled_acl_state);
			if (rc)
				goto error;

			vdisp->unset.oled_acl_state = false;
		}

		if (vdisp->unset.oled_ore_state) {
			rc = dsi_panel_update_seed_ore(panel, vdisp->oled_ore_state);
			if (rc)
				goto error;

			vdisp->unset.oled_ore_state = false;
		}

		if (vdisp->unset.oled_hbm_state) {
			if (panel->oled_hbm_state == PANEL_HBM_AOD_UD_OFF && panel->bl_config.bl_level == 0)
				dsi_panel_update_hbm_aod_ud_fingerprint(panel, panel->oled_hbm_state);
			else
				dsi_panel_update_hbm_ud_fingerprint(panel, panel->oled_hbm_state);
			if (rc)
				goto error;

			vdisp->unset.oled_hbm_state = false;
		}
	}

error:
	return rc;
}

bool get_dc_dimming_state(void)
{
	struct dsi_panel *panel = NULL;
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();

	if (!vdisp) {
		LCD_ERR("get_dc_dimming_state vdisp is NULL\n");
		return false;
	}

	if (!vdisp->dc_enable) {
		return false;
	}

	panel = (struct dsi_panel *)vdisp->msm_private;
	if (!panel) {
		pr_err("%s:panel is null", __func__);
		return false;
	}
	/* mutex_lock(&panel->panel_lock); */
	/*  dc dimming state is true condition:
			1.dc_dimming > 0
			2.not in ud state
			3.not in AOD
			4.bl_level > close_dither_threshold
	*/
	if (vdisp->dc_dimming > 0
		&& vdisp->ud_hbm_sync.ud_flag != 1 && panel->ud_hbm_state != UD_HBM_ON
		&& panel->doze_mode_state != 1
		&& vdisp->userspace_bl_lvl > vdisp->close_dither_threshold) {
		/* mutex_unlock(&panel->panel_lock); */
		return true;
	}
	/* mutex_unlock(&vdisp->panel_lock); */
	return false;
}
EXPORT_SYMBOL(get_dc_dimming_state);

bool compare_project_name(const char *project_name)
{
	struct vivo_display_driver_data *vdisp = vivo_get_vdisp();
	if (project_name == NULL)
		return false;
	if (vdisp && !strcmp(vdisp->project_name, project_name))
		return true;
	return false;
}
EXPORT_SYMBOL(compare_project_name);
