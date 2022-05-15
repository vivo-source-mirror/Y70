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

#ifndef VIVO_SRC_PANEL_COMMON_H
#define VIVO_SRC_PANEL_COMMON_H

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/err.h>
#include <linux/lcd.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <video/mipi_display.h>
#include <linux/regulator/consumer.h>

#include "dsi_display.h"
#include "dsi_panel.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_encoder_phys.h"
#include "dsi_panel.h"

#define LOG_KEYWORD "[SDE]"

#define LCD_INFO(X, ...) pr_info("%s %s : "X, LOG_KEYWORD, __func__, ## __VA_ARGS__)
#define LCD_ERR(X, ...) pr_err("%s %s : "X, LOG_KEYWORD, __func__, ## __VA_ARGS__)
#define NODE_BUFERR_COUNT 4
#define GET_DSI_PANEL(vdisp)	((struct dsi_panel *) (vdisp)->msm_private)
#define INT_BASE_16 16

#define UD_FLAG_DC_DIMMING_OPEN			0x10000000
#define UD_FLAG_DC_DIMMING_CLOSE		0x20000000
#define UD_FLAG_DC_DIMMING_DROP_OPEN	0x40000000
#define UD_FLAG_DC_DIMMING_DROP_CLOSE	0x80000000
#define UD_FLAG_DC_DIMMING_MASK		(UD_FLAG_DC_DIMMING_OPEN | UD_FLAG_DC_DIMMING_CLOSE | UD_FLAG_DC_DIMMING_DROP_OPEN | UD_FLAG_DC_DIMMING_DROP_CLOSE)

#define HBM_UD_OFF_SET_BACKLIGHT_FLAG   0x10000008

enum vivo_panel_type {
	DSI_PANEL_UNKNOW = 0,
	DSI_PANEL_OLED,
	DSI_PANEL_TFT,
	DSI_PANEL_MAX
};

enum vivo_backlight_status {
	PANEL_BACKLIGHT_OFF = 0,
	PANEL_BACKLIGHT_ON
};

enum panel_hbm {
	PANEL_HBM_OFF = 0,
	PANEL_HBM_SRE_LEVEL1,/* 1 */
	PANEL_HBM_SRE_LEVEL2,/* 2 */
	PANEL_HBM_SRE_LEVEL3, /*not used*/
	PANEL_HBM_UD_ON,/* 4 */
	PANEL_HBM_UD_OFF,/* 5 */
	PANEL_HBM_AOD_UD_ON,/* 6 */
	PANEL_HBM_AOD_UD_OFF,/* 7 */
	PANEL_HBM_AOD_UD_OFF_PERF_V3, /* 8 */
	PANEL_HBM_PS_OFF,/* 9 */
	PANEL_HBM_POWER_OFF, /*10*/
	PANEL_HBM_MAX
};

enum ud_hbm {
	UD_HBM_OFF = 0,
	UD_HBM_ON,
	UD_HBM_MAX
};
struct panel_ud_hbm_sync {
	bool te_con;
	bool te_con_use_vblank;
	int panel_te_gpio;
	int ud_flag;

	struct completion ud_hbm_gate;
	struct workqueue_struct *ud_hbm_workq;
	struct workqueue_struct *hbm_workq;
	struct work_struct hbm_on_work;
	struct work_struct ud_hbm_on_work;
	struct work_struct ud_hbm_off_work;

	wait_queue_head_t te_wq;
	ktime_t te_time;
};

struct cmd_panel_te_check {
	struct timer_list te_swt;
	struct work_struct te_check_work;
	long long te_duration;
	long long *te_time;
	int sampling_rate;
	bool te_check_enable;
	unsigned int te_check_done;
	ktime_t te_check_start;
	struct completion te_check_comp;
};

struct cmd_te {
	int panel_te_gpio;
	int refcount;
};

/* smart aod setting for 1bit ram panel start */
enum color_mode {
	AOD_COLOR_WHITE = 0,
	AOD_COLOR_RED,
	AOD_COLOR_GREEN,
	AOD_COLOR_BLUE,
	AOD_COLOR_MAGENTA,
	AOD_COLOR_CYAN,
	AOD_COLOR_YELLOW,
	AOD_COLOR_BLACK,
};

enum color_depth {
	AOD_1_BIT = 0,
	AOD_2_BIT,
	AOD_4_BIT,
	AOD_8_BIT,
};

struct aod_area_data {
	bool enable;
	bool rgb_enable;
	enum color_mode color;
	enum color_depth depth;
	unsigned int depth_bit;
	unsigned char gray;
	unsigned int area_id;
	unsigned int bit_num;
	unsigned short x_start;
	unsigned short y_start;
	unsigned short width;
	unsigned short height;
};

#define SMART_AOD_MAX_AREA_NUM 6

struct smart_aod_data {
	bool is_support;
	struct aod_area_data area[SMART_AOD_MAX_AREA_NUM];
};
/* smart aod setting for 1bit ram panel end */

struct vivo_display_driver_data;

struct panel_ops {

	int (*dsi_write_cmdlist)(struct vivo_display_driver_data *vdisp, enum dsi_cmd_set_type type);
	int (*dsi_write_dcs_cmd)(struct vivo_display_driver_data *vdisp, u8 cmd,
			   const void *data, size_t len);
	int (*dsi_write_cmd)(struct dsi_panel *panel, const void *data, size_t len, int flags);
	int (*update_backlight)(struct dsi_panel *panel, u32 bl_lvl);
	int (*update_elvss_dimming)(struct vivo_display_driver_data *vdisp, int enable);
	int (*set_backlight)(struct vivo_display_driver_data *vdisp, u32 bl_lvl);

	void (*te_sw_pet)(struct vivo_display_driver_data *vdisp, bool enable);
	void (*te_swt_work_deinit)(struct vivo_display_driver_data *vdisp);

	void (*ud_work_deinit)(struct vivo_display_driver_data *vdisp);
	void (*ud_te_irq_use_rdptr)(struct vivo_display_driver_data *vdisp, int irq_index);
	void (*te_irq_handler)(struct vivo_display_driver_data *vdisp);

	int (*update_seed_crc)(struct vivo_display_driver_data *vdisp, u32 crc_lvl);

	void (*ops_init)(struct vivo_display_driver_data *vdisp);
	int (*update_crc_compensate)(struct vivo_display_driver_data *vdisp, u32 bl_lvl);
	void (*vblank_irq_handler)(struct vivo_display_driver_data *vdisp);

	bool (*mode_switch_pending)(struct dsi_panel *panel);
	bool (*unset_mode_state)(struct dsi_panel *panel);

	/* Event */
	//void (*vivo_event_drm_event_callback)(struct vivo_display_driver_data *vdisp, int event, void *arg);

};

struct panel_unset_state {
	bool colour_gamut_state;
	bool oled_acl_state;
	bool tft_cabc_state;
	bool lcm_sre_state;
	bool oled_ore_state;
	bool oled_hbm_state;
};

struct vivo_display_driver_data {
	void *msm_private;
	struct kobject disp_kobject;
	int creat_lcm_id;
	int silent_reboot;
	int colour_gamut_state;
	int lcm_especial_id;
	int lcm_internal_id;
	int ud_fingerprint_hbm_off_state;
	int oled_acl_state;
	int oled_dimming_enable;
	int force_dimmng_control;
	int oled_hbm_on;
	bool sre_enabled;
	int vivo_sre_enable;
	int sre_threshold; //brigghtness > sre_threshold, open sre
	int oled_ore_state;
	int oled_ore_on;
	bool dynamic_dsi_timing_enable;
	u64 default_bitrate;
	u64 intent_bitrate;
	int doze_mode_state; // 0: not into doze; 1: into doze;
	int oled_alpm_state;
	int oled_hlpm_state;
	enum panel_hbm oled_hbm_state;
	int ud_hbm_state;
	enum vivo_panel_type panel_type;
	int tft_cabc_state;
	int is_cabc_recover;
	int g_cabc_state;
	int panel_id;
	u32 userspace_bl_lvl; /*record backlight from userspece*/
	int lcm_sre_level;
	int lcm_sre_max_level;
	int sre_min_brightness;
	bool is_ud_fingerprint;
	bool is_ddic_power_aod;
	struct panel_ud_hbm_sync ud_hbm_sync;
	struct cmd_te te_pin;
	struct cmd_panel_te_check te_sync;
	struct panel_ops panel_ops;
	char project_name[MDSS_MAX_PANEL_LEN];
	int color_gamut_compensate_state;
	bool is_gamut_compensate;
	bool dc_enable;
	int dc_dimming;				/* set dc dimming value by sf */
	u32 dc_dimming_brightness;	/* brightness value when open dc */
	int dc_dimming_pre_wait_te_cnt;  /*backlight from low to high,need some te for sync*/
	int dc_dimming_wait_te_cnt;	/* backlight from high to low,need some te for sync */
	u32 close_dither_threshold; /*When the backlight is lower than a the threshold, close dither*/
	bool is_real_esd_check;
	unsigned int ud_hbm_off_first_bl;
	struct smart_aod_data smart_aod;
	struct panel_unset_state unset;
	u32 xres;
	u32 yres;
	bool aod_display_oning;//smart aod on flag
	bool smart_aod_update;
	u64 display_off_jiffies;
	u64 sleep_out_jiffies;
	u64 lp11_jiffies;
	u64 smart_aod_update_jiffies;
	int aod_bl_lvl;
};

#define VDISP_TO_PANEL(vdisp)	((struct dsi_panel *) (vdisp)->msm_private)
static inline struct device_node *get_panel_device_node(
		struct vivo_display_driver_data *vdisp)
{
	struct dsi_panel *panel = VDISP_TO_PANEL(vdisp);

	return panel->panel_of_node;
}

static inline struct device_node *vivo_get_panel_of(
		struct vivo_display_driver_data *vdisp)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdisp);

	return panel->panel_of_node;
}

static inline bool vivo_panel_mode_switch_pending(struct vivo_display_driver_data *vdisp)
{
	if (vdisp->panel_ops.mode_switch_pending)
		return vdisp->panel_ops.mode_switch_pending(VDISP_TO_PANEL(vdisp));

	return false;
}

static inline bool vivo_panel_unset_mode_state(struct vivo_display_driver_data *vdisp)
{
	if (vdisp->panel_ops.unset_mode_state)
		return vdisp->panel_ops.unset_mode_state(VDISP_TO_PANEL(vdisp));

	return false;
}

struct vivo_display_driver_data *vivo_get_vdisp(void);

void vivo_panel_init(struct dsi_panel *panel);
void vivo_panel_deinit(struct dsi_panel *panel);

int dsi_panel_update_seed_crc(struct dsi_panel *panel, u32 crc_lvl);
int dsi_panel_update_acl(struct dsi_panel *panel, u32 acl_lvl);
int dsi_panel_update_cabc(struct dsi_panel *panel, u32 cabc_lvl);
int vivo_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl);

bool is_panel_backlight_on(void);

void dsi_panel_hbm_ud_ctrl(bool on);
void dsi_panel_for_ud_te_irq_handler(struct dsi_display *display);
void dsi_panel_ud_register_te_irq(struct dsi_display *display);
void dsi_panel_ud_te_irq_by_rd_ptr(int irq_idx);
void dsi_panel_set_dc_dimming_brightness(u32 brightness, u32 state);

void compensate_seed_for_dc_dimming(struct dsi_panel *panel, u32 bl_lvl);
bool smart_aod_total_size_check(struct smart_aod_data *aod_data);
bool smart_aod_area_check(struct vivo_display_driver_data *vdisp, u32 area_id, u32 depth,
			u32 x_start, u32 y_start, u32 width, u32 height);
void vivo_smart_aod_init(struct vivo_display_driver_data *vdisp, struct smart_aod_data *aod_data);
int vivo_smart_aod_set(struct vivo_display_driver_data *vdisp, struct smart_aod_data *aod_data);
void vivo_smart_aod_update(struct vivo_display_driver_data *vdisp);
void vivo_smart_aod_display_on(struct drm_device *dev, struct vivo_display_driver_data *vdisp);
int dsi_panel_unset_state_reset(struct dsi_panel *panel);
void do_away_udelay(u64 old_jiffies, unsigned int delay_us);

#endif /* VIVO_SRC_PANEL_COMMON_H */
