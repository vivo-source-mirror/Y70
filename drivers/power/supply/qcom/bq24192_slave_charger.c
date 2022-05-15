/* Copyright (c) 2013 LGE Inc. All rights reserved.
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

#define pr_fmt(fmt)	"BQ24192:%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/bitops.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
/*#include <linux/qpnp/qpnp-adc.h>*/
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/mm.h>
/*#include <linux/bbk_drivers_info.h>*/
#include "vivo-fuel_summary.h"
#if 1
#ifdef pr_debug
#undef pr_debug
#define pr_debug(fmt, ...) \
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif
#define BQ24192_NAME "bq24192_charger"

/* Register definitions */
#define INPUT_SRC_CONT_REG              0X00
#define PWR_ON_CONF_REG                 0X01
#define CHARGE_CUR_CONT_REG             0X02
#define PRE_CHARGE_TERM_CUR_REG         0X03
#define CHARGE_VOLT_CONT_REG            0X04
#define CHARGE_TERM_TIMER_CONT_REG      0X05
#define IR_COMP_THERM_CONT_REG          0X06
#define MISC_OPERATION_CONT_REG         0X07
#define SYSTEM_STATUS_REG               0X08
#define FAULT_REG                       0X09
#define VENDOR_PART_REV_STATUS_REG      0X0A

#define EN_HIZ_MASK                0x80
#define RESET_REGISTER_MASK        0x80
#define CHG_CONFIG_MASK            0x30
#define EN_CHG_MASK                0x10
#define PG_STAT_MASK               0x04
#define OTG_EN_MASK                0x20
#define VBUS_STAT_MASK             0xC0
#define PRE_CHARGE_MASK            0x10
#define FAST_CHARGE_MASK           0x20
#define CHARGING_MASK (PRE_CHARGE_MASK | FAST_CHARGE_MASK)
#define CHG_DONE_MASK              0x30
#define INPUT_CURRENT_LIMIT_MASK   0x07
#define INPUT_VOLTAGE_LIMIT_MASK   0x78
#define SYSTEM_MIN_VOLTAGE_MASK    0x0E
#define PRECHG_CURRENT_MASK        0xF0
#define TERM_CURRENT_MASK          0x0F
#define CHG_VOLTAGE_LIMIT_MASK     0xFC
#define CHG_CURRENT_LIMIT_MASK     0xFC
#define EN_CHG_TERM_MASK           0x80
#define EN_CHG_TIMER_MASK          0x08
#define I2C_TIMER_MASK             0x30
#define CHG_TIMER_LIMIT_MASK       0x06
#define IR_COMP_R_MASK             0xE0
#define IR_COMP_VCLAMP_MASK        0x1C
#define THERM_REG_MASK             0x03
#define BOOST_LIM_MASK             0x01
#define CHRG_FAULT_MAST			   (BIT(4) | BIT(5))
#define WATCHDOG_TIMER_MAST		   (BIT(4) | BIT(5))
#define WATCHDOG_TIMER_RESET		BIT(6)
#define BQ24192_PN_MASK             0x38
#define BQ24192_PN_SHIFT            3

/* Register 0x0B*/
#define BQ25601D_REG_0B			0x0B
#define BQ25601D_PN_MASK		0x78
#define BQ25601D_PN_SHIFT		3
extern int bq25601d_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int bq25601d_remove(struct i2c_client *client);
extern int bq25601d_suspend(struct device *dev);
extern int bq25601d_resume(struct device *dev);
extern unsigned long totalram_pages;

/* Register 0x14*/
#define BQ25890_REG_14              0x14
#define BQ25890_PN_MASK             0x38
#define BQ25890_PN_SHIFT            3
extern char* get_bbk_board_version(void);
extern int bq25890_probe(struct i2c_client *client,
			   const struct i2c_device_id *id);
extern int bq25890_remove(struct i2c_client *client);
enum ti_part_num{
    BQ25892 = 0x00,
    BQ25601D = 0x02,
    BQ25890 = 0x03,//BQ24192I = 0x03,
    BQ24190 = 0x04,
    BQ24192 = 0x05,
    BQ25895 = 0x07,
};
extern unsigned int parallel_charger_type;
struct bq24192_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};
enum usb_request_vol {
	USB_REQUEST_5V = 5,
	USB_REQUEST_9V = 9,
	USB_REQUEST_12V = 12,
};
/*don't distinguish between normal DCP and HVDCP_TYPE_5V*/
enum hvdcp_type {
	HVDCP_TYPE_UNKOWN = 0,
	HVDCP_TYPE_5V = 5,
	HVDCP_TYPE_9V = 9,
	HVDCP_TYPE_12V = 12,
};
struct hvdcp_detect_voltage {
	int min_mv;
	int max_mv;
	enum hvdcp_type type;
};
enum qc2p0_detect_state {
	QC2P0_DETECT_STATE_UNDEFINED = 0,
	QC2P0_DETECT_STATE_DETERMINE_HVDCP,
	QC2P0_DETECT_STATE_DETERMINE_REQUEST,
	QC2P0_DETECT_STATE_REDUCE_ICURRENT,
	QC2P0_DETECT_STATE_SEND_REQUEST,
	QC2P0_DETECT_STATE_READ_REQUEST,
	QC2P0_DETECT_STATE_AC_POWER_DETECT,
	QC2P0_DETECT_STATE_FAIL,
	QC2P0_DETECT_STATE_DETECT_DONE,
};
enum {
	BQ24192_CHG_OV_STATUS = 0,
	BQ24192_BAT_OV_STATUS = 3,
	BQ24192_CHG_TIMEOUT_STATUS,
	BQ24192_CHG_TERM_STATUS = 10,
	BQ24192_CHG_UV_STATUS,
};

struct bq24192_chip {
	int  chg_current_ma;
	int  input_limit_ma;
	int  term_current_ma;
	int  vbat_max_mv;
	int	 default_vbat_max_mv;
	int  pre_chg_current_ma;
	int  sys_vmin_mv;
	int  vin_limit_mv;
	int  wlc_vin_limit_mv;
	int  int_gpio;
	int  otg_en_gpio;
	int  irq;
	struct i2c_client  *client;
	struct device		*dev;
	struct work_struct  irq_work;
	int  irq_scheduled_time_status;
	spinlock_t irq_work_lock;
	struct delayed_work  vbat_work;
	struct delayed_work  input_limit_work;
	struct delayed_work  therm_work;
	struct dentry  *dent;
	struct wake_lock  chg_wake_lock;
	struct wake_lock  icl_wake_lock;
	struct wake_lock  irq_wake_lock;
	struct power_supply  *usb_psy;
	struct power_supply  *ac_psy;
	struct power_supply  *wlc_psy;
	struct power_supply  *batt_psy;
	struct power_supply	 *bms_psy;
	struct power_supply	 *secondary_ac_psy;
	struct power_supply	 *parallel_psy;
	struct bq24192_regulator		otg_vreg;
	const char			*bms_psy_name;
	const char			*sec_ac_psy_name;
	/*struct qpnp_adc_tm_btm_param  adc_param;*/
	/*struct qpnp_adc_tm_chip		*adc_tm_dev;*/

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_active;
	struct pinctrl_state	*pin_sleep;
	/*struct qpnp_vadc_chip   *vadc_dev;*/
	/*enum qpnp_vadc_channels vadc_channel;*/

	struct delayed_work  qc2p0_detect_work;
	enum qc2p0_detect_state	hvdcp_state;
	enum usb_request_vol 	vol_request;
	enum hvdcp_type			hvdcp_type;
	bool  					is_hvdcp;
	int  					cv_thr_ma;
	int 					vol_output_mv;
	int						hvdcp_retries;
	int						qc2p0_retries;
	bool  					usb_switched;
	bool					sec_chger_on;
	bool					recharging;
	int						rsense_mohm;



	int  usb_online;
	int  ac_online;
	int	 usb_psy_type;
	int  ext_pwr;
	int  wlc_pwr;
	int  wlc_support;
	int  ext_ovp_otg_ctrl;
	int  set_chg_current_ma;
	int  vbat_noti_stat;
	int  cv_thr_mv;
	int  step_dwn_currnet_ma;
	int  icl_idx;
	bool icl_first;
	int  icl_fail_cnt;
	int  icl_vbus_mv;
	int  dwn_chg_i_ma;
	int  up_chg_i_ma;
	int  dwn_input_i_ma;
	int  up_input_i_ma;
	int  wlc_dwn_i_ma;
	int  wlc_dwn_input_i_ma;
	int  batt_health;
	int  saved_ibat_ma;
	int  saved_input_i_ma;
	int  max_input_i_ma;
	int	fake_battery_soc;

	int usbsel_gpio;
	int	qc2sw_dp_gpio;
	int qc2sw_dm_gpio;
	int en_gpio;

	bool  therm_mitigation;
	bool  secondary_charger;
	bool  primary_charger;
	bool  otg_func_support;
	bool  qc2p0_detect_support;
	bool  chg_enabled;

	long	health_status;
	int 	chg_type;
	unsigned long    charge_begin;
	int			chg_tmout_mins;
	bool			parallel_charger;
	bool			parallel_charger_present;
	int				parallel_iindpm_ma;
	struct delayed_work  kick_watchdog_work;
	int			c_charger_temp_max;
	bool	pd1965_flag;
};

#define HVDCP_DETECT_MAX_RETRIES 3
static struct hvdcp_detect_voltage hvdcp_vol[] = {
	{
		.min_mv	= 4150,//4350,
		.max_mv	= 7800,
		.type	= HVDCP_TYPE_5V,
	},
	{
		.min_mv	= 7800,
		.max_mv	= 10300,
		.type	= HVDCP_TYPE_9V,
	},
	{
		.min_mv	= 10300,
		.max_mv	= 14200,
		.type	= HVDCP_TYPE_12V,
	},
};

static struct bq24192_chip *the_chip;
static int input_limit_idx = 0;
struct debug_reg {
	char  *name;
	u32  reg;
};

#define BQ24192_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg bq24192_debug_regs[] = {
	BQ24192_DEBUG_REG(INPUT_SRC_CONT),
	BQ24192_DEBUG_REG(PWR_ON_CONF),
	BQ24192_DEBUG_REG(CHARGE_CUR_CONT),
	BQ24192_DEBUG_REG(PRE_CHARGE_TERM_CUR),
	BQ24192_DEBUG_REG(CHARGE_VOLT_CONT),
	BQ24192_DEBUG_REG(CHARGE_TERM_TIMER_CONT),
	BQ24192_DEBUG_REG(IR_COMP_THERM_CONT),
	BQ24192_DEBUG_REG(MISC_OPERATION_CONT),
	BQ24192_DEBUG_REG(SYSTEM_STATUS),
	BQ24192_DEBUG_REG(FAULT),
	BQ24192_DEBUG_REG(VENDOR_PART_REV_STATUS),
};

struct current_limit_entry {
	int input_limit;
	int chg_limit;
};
/*ibat value must be more than 1024mA in bq24192*/
/*1.5A:1536 2A:2048 2.5A:2560*/
static struct current_limit_entry adap_tbl[] = {
	//{100, 1536},
	//{150, 1536},
	{500, 1536},
	{900, 1536},
	{1200, 1536},
	{1500, 1536},
	//{2000, 1536},
	//{3000, 1536},
};

static int bq24192_vbat_detect_disable(struct bq24192_chip *chip);
static int bq24192_get_soc_from_batt_psy(struct bq24192_chip *chip);
static void bq24192_trigger_recharge(struct bq24192_chip *chip);
static void bq24192_qc2p0_detect_init(struct bq24192_chip *chip);
static void bq24192_qc2p0_detect_deinit(struct bq24192_chip *chip);
static int bq24192_get_vbus_voltage(struct bq24192_chip *chip,int *vbus_mv);
static int bq24192_read_request_result(struct bq24192_chip *chip);
static int bq24192_config_charger_based_updated_voltage(struct bq24192_chip *chip,
																		int input_limit_ma,int chg_limit_ma);
static int bq24192_get_hvdcp_type(struct bq24192_chip *chip,int vbus_mv);
static int bq24192_get_prop_batt_voltage_now(struct bq24192_chip *chip);
static int bq24192_get_prop_batt_current_now(struct bq24192_chip *chip);
static void bq24192_update_bms(struct bq24192_chip *chip);
static int bq24192_dump_qc2p0_pin_status(struct bq24192_chip *chip);
static int bq24192_send_voltage_request(struct bq24192_chip *chip,int request_voltage);
static int bq24192_hw_init(struct bq24192_chip *chip);
static void bq24192_kick_watchdog(struct bq24192_chip *chip);
static void bq24192_disable_watchdog(struct bq24192_chip *chip);



static int is_between(int value, int left, int right)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}

static int bq24192_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c read fail: can't read from %02x: %d\n",
			reg, ret);
		return ret;
	} else {
		*val = ret;
	}
	//pr_debug("read 0x%02X,value=0x%02X\n",reg,*val);
	return 0;
}

static int bq24192_write_reg(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	//return 0;
	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	//pr_debug("write 0x%02X,value=0x%02X\n",reg,val);
	return 0;
}

static int bq24192_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = bq24192_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("bq24192_read_reg failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	temp &= ~mask;
	temp |= val & mask;

	rc = bq24192_write_reg(client, reg, temp);
	if (rc) {
		pr_err("bq24192_write failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	return 0;
}

static bool bq24192_is_charger_present(struct bq24192_chip *chip)
{
	u8 temp;
	bool chg_online;
	int ret;

	ret = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (ret) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", ret);
		return false;
	}

	chg_online = temp & PG_STAT_MASK;
	pr_debug("charger present = %d\n", chg_online);

	return !!chg_online;
}

#define EN_HIZ_SHIFT 7
static int bq24192_enable_hiz(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << EN_HIZ_SHIFT);

	pr_err("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, INPUT_SRC_CONT_REG,
				EN_HIZ_MASK, val);
	if (ret) {
		pr_err("failed to set HIZ rc=%d\n", ret);
		return ret;
	}

	return 0;
}

#define CHG_ENABLE_SHIFT  4
static int bq24192_enable_charging(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << CHG_ENABLE_SHIFT);

	pr_err("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
						EN_CHG_MASK, val);
	if (ret) {
		pr_err("failed to set EN_CHG rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq24192_is_chg_enabled(struct bq24192_chip *chip)
{
	int ret;
	u8 temp;

	ret = bq24192_read_reg(chip->client, PWR_ON_CONF_REG, &temp);
	if (ret) {
		pr_err("failed to read PWR_ON_CONF_REG rc=%d\n", ret);
		return ret;
	}

	return (temp & CHG_CONFIG_MASK) == 0x10;
}

struct input_ma_limit_entry {
	int  icl_ma;
	u8  value;
};

static struct input_ma_limit_entry icl_ma_table[] = {
	{100, 0x00},
	{150, 0x01},
	{500, 0x02},
	{900, 0x03},
	{1200, 0x04},
	{1500, 0x05},
	{2000, 0x06},
	{3000, 0x07},
};

#define INPUT_CURRENT_LIMIT_MIN_MA  100
#define INPUT_CURRENT_LIMIT_MAX_MA  3000
static int bq24192_set_input_i_limit(struct bq24192_chip *chip, int ma)
{
	int i;
	u8 temp;

	if (ma < INPUT_CURRENT_LIMIT_MIN_MA
			|| ma > INPUT_CURRENT_LIMIT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}
	chip->input_limit_ma = ma;

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].icl_ma <= ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", ma);
		i = 0;
	}
	chip->input_limit_ma = icl_ma_table[i].icl_ma;

	temp = icl_ma_table[i].value;

	if (ma > chip->max_input_i_ma) {
		chip->saved_input_i_ma = ma;
		pr_warn("reject %d mA due to therm mitigation\n", ma);
		return 0;
	}

	if (!chip->therm_mitigation)
		chip->saved_input_i_ma = ma;

	chip->therm_mitigation = false;
	pr_warn("input current limit = %d setting 0x%02x\n", ma, temp);
	return bq24192_masked_write(chip->client, INPUT_SRC_CONT_REG,
			INPUT_CURRENT_LIMIT_MASK, temp);
}

static int mitigate_tbl[] = {3000, 900, 500, 100};
static void bq24192_therm_mitigation_work(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work,
				struct bq24192_chip, therm_work.work);
	int ret;
	int input_limit_ma;

	chip->max_input_i_ma = mitigate_tbl[input_limit_idx];
	if (chip->max_input_i_ma < chip->saved_input_i_ma) {
		input_limit_ma = chip->max_input_i_ma;
		chip->therm_mitigation = true;
	} else {
		input_limit_ma = chip->saved_input_i_ma;
		chip->therm_mitigation = false;
	}
	pr_warn("max_input_i_ma=%d,saved_input_i_ma=%d\n",
			chip->max_input_i_ma,chip->saved_input_i_ma);
	ret = bq24192_set_input_i_limit(chip, input_limit_ma);
	if (ret)
		pr_err("failed to set input current limit as %d\n",
					input_limit_ma);
}

static int bq24192_therm_set_input_i_limit(const char *val,
					const struct kernel_param *kp)
{
	int ret;

	if (!the_chip)
		return -ENODEV;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("failed to set input_limit_idx\n");
		return ret;
	}

	if (input_limit_idx >= ARRAY_SIZE(mitigate_tbl))
		input_limit_idx = ARRAY_SIZE(mitigate_tbl) - 1;

	if (!power_supply_is_system_supplied())
		return 0;

	schedule_delayed_work(&the_chip->therm_work,
			msecs_to_jiffies(2000));

	return 0;
}

static struct kernel_param_ops therm_input_limit_ops = {
	.set = bq24192_therm_set_input_i_limit,
	.get = param_get_int,
};
module_param_cb(input_current_idx, &therm_input_limit_ops,
				&input_limit_idx, 0644);

#define IBAT_MAX_MA  4532
#define IBAT_MIN_MA  512
#define IBAT_STEP_MA  64
static int bq24192_set_ibat_max(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ibat = 0;

	if (ma < IBAT_MIN_MA || ma > IBAT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}

	if ((chip->batt_health == POWER_SUPPLY_HEALTH_OVERHEAT)
			&& (ma > chip->set_chg_current_ma)) {
		chip->saved_ibat_ma = ma;
		pr_warn("reject %d mA setting due to overheat\n", ma);
		return 0;
	}

	reg_val = (ma - IBAT_MIN_MA)/IBAT_STEP_MA;
	set_ibat = reg_val * IBAT_STEP_MA + IBAT_MIN_MA;
	reg_val = reg_val << 2;
	chip->set_chg_current_ma = set_ibat;
	pr_warn("req_ibat = %d set_ibat = %d reg_val = 0x%02x\n",
				ma, set_ibat, reg_val);

	return bq24192_masked_write(chip->client, CHARGE_CUR_CONT_REG,
			CHG_CURRENT_LIMIT_MASK, reg_val);
}

static int bq24192_check_restore_ibatt(struct bq24192_chip *chip,
				int old_health, int new_health)
{
	if (old_health == new_health)
		return 0;

	switch (new_health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
		chip->saved_ibat_ma = chip->set_chg_current_ma;
		break;
	case POWER_SUPPLY_HEALTH_GOOD:
		chip->batt_health = new_health;
		if (chip->saved_ibat_ma) {
			bq24192_set_ibat_max(chip,
					chip->saved_ibat_ma);
			pr_warn("restore ibat max = %d by decreasing temp",
						chip->saved_ibat_ma);
		}
		chip->saved_ibat_ma = 0;
		break;
	case POWER_SUPPLY_HEALTH_COLD:
	case POWER_SUPPLY_HEALTH_UNKNOWN:
	default:
		break;
	}

	return 0;
}

#define VIN_LIMIT_MIN_MV  3880
#define VIN_LIMIT_MAX_MV  5080
#define VIN_LIMIT_STEP_MV  80
static int bq24192_set_input_vin_limit(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vin = 0;

	if (mv < VIN_LIMIT_MIN_MV || mv > VIN_LIMIT_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VIN_LIMIT_MIN_MV)/VIN_LIMIT_STEP_MV;
	set_vin = reg_val * VIN_LIMIT_STEP_MV + VIN_LIMIT_MIN_MV;
	reg_val = reg_val << 3;

	pr_debug("req_vin = %d set_vin = %d reg_val = 0x%02x\n",
				mv, set_vin, reg_val);

	return bq24192_masked_write(chip->client, INPUT_SRC_CONT_REG,
			INPUT_VOLTAGE_LIMIT_MASK, reg_val);
}

#define VBAT_MAX_MV  4400
#define VBAT_MIN_MV  3504
#define VBAT_STEP_MV  16
static int bq24192_set_vbat_max(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vbat = 0;

	if (mv < VBAT_MIN_MV || mv > VBAT_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VBAT_MIN_MV)/VBAT_STEP_MV;
	set_vbat = reg_val * VBAT_STEP_MV + VBAT_MIN_MV;
	reg_val = reg_val << 2;
	pr_debug("req_vbat = %d set_vbat = %d reg_val = 0x%02x\n",
				mv, set_vbat, reg_val);

	return bq24192_masked_write(chip->client, CHARGE_VOLT_CONT_REG,
			CHG_VOLTAGE_LIMIT_MASK, reg_val);
}

#define SYSTEM_VMIN_LOW_MV  3000
#define SYSTEM_VMIN_HIGH_MV  3700
#define SYSTEM_VMIN_STEP_MV  100
static int bq24192_set_system_vmin(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vmin = 0;

	if (mv < SYSTEM_VMIN_LOW_MV || mv > SYSTEM_VMIN_HIGH_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - SYSTEM_VMIN_LOW_MV)/SYSTEM_VMIN_STEP_MV;
	set_vmin = reg_val * SYSTEM_VMIN_STEP_MV + SYSTEM_VMIN_LOW_MV;
	reg_val = reg_val << 1;

	pr_debug("req_vmin = %d set_vmin = %d reg_val = 0x%02x\n",
				mv, set_vmin, reg_val);

	return bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
			SYSTEM_MIN_VOLTAGE_MASK, reg_val);
}

#define IPRECHG_MIN_MA  128
#define IPRECHG_MAX_MA  2048
#define IPRECHG_STEP_MA  128
static int bq24192_set_prechg_i_limit(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < IPRECHG_MIN_MA || ma > IPRECHG_MAX_MA) {
		pr_err("bad ma=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IPRECHG_MIN_MA)/IPRECHG_STEP_MA;
	set_ma = reg_val * IPRECHG_STEP_MA + IPRECHG_MIN_MA;
	reg_val = reg_val << 4;

	pr_debug("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, PRE_CHARGE_TERM_CUR_REG,
			PRECHG_CURRENT_MASK, reg_val);
}

#define ITERM_MIN_MA  128
#define ITERM_MAX_MA  2048
#define ITERM_STEP_MA  128
static int bq24192_set_term_current(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < ITERM_MIN_MA || ma > ITERM_MAX_MA) {
		pr_err("bad mv=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - ITERM_MIN_MA)/ITERM_STEP_MA;
	set_ma = reg_val * ITERM_STEP_MA + ITERM_MIN_MA;

	pr_debug("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, PRE_CHARGE_TERM_CUR_REG,
			TERM_CURRENT_MASK, reg_val);
}

#define IRCOMP_R_MIN_MOHM  0
#define IRCOMP_R_MAX_MOHM  70
#define IRCOMP_R_STEP_MOHM  10
static int bq24192_set_ir_comp_resister(struct bq24192_chip *chip, int mohm)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (mohm < IRCOMP_R_MIN_MOHM
			|| mohm > IRCOMP_R_MAX_MOHM) {
		pr_err("bad r=%d asked to set\n", mohm);
		return -EINVAL;
	}

	reg_val = (mohm - IRCOMP_R_MIN_MOHM)/IRCOMP_R_STEP_MOHM;
	set_ma = reg_val * IRCOMP_R_STEP_MOHM + IRCOMP_R_MIN_MOHM;
	reg_val = reg_val << 5;

	pr_debug("req_r = %d set_r = %d reg_val = 0x%02x\n",
				mohm, set_ma, reg_val);

	return bq24192_masked_write(chip->client, IR_COMP_THERM_CONT_REG,
			IR_COMP_R_MASK, reg_val);
}

#define IRCOMP_VCLAMP_MIN_MV  0
#define IRCOMP_VCLAMP_MAX_MV  112
#define IRCOMP_VCLAMP_STEP_MV  16
static int bq24192_set_vclamp_mv(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (mv < IRCOMP_VCLAMP_MIN_MV
			|| mv > IRCOMP_VCLAMP_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - IRCOMP_VCLAMP_MIN_MV)/IRCOMP_VCLAMP_STEP_MV;
	set_ma = reg_val * IRCOMP_VCLAMP_STEP_MV + IRCOMP_VCLAMP_MIN_MV;
	reg_val = reg_val << 2;

	pr_debug("req_mv = %d set_mv = %d reg_val = 0x%02x\n",
				mv, set_ma, reg_val);

	return bq24192_masked_write(chip->client, IR_COMP_THERM_CONT_REG,
			IR_COMP_VCLAMP_MASK, reg_val);
}
static int bq24192_get_vbat_voltage(struct bq24192_chip *chip)
{
	/*int rc = 0;
	struct qpnp_vadc_result result;
	return 0;
	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &result);
	if (rc) {
		pr_err("Unable to read vbat rc=%d\n", rc);
		return 0;
	}
	pr_debug("vbat=%lld\n",result.physical);
	return div_s64(result.physical,1000);*/
	return 0;
}
#define DEFAULT_RSENSE_MOHM 50
static int bq24192_get_emulate_ocv(struct bq24192_chip *chip)
{
	int vbat_mv,chg_ma,ocv_mv,rsense_mohm;

	rsense_mohm = DEFAULT_RSENSE_MOHM;
	if(chip->rsense_mohm) {
		rsense_mohm = chip->rsense_mohm;
	}

	vbat_mv = bq24192_get_prop_batt_voltage_now(chip);
	vbat_mv /= 1000;
	chg_ma = bq24192_get_prop_batt_current_now(chip);
	chg_ma /= 1000;
	ocv_mv = vbat_mv + rsense_mohm * chg_ma/1000;
	pr_debug("ocv=%dmv,vbat=%dmv,chg=%dma,rsense=%dmohm\n",ocv_mv,vbat_mv,chg_ma,rsense_mohm);
	return ocv_mv;
}

#if defined(CONFIG_BQ24192_SECONDARY_CHARGER)
static void bq24192_set_secondary_chg_cur(struct bq24192_chip *chip,
												int input_current_ma,int chg_current_ma)
{
	union power_supply_propval input_cur = {0,};
	union power_supply_propval chg_cur = {0,};

	input_cur.intval = input_current_ma * 1000;
	chg_cur.intval = chg_current_ma * 1000;
	if (!chip->secondary_ac_psy && chip->sec_ac_psy_name)
		chip->secondary_ac_psy =
			power_supply_get_by_name((char *)chip->sec_ac_psy_name);

	if (chip->secondary_ac_psy) {
		pr_debug("set secondary chg input=%dma,chg=%dma\n",input_current_ma,chg_current_ma);
		power_supply_set_property(chip->secondary_ac_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &input_cur);
		power_supply_set_property(chip->secondary_ac_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &chg_cur);
	}
	return;

}
static void bq24192_enable_secondary_chg(struct bq24192_chip *chip,bool enable)
{
	union power_supply_propval enable_psy = {0,};

	enable_psy.intval = enable;
	if (!chip->secondary_ac_psy && chip->sec_ac_psy_name)
		chip->secondary_ac_psy =
			power_supply_get_by_name((char *)chip->sec_ac_psy_name);

	if (chip->secondary_ac_psy) {
		pr_debug("enable=%d\n",enable);
		power_supply_set_property(chip->secondary_ac_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &enable_psy);
	}
}
static int bq24192_get_secondary_chg_status(struct bq24192_chip *chip)
{
	union power_supply_propval status_psy = {0,};

	if (!chip->secondary_ac_psy && chip->sec_ac_psy_name)
		chip->secondary_ac_psy =
			power_supply_get_by_name((char *)chip->sec_ac_psy_name);

	if (chip->secondary_ac_psy) {
		power_supply_get_property(chip->secondary_ac_psy,
				POWER_SUPPLY_PROP_STATUS, &status_psy);
	}
	return status_psy.intval;
}

#else
static void bq24192_set_secondary_chg_cur(struct bq24192_chip *chip,
												int input_current_ma,int chg_current_ma)
{
	pr_debug("secondary charger not supported\n");
	return;
}
static void bq24192_enable_secondary_chg(struct bq24192_chip *chip,bool enable)
{
	pr_debug("secondary charger not supported\n");
	return;
}
static int bq24192_get_secondary_chg_status(struct bq24192_chip *chip)
{
	pr_debug("secondary charger not supported\n");
	return 0;
}
#endif

static void bq24192_dump_register(struct bq24192_chip *chip)
{
	int rc;
	u8 temp = 0,reg = 0;
	char printline[512];
	char buf[32];
	uint8_t reg_fault = 0, reg_status = 0;
#if 0
	reg = SYSTEM_STATUS_REG;
	rc = bq24192_read_reg(chip->client, reg, &temp);
	pr_debug("reg=0x%02x,value=0x%02x\n",reg,temp);

	reg = INPUT_SRC_CONT_REG;
	rc = bq24192_read_reg(chip->client, reg, &temp);
	pr_debug("reg=0x%02x,value=0x%02x\n",reg,temp);

	reg = CHARGE_CUR_CONT_REG;
	rc = bq24192_read_reg(chip->client, reg, &temp);
	pr_debug("reg=0x%02x,value=0x%02x\n",reg,temp);

	reg = CHARGE_TERM_TIMER_CONT_REG;
	rc = bq24192_read_reg(chip->client, reg, &temp);
	pr_debug("reg=0x%02x,value=0x%02x\n",reg,temp);

	reg = FAULT_REG;
	rc = bq24192_read_reg(chip->client, reg, &temp);
	pr_debug("reg=0x%02x,value=0x%02x\n",reg,temp);
#endif
	memset(printline, 0, sizeof(printline));
	memset(buf, 0, sizeof(buf));
	for (reg = 0;reg <= 0x0A;reg ++) {
		rc = bq24192_read_reg(chip->client, reg, &temp);
		snprintf(buf, 32, "reg=0x%02X,value=0x%02X, ",reg,temp);
		strcat(printline, buf);
		memset(buf, 0, sizeof(buf));
		if (reg == SYSTEM_STATUS_REG) {
			reg_status = temp;
		} else if (reg == FAULT_REG) {
			reg_fault = temp;
		}
	}
	printline[512-1] = '\0';
	pr_err("reg: %s\n", printline);
	//fuelsummary_collect_value(ID_SIC_FAULT, reg_fault);
	//fuelsummary_collect_value(ID_SIC_NPG, ((reg_status & 0x04) != 0x04));
	//fuelsummary_collect_value(ID_SIC_DPM, ((reg_status & 0x08) == 0x08));
}
static int bq24192_config_usbsel_status(struct bq24192_chip *chip,bool reset);

static void bq24192_irq_worker(struct work_struct *work)
{
	struct bq24192_chip *chip =
		container_of(work, struct bq24192_chip, irq_work);
	union power_supply_propval ret = {0,};
	bool ext_pwr;
	bool wlc_pwr = 0;
	bool chg_done = false,chg_fault = false;
	u8 temp;
	int rc;
	unsigned long flags;


	wake_lock(&chip->irq_wake_lock);

	msleep(100 * chip->irq_scheduled_time_status);

	rc = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	/* Open up for next possible interrupt handler beyond read reg
	 * asap, lest we miss an interrupt
	 */
	spin_lock_irqsave(&chip->irq_work_lock, flags);
	chip->irq_scheduled_time_status = 0;
	spin_unlock_irqrestore(&chip->irq_work_lock, flags);

	//bq24192_dump_register(chip);
	ext_pwr = !!(temp & PG_STAT_MASK);
	if (ext_pwr) {
		bq24192_enable_hiz(chip, false);
	} else {
		bq24192_enable_hiz(chip, true);
	}

	if (rc) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", rc);
		goto irq_worker_exit;
	}
	ext_pwr = !!(temp & PG_STAT_MASK);
	chg_done = (temp & CHARGING_MASK) == 0x30 ? true : false;
	pr_err("chg present=%d,last present=%d,chg done=%d\n",ext_pwr,chip->ext_pwr,chg_done);

	if (!chip->primary_charger)
		goto irq_worker_exit;

	if (chg_done) {
		if (chip->batt_health != POWER_SUPPLY_HEALTH_OVERHEAT &&
				bq24192_get_soc_from_batt_psy(chip) < 100) {
			pr_warn("recharge!!\n");
			//bq24192_trigger_recharge(chip);
		} else {
			power_supply_changed(chip->batt_psy);
			pr_warn("charge done!!\n");
		}
	}
	rc = bq24192_read_reg(chip->client, FAULT_REG, &temp);
	chg_fault = (temp & CHRG_FAULT_MAST) == 0x10 ? true:false;
	if (chg_fault) {
		pr_debug("chg fault,qc2p0_retries=%d\n",chip->qc2p0_retries);
		chip->qc2p0_retries++;
	}
	if (chip->wlc_psy) {
		power_supply_get_property(chip->wlc_psy,
				POWER_SUPPLY_PROP_PRESENT, &ret);
		wlc_pwr = ret.intval;
	}

	if ((chip->ext_pwr ^ ext_pwr) || (chip->wlc_pwr ^ wlc_pwr)) {
		pr_warn("power source changed! ext_pwr = %d wlc_pwr = %d\n",
				ext_pwr, wlc_pwr);
		if (wake_lock_active(&chip->icl_wake_lock))
			wake_unlock(&chip->icl_wake_lock);

		if (!wlc_pwr && chip->primary_charger) {
			pr_warn("notify vbus to usb otg ext_pwr = %d\n", ext_pwr);
			ret.intval = ext_pwr;
			power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_PRESENT, &ret);
			power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_ONLINE, &ret);
		}

		cancel_delayed_work_sync(&chip->input_limit_work);
		cancel_delayed_work_sync(&chip->therm_work);
		bq24192_vbat_detect_disable(chip);
		chip->saved_ibat_ma = 0;
		chip->set_chg_current_ma = chip->chg_current_ma;
		chip->max_input_i_ma = INPUT_CURRENT_LIMIT_MAX_MA;

		if (chip->wlc_psy) {
			if (wlc_pwr && ext_pwr) {
				chip->wlc_pwr = true;
				ret.intval = true;
				power_supply_set_property(chip->wlc_psy, POWER_SUPPLY_PROP_ONLINE, &ret);
			} else if (chip->wlc_pwr && !(ext_pwr && wlc_pwr)) {
				chip->wlc_pwr = false;
				ret.intval = false;
				power_supply_set_property(chip->wlc_psy, POWER_SUPPLY_PROP_ONLINE, &ret);
			}
		}

		if (!ext_pwr) {
			chip->usb_psy_type = 0;
			bq24192_qc2p0_detect_deinit(chip);
			clear_bit(BQ24192_CHG_TIMEOUT_STATUS, &chip->health_status);
		} else {
			bq24192_qc2p0_detect_init(chip);
		}
		chip->ext_pwr = ext_pwr;
	}
	bq24192_dump_register(chip);

irq_worker_exit:
	wake_lock_timeout(&chip->irq_wake_lock, 2*HZ);
}
static int bq24192_vbat_detect_disable(struct bq24192_chip *chip)
{
	cancel_delayed_work_sync(&chip->vbat_work);
	if (wake_lock_active(&chip->chg_wake_lock)) {
		pr_debug("releasing wakelock\n");
		wake_unlock(&chip->chg_wake_lock);
	}
	return 0;
}

static int bq24192_reset_watchdog_and_kick(struct bq24192_chip *chip)
{
	int ret;

	pr_err("reset watch dog and enable watch dog!\n");
	bq24192_kick_watchdog(chip);
	ret = bq24192_masked_write(chip->client, CHARGE_TERM_TIMER_CONT_REG,
				WATCHDOG_TIMER_MAST, 0x20);
	if (ret) {
		pr_err("failed to set wdog to 80s\n");
		return ret;
	}
	return ret;
}

static int bq24192_vbat_detect_init(struct bq24192_chip *chip)
{
	cancel_delayed_work_sync(&chip->vbat_work);
	bq24192_reset_watchdog_and_kick(chip);
	if (!wake_lock_active(&chip->chg_wake_lock)) {
		pr_err("lock wakelock\n");
		wake_lock(&chip->chg_wake_lock);
	}
	schedule_delayed_work(&chip->vbat_work, msecs_to_jiffies(5000));
	return 0;
}

static irqreturn_t bq24192_irq(int irq, void *dev_id)
{
	struct bq24192_chip *chip = dev_id;
	unsigned long flags;
	spin_lock_irqsave(&chip->irq_work_lock, flags);
	if (chip->irq_scheduled_time_status == 0) {
		schedule_work(&chip->irq_work);
		chip->irq_scheduled_time_status = 1;
	}
	spin_unlock_irqrestore(&chip->irq_work_lock, flags);

	return IRQ_HANDLED;
}
#if 1
static int set_reg(void *data, u64 val)
{
#if 1
	u32 addr = *(u32 *) data;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_write_reg(client, addr, (u8) val);

	return ret;
#endif
	return 0;
}

static int get_reg(void *data, u64 *val)
{
#if 1
	u32 addr = *(u32 *) data;
	u8 temp;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_read_reg(client, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;
#endif

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");
#endif

#define OTG_ENABLE_SHIFT  5
static int bq24192_enable_otg(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << OTG_ENABLE_SHIFT);

	pr_warn("otg enable = %d\n", enable);

	ret = bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
					OTG_EN_MASK, val);
	if (ret) {
		pr_err("failed to set OTG_EN rc=%d\n", ret);
		return ret;
	}

	if (chip->otg_en_gpio)
		gpio_set_value(chip->otg_en_gpio, enable);

	return 0;
}

static bool bq24192_is_otg_mode(struct bq24192_chip *chip)
{
	u8 temp;
	int ret;

	ret = bq24192_read_reg(chip->client, PWR_ON_CONF_REG, &temp);
	if (ret) {
		pr_err("failed to read OTG enable bits =%d\n", ret);
		return false;
	}

	return !!(temp & OTG_EN_MASK);
}

static bool bq24192_is_chg_done(struct bq24192_chip *chip)
{
	int ret;
	u8 temp;

	ret = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (ret) {
		pr_err("i2c read fail\n");
		return false;
	}

	return (temp & CHG_DONE_MASK) == CHG_DONE_MASK;
}

static void bq24192_trigger_recharge(struct bq24192_chip *chip)
{

	if (chip->batt_health != POWER_SUPPLY_HEALTH_GOOD)
		return;

	if (!bq24192_is_chg_done(chip))
		return;

	bq24192_enable_hiz(chip, true);
	bq24192_enable_hiz(chip, false);
}

#define WLC_BOUNCE_INTERVAL_MS 15000
#define WLC_BOUNCE_COUNT 3
static bool bq24192_is_wlc_bounced(struct bq24192_chip *chip)
{
	ktime_t now_time;
	uint32_t interval_ms;
	static ktime_t prev_time;
	static int bounced_cnt = 0;

	now_time = ktime_get();

	interval_ms = (uint32_t)ktime_to_ms(ktime_sub(now_time, prev_time));
	if (interval_ms < WLC_BOUNCE_INTERVAL_MS)
		bounced_cnt ++;
	else
		bounced_cnt = 0;

	prev_time = now_time;
	if (bounced_cnt >= WLC_BOUNCE_COUNT) {
		pr_warn("detect wlc bouncing!\n");
		bounced_cnt = 0;
		return true;
	}

	return false;
}

#define WLC_INPUT_I_LIMIT_MA 900
#define USB_MAX_IBAT_MA 1500
static void bq24192_external_battery_changed(struct power_supply *psy)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);
	union power_supply_propval ret = {0,};
	union power_supply_propval usb_type = {0,};
	int wlc_online = 0;
	int wlc_chg_current_ma = 0;

	if(!chip->usb_psy){
		chip->usb_psy = power_supply_get_by_name("usb");
		if(!chip->usb_psy){
    		pr_err("usb psy not found!\n");
    		return;
		}
	}

	power_supply_get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_ONLINE, &ret);
	chip->usb_online = ret.intval;

	power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &usb_type);

	if (chip->wlc_support) {
		power_supply_get_property(chip->wlc_psy,
				  POWER_SUPPLY_PROP_ONLINE, &ret);
		wlc_online = ret.intval;

		power_supply_get_property(chip->wlc_psy,
				  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		wlc_chg_current_ma = ret.intval / 1000;
	}
	pr_warn("usb_online=%d,last_type=%d,type=%d\n",
			chip->usb_online,chip->usb_psy_type,usb_type.intval);
	//bq24192_enable_charging(chip, false);
	if (chip->usb_online &&	bq24192_is_charger_present(chip)) {

		if(chip->usb_psy_type != usb_type.intval){

			if (usb_type.intval == POWER_SUPPLY_TYPE_USB) {
				power_supply_get_property(chip->usb_psy,
						  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
				bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
				ret.intval = 500 * 1000;
				bq24192_set_input_i_limit(chip, ret.intval / 1000);
				bq24192_set_ibat_max(chip, USB_MAX_IBAT_MA);
				//gpio_direction_output(chip->en_gpio,0);
				bq24192_vbat_detect_init(chip);
				pr_warn("sdp is online! i_limit = %d v_limit = %d\n",
						ret.intval / 1000, chip->vin_limit_mv);

			} else if (usb_type.intval == POWER_SUPPLY_TYPE_USB_DCP){
				//bq24192_enable_hiz(chip, false);
				chip->icl_first = true;
				chip->icl_idx = 0;
				//bq24192_enable_charging(chip,0);
				//bq24192_set_input_vin_limit(chip,
						//chip->icl_vbus_mv - 2 * VIN_LIMIT_STEP_MV);
				bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
				bq24192_set_input_i_limit(chip, adap_tbl[2].input_limit);
				bq24192_set_ibat_max(chip, adap_tbl[2].chg_limit);
				pr_warn("ac is online! i_limit = %d v_limit = %d,icl_vbus_mv=%d\n",
						adap_tbl[0].chg_limit, chip->vin_limit_mv,chip->icl_vbus_mv);

			}
			pr_warn("type changed start or stop charge\n");
			chip->usb_psy_type = usb_type.intval;

		}


	} else if (wlc_online) {
		chip->dwn_chg_i_ma = chip->wlc_dwn_i_ma;
		chip->up_chg_i_ma = wlc_chg_current_ma;
		chip->dwn_input_i_ma = chip->wlc_dwn_input_i_ma;
		if (bq24192_is_wlc_bounced(chip))
			chip->up_input_i_ma = chip->wlc_dwn_input_i_ma;
		else
			chip->up_input_i_ma = WLC_INPUT_I_LIMIT_MA;
		bq24192_set_input_vin_limit(chip, chip->wlc_vin_limit_mv);
		bq24192_set_input_i_limit(chip, chip->up_input_i_ma);
		bq24192_set_ibat_max(chip, wlc_chg_current_ma);
		bq24192_vbat_detect_init(chip);
		pr_warn("wlc is online! i_limit = %d v_limit = %d\n",
				wlc_chg_current_ma, chip->wlc_vin_limit_mv);
	}

	//if (bq24192_is_charger_present(chip))
		//schedule_delayed_work(&chip->therm_work,
				//msecs_to_jiffies(2000));

	power_supply_get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_SCOPE, &ret);

	if (ret.intval) {
		pr_warn("usb host mode = %d\n", ret.intval);
		if ((ret.intval == POWER_SUPPLY_SCOPE_SYSTEM)
					&& !bq24192_is_otg_mode(chip))
			bq24192_enable_otg(chip, true);
		else if ((ret.intval == POWER_SUPPLY_SCOPE_DEVICE)
					&& bq24192_is_otg_mode(chip))
			bq24192_enable_otg(chip, false);
	}

	power_supply_changed(chip->batt_psy);
}

#define FAIL_DEFAULT_SOC 50
static int bq24192_get_soc_from_batt_psy(struct bq24192_chip *chip)
{
#if 1
	union power_supply_propval ret = {0,};
	int soc = 0;
	struct power_supply  *batt_psy = NULL;

	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");

	if (batt_psy) {
		power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		soc = ret.intval;
	} else {
		pr_warn("battery power supply is not registered yet\n");
		soc = FAIL_DEFAULT_SOC;
	}

	return soc;
#endif
    return FAIL_DEFAULT_SOC;
}

static int bq24192_get_prop_charge_type(struct bq24192_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 temp;

	ret = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (ret) {
		pr_err("i2c read fail\n");
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	temp = temp & CHARGING_MASK;

	if (temp == FAST_CHARGE_MASK)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (temp == PRE_CHARGE_MASK)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	if (chip->chg_type != chg_type) {
		if (chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE) {
			pr_warn("Charging stopped.\n");
		} else {
			pr_warn("Charging started.\n");
		    chip->charge_begin = jiffies;
		}
	}
	chip->chg_type = chg_type;
	return chg_type;
}

static int bq24192_get_prop_chg_status(struct bq24192_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	int soc = 0;

	soc = bq24192_get_soc_from_batt_psy(chip);
	chg_type = bq24192_get_prop_charge_type(chip);

	switch (chg_type) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		if (bq24192_is_charger_present(chip)) {
			if (soc >= 100)
				chg_status = POWER_SUPPLY_STATUS_FULL;
			else
				chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE:
	case POWER_SUPPLY_CHARGE_TYPE_FAST:
		chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		break;
	}

	pr_debug("chg status = %d soc = %d\n", chg_status, soc);
	return chg_status;
}

#define UNINIT_VBUS_UV 5000000
static int bq24192_get_prop_input_voltage(struct bq24192_chip *chip)
{
	return 0;
#if 0
	int rc = 0;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(USBIN, &results);
	if (rc) {
		pr_err("Unable to read vbus rc=%d\n", rc);
		return UNINIT_VBUS_UV;
	}

	return (int)results.physical;
#endif
}

static void bq24192_input_limit_worker(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work, struct bq24192_chip,
						input_limit_work.work);
	int vbus_mv,chg_ma,rc;
	int en_value = 0;
	int hvdcp_type;

	if (!chip->qc2p0_detect_support) {
		return;
	}

	bq24192_update_bms(chip);
	rc = bq24192_get_vbus_voltage(chip,&vbus_mv);
	chg_ma = bq24192_get_prop_batt_current_now(chip);
	chg_ma /= 1000;
	en_value = gpio_get_value(chip->en_gpio);
	if (en_value) {
		pr_debug("set en_gpio to 0\n");
		gpio_direction_output(chip->en_gpio,0);
	}
	//goto restart;
	hvdcp_type = bq24192_get_hvdcp_type(chip,vbus_mv);
	pr_debug("vbus_mv=%d,chg_ma=%d,en_value = %d,type=%dv,hvdcp type=%dv\n",
			vbus_mv,chg_ma,en_value,hvdcp_type,chip->hvdcp_type);

	if (chip->icl_first && chip->icl_idx > 0) {
		pr_debug("icl fail cnt=%d\n",chip->icl_fail_cnt);
		chip->icl_fail_cnt++;
		if (chip->icl_fail_cnt > 1)
			vbus_mv = chip->icl_vbus_mv;
		else
			chip->icl_idx = 0;
	}
	chip->icl_first = false;

	if (hvdcp_type == chip->hvdcp_type
			&& chip->icl_idx < (ARRAY_SIZE(adap_tbl) - 1)) {
		chip->icl_idx++;
		bq24192_config_charger_based_updated_voltage(chip,
				adap_tbl[chip->icl_idx].input_limit,
				adap_tbl[chip->icl_idx].chg_limit);

		schedule_delayed_work(&chip->input_limit_work,
					msecs_to_jiffies(2000));
	} else {
		if (chip->icl_idx > 0 && hvdcp_type != chip->hvdcp_type)
			chip->icl_idx--;

		bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
		bq24192_config_charger_based_updated_voltage(chip,
				adap_tbl[chip->icl_idx].input_limit,
				adap_tbl[chip->icl_idx].chg_limit);

		if (adap_tbl[chip->icl_idx].chg_limit
				> chip->step_dwn_currnet_ma) {
			chip->dwn_chg_i_ma = chip->step_dwn_currnet_ma;
			chip->up_chg_i_ma = adap_tbl[chip->icl_idx].chg_limit;
			chip->dwn_input_i_ma = adap_tbl[chip->icl_idx].input_limit;
			chip->up_input_i_ma = adap_tbl[chip->icl_idx].input_limit;
			bq24192_vbat_detect_init(chip);
		}

		pr_warn("optimal input i limit = %d chg limit = %d,chg_ma=%d\n",
					adap_tbl[chip->icl_idx].input_limit,
					adap_tbl[chip->icl_idx].chg_limit,chg_ma);
		chip->icl_idx = 0;
		chip->icl_fail_cnt = 0;
		wake_unlock(&chip->icl_wake_lock);
	}

}
static void bq24192_is_chg_timeout(struct bq24192_chip *chip)
{
	unsigned long timeout;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	return;

	chg_type = bq24192_get_prop_charge_type(chip);

	if (chip->charge_begin > 0) {
		timeout= chip->charge_begin + chip->chg_tmout_mins * 60 * HZ;
		if(chg_type > POWER_SUPPLY_CHARGE_TYPE_NONE &&
			time_after(jiffies, timeout)){
			#if defined(PD1227LG4) || defined(PD1402LG4)|| defined(PD1408LG4)
			pr_warn("ignore time out for ccmc\n");//ignore for ccmc
			#else
			set_bit(BQ24192_CHG_TIMEOUT_STATUS, &chip->health_status);
			#endif
		}
		if(chg_type > POWER_SUPPLY_CHARGE_TYPE_NONE){
			pr_warn("charging runs %d seconds,status=%ld,chg_tmout_mins=%d\n",
				jiffies_to_msecs(jiffies-chip->charge_begin)/ 1000,
				chip->health_status,chip->chg_tmout_mins);
		}
	}

}
#define HVDCP_TYPE_5V_OVP_MV 	6400
#define HVDCP_TYPE_9V_OVP_MV 	10400
#define HVDCP_TYPE_12V_OVP_MV 	13200
static int bq24192_is_chg_ov(struct bq24192_chip *chip)
{
	int vbus_mv;
	bool is_ov = false;
	return 0;
	bq24192_get_vbus_voltage(chip,&vbus_mv);

	switch (chip->hvdcp_type) {
	case HVDCP_TYPE_5V:
		if (vbus_mv > HVDCP_TYPE_5V_OVP_MV) {
			is_ov = true;
		} else {
			is_ov = false;
		}
		break;
	case HVDCP_TYPE_9V:
		if (vbus_mv > HVDCP_TYPE_9V_OVP_MV) {
			is_ov = true;
		} else {
			is_ov = false;
		}
		break;
	case HVDCP_TYPE_12V:
		if (vbus_mv > HVDCP_TYPE_12V_OVP_MV) {
			is_ov = true;
		} else {
			is_ov = false;
		}
		break;
	default:
		break;
	}

	if (is_ov) {
		set_bit(BQ24192_CHG_OV_STATUS, &chip->health_status);
	} else {
		clear_bit(BQ24192_CHG_OV_STATUS, &chip->health_status);
	}

	pr_warn("is_ov=%d,hvdcp_type=%d\n",is_ov,chip->hvdcp_type);
	return 0;
}

static int
bq24192_get_prop_chg_health_status(struct bq24192_chip *chip)
{
	bq24192_is_chg_timeout(chip);
	bq24192_is_chg_ov(chip);
	return chip->health_status;
}
//extern void charger_connect_judge(char on_or_off);
static int bq24192_parallel_set_chg_present(struct bq24192_chip *chip,
						int present)
{
	pr_debug("present=%d\n",present);
	if (present == chip->parallel_charger_present) {
		pr_debug("present %d -> %d, skipping\n",
				chip->parallel_charger_present, present);
		return 0;
	}
	if (present) {
		//charger_connect_judge(1);
		bq24192_vbat_detect_init(chip);
	} else {
		//charger_connect_judge(0);
		bq24192_disable_watchdog(chip);
		bq24192_enable_charging(chip, false);
		bq24192_enable_hiz(chip, true);
	}
	chip->parallel_charger_present = present;

	return 0;
}
#define FASTCHG_CURRENT_MIN_MA 1024
static int bq24192_fastchg_current_set(struct bq24192_chip *chip,
					unsigned int fastchg_current)
{
	pr_warn("fastchg_current=%d\n",fastchg_current);
	bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
	bq24192_set_ibat_max(chip, fastchg_current);
	chip->set_chg_current_ma = fastchg_current;
	return 0;
}
#define SUSPEND_CURRENT_MA	2
static int bq24192_set_usb_chg_current(struct bq24192_chip *chip,
							int current_ma)
{
	pr_warn("current_ma=%d\n",current_ma);

	/* Only set suspend bit when chg present and current_ma <= 2 */
	if (current_ma <= SUSPEND_CURRENT_MA) {
		bq24192_enable_hiz(chip, false);
		msleep(250);
		bq24192_enable_charging(chip, false);
		pr_debug("bq24192 suspend\n");
		chip->input_limit_ma = 0;
		return 0;
	}
#if 0
	/*IINDPM not triggerd in hvdcp charging*/
	if (current_ma < chip->parallel_iindpm_ma) {
	current_ma = chip->parallel_iindpm_ma;
	}
#endif
	bq24192_enable_hiz(chip, false);
	msleep(250);
	bq24192_set_input_i_limit(chip, current_ma);
	bq24192_enable_charging(chip, true);
	//bq24192_enable_hiz(chip, false);
	//chip->input_limit_ma = current_ma;
	return 0;
}

#define	BQ24192_STATUS_DEFAULT_OK	0x00
#define	BQ24192_STATUS_I2C_ERROR	0x01
static int bq24192_get_prop_batt_slave_charger_status(struct bq24192_chip *chip)
{
	int i = 0;
	int rc;
	u8 temp;

	rc = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
	if (rc) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", rc);
		for(; i < 3; i++){
			msleep(1);
			rc = bq24192_read_reg(chip->client, SYSTEM_STATUS_REG, &temp);
			if(rc){
				pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", rc);
			}else{
				break;
			}
		}
	}
	//pr_err("i=%d\n", i);

	if(i >= 3)
		return BQ24192_STATUS_I2C_ERROR;
	else
		return BQ24192_STATUS_DEFAULT_OK;
}

static enum power_supply_property bq24192_parallel_properties[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
};
static int bq24192_parallel_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		/*
		 *CHG EN is controlled by pin in the parallel charging.
		 *Use suspend if disable charging by command.
		 */
		if (chip->parallel_charger_present) {
			bq24192_enable_charging(chip, val->intval);
			bq24192_enable_hiz(chip, !val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = bq24192_parallel_set_chg_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_charger_present) {
			rc = bq24192_fastchg_current_set(chip,
						val->intval / 1000);
			//fuelsummary_collect_value(ID_SIC_INPUT, val->intval / 1000);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_charger_present) {
			rc = bq24192_set_usb_chg_current(chip,
						val->intval / 1000);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		/*
		if (chip->parallel_charger_present &&
			(chip->vbat_max_mv != val->intval)) {
			rc = bq24192_set_vbat_max(chip, val->intval);
			chip->vbat_max_mv = val->intval;
		} else {
			chip->vbat_max_mv = val->intval;
		}
		*/
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (chip->parallel_charger_present) {
			bq24192_enable_charging(chip, !val->intval);
			bq24192_enable_hiz(chip, val->intval);
		}
		//if (chip->parallel_charger_present && !val->intval)
			//fuelsummary_collect_value(ID_SIC_STAT, 1);
		//else
			//fuelsummary_collect_value(ID_SIC_STAT, 0);
		//break;	
	case POWER_SUPPLY_PROP_WATCH_DOG_STATUS:
		if(!!val->intval){
			bq24192_disable_watchdog(chip);
		}
		break;
	case POWER_SUPPLY_PROP_SLAVE_SUSPEND_STATUS:
		if(!!val->intval){
			bq24192_enable_hiz(chip, true);
		}
		break;
	case POWER_SUPPLY_PROP_INIT_SLAVE_CHARGER:
		if (chip->parallel_charger_present) {
			bq24192_hw_init(chip);
		}
		break;
	case POWER_SUPPLY_PROP_OTG_DISABLE_PL:
		pr_err("otg disable pl =%d\n", val->intval);
		bq24192_enable_charging(chip, !val->intval);
		bq24192_enable_hiz(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		chip->c_charger_temp_max = val->intval;
		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static int bq24192_parallel_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		return 0;
	}
}

static int bq24192_parallel_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq24192_is_chg_enabled(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_charger_present)
			val->intval = chip->input_limit_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->parallel_charger_present;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_charger_present)
			val->intval = chip->set_chg_current_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->parallel_charger_present)
			val->intval = bq24192_get_prop_chg_status(chip);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_CHECK_SLAVE_CHARGER_STATUS:
		val->intval = bq24192_get_prop_batt_slave_charger_status(chip);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN_EXT;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
		val->intval = POWER_SUPPLY_PL_NON_STACKED_BATFET;
		break;
//	case POWER_SUPPLY_PROP_PARALLEL_FCC_MIN:
//		val->intval = 512000;
//		break;
	case POWER_SUPPLY_PROP_DUMP_REG:
		bq24192_dump_register(chip);
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		val->intval = chip->c_charger_temp_max;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static char *bq24192_power_supplied_to[] = {
	//"battery",
};

static enum power_supply_property bq24192_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
};

static int bq24192_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24192_get_prop_chg_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24192_get_prop_chg_health_status(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->set_chg_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24192_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq24192_get_prop_input_voltage(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chip->vbat_max_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq24192_is_chg_enabled(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24192_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);
	unsigned long flags;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		spin_lock_irqsave(&chip->irq_work_lock, flags);
		if (chip->irq_scheduled_time_status == 0) {
			schedule_work(&chip->irq_work);
			/* Set by wireless needs 2 units of 100 ms delay */
			chip->irq_scheduled_time_status = 2;
		}
		spin_unlock_irqrestore(&chip->irq_work_lock, flags);
		return 0;
	case POWER_SUPPLY_PROP_HEALTH:
		bq24192_check_restore_ibatt(chip,
				chip->batt_health, val->intval);
		chip->batt_health = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		chip->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		bq24192_set_ibat_max(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		bq24192_enable_charging(chip, val->intval);
		if (val->intval)
			bq24192_trigger_recharge(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		bq24192_set_vbat_max(chip, val->intval / 1000);
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(chip->batt_psy);
	return 0;
}

static char *bq24192_battery_supplied_to[] = {
	"bms",
#if defined(CONFIG_BQ24192_SECONDARY_CHARGER)
	"secondary_ac",
#endif
};

static enum power_supply_property bq24192_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_FLASH_CURRENT_MAX,

};
static int bq24192_get_prop_batt_present(struct bq24192_chip *chip)
{

	return 1;

}

#define DEFAULT_BATT_CAPACITY	50
static int bq24192_get_prop_batt_capacity(struct bq24192_chip *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;
	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	}

	return DEFAULT_BATT_CAPACITY;
}
#define DEFAULT_BATT_TEMP		200
static int bq24192_get_prop_batt_temp(struct bq24192_chip *chip)
{
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &ret);
		return ret.intval;
	}

	return DEFAULT_BATT_TEMP;
}

static void bq24192_update_bms(struct bq24192_chip *chip)
{
	union power_supply_propval update = {1, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		pr_debug("update bms =%d\n",update.intval);
		power_supply_set_property(chip->bms_psy,
				POWER_SUPPLY_PROP_UPDATE_NOW, &update);
	}
	return;
}

#define DEFAULT_BATT_CURRENT_NOW	0
static int bq24192_get_prop_batt_current_now(struct bq24192_chip *chip)
{
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
		pr_debug("current=%d\n",ret.intval);
		return ret.intval;
	}

	return DEFAULT_BATT_CURRENT_NOW;
}

#define DEFAULT_BATT_VOLTAGE_NOW	0
static int bq24192_get_prop_batt_voltage_now(struct bq24192_chip *chip)
{
	union power_supply_propval ret = {0, };
	bq24192_get_vbat_voltage(chip);
	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
		return ret.intval;
	}

	return DEFAULT_BATT_VOLTAGE_NOW;
}
static int bq24192_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);
	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24192_get_prop_chg_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq24192_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq24192_is_chg_enabled(chip);//chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24192_get_prop_charge_type(chip);//get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq24192_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq24192_get_prop_batt_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq24192_get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq24192_get_prop_batt_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->vbat_max_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_FLASH_CURRENT_MAX:
		val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = 1500;//chip->therm_lvl_sel;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24192_battery_set_property(struct power_supply *psy,
				  enum power_supply_property prop,
				  const union power_supply_propval *val)
{
	struct bq24192_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		//chip->chg_enabled = val->intval;
		bq24192_enable_charging(chip,val->intval);
		gpio_direction_output(chip->en_gpio,0);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(chip->batt_psy);
		break;
	default:
		return -EINVAL;
	}
	return 0;

}
static int bq24192_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int bq24192_create_debugfs_entries(struct bq24192_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir(BQ24192_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("bq24192 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(bq24192_debug_regs) ; i++) {
		char *name = bq24192_debug_regs[i].name;
		u32 *reg = &bq24192_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static const struct power_supply_desc parallel_psy_desc = {
	.name			= "parallel",
	.type			= POWER_SUPPLY_TYPE_PARALLEL,
	.properties		= bq24192_parallel_properties,
	.num_properties		= ARRAY_SIZE(bq24192_parallel_properties),
	.get_property		= bq24192_parallel_get_property,
	.set_property		= bq24192_parallel_set_property,
	.property_is_writeable	= bq24192_parallel_is_writeable,
};

static int bq24192_init_parallel_psy(struct bq24192_chip *chip)
{
	struct power_supply_config parallel_cfg = {};

	parallel_cfg.drv_data = chip;
	parallel_cfg.of_node = chip->dev->of_node;
	chip->parallel_psy = devm_power_supply_register(chip->dev,
						   &parallel_psy_desc,
						   &parallel_cfg);
	if (IS_ERR(chip->parallel_psy)) {
		pr_err("Couldn't register parallel power supply\n");
		return PTR_ERR(chip->parallel_psy);
	}

	return 0;
}

static const struct power_supply_desc ac_psy_desc = {
	.name			= "ac",
	.type			= POWER_SUPPLY_TYPE_MAINS,
	.properties		= bq24192_power_props,
	.num_properties		= ARRAY_SIZE(bq24192_power_props),
	.get_property		= bq24192_power_get_property,
	.set_property		= bq24192_power_set_property,
};

static int bq24192_init_ac_psy(struct bq24192_chip *chip)
{
	struct power_supply_config ac_cfg = {};

	ac_cfg.drv_data = chip;
	ac_cfg.of_node = chip->dev->of_node;
	ac_cfg.supplied_to = bq24192_power_supplied_to;
	ac_cfg.num_supplicants = ARRAY_SIZE(bq24192_power_supplied_to);
	chip->ac_psy = devm_power_supply_register(chip->dev,
						   &ac_psy_desc,
						   &ac_cfg);
	if (IS_ERR(chip->ac_psy)) {
		pr_err("Couldn't register ac power supply\n");
		return PTR_ERR(chip->ac_psy);
	}

	return 0;
}

static int bq24192_hw_init(struct bq24192_chip *chip)
{
	int ret = 0;

	pr_err("bq24192_hw_init\n");

	ret = bq24192_write_reg(chip->client, PWR_ON_CONF_REG,
			RESET_REGISTER_MASK);
	if (ret) {
		pr_err("failed to reset register\n");
		return ret;
	}

	bq24192_kick_watchdog(chip);

	ret = bq24192_set_input_i_limit(chip, 100);
	if (ret)
		pr_err("failed to set input current limit as %d\n",
					100);

	ret = bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
	if (ret) {
		pr_err("failed to set input voltage limit\n");
		return ret;
	}

	ret = bq24192_set_system_vmin(chip, chip->sys_vmin_mv);
	if (ret) {
		pr_err("failed to set system min voltage\n");
		return ret;
	}

	ret = bq24192_set_prechg_i_limit(chip, chip->pre_chg_current_ma);
	if (ret) {
		pr_err("failed to set pre-charge current\n");
		return ret;
	}

	ret = bq24192_set_term_current(chip, chip->term_current_ma);
	if (ret) {
		pr_err("failed to set charge termination current\n");
		return ret;
	}

	ret = bq24192_set_vbat_max(chip, chip->default_vbat_max_mv);
	if (ret) {
		pr_err("failed to set vbat max\n");
		//return ret;
	}

	ret = bq24192_write_reg(chip->client, CHARGE_TERM_TIMER_CONT_REG,
			EN_CHG_TERM_MASK);
	if (ret) {
		pr_err("failed to enable chg termination\n");
		return ret;
	}
#if 1
	ret = bq24192_masked_write(chip->client, CHARGE_TERM_TIMER_CONT_REG,
			WATCHDOG_TIMER_MAST, 0x20);
	if (ret) {
		pr_err("failed to set wdog to 80s\n");
		return ret;
	}
#endif
	ret = bq24192_set_ir_comp_resister(chip, IRCOMP_R_MAX_MOHM);
	if (ret) {
		pr_err("failed to set ir compensation resister\n");
		return ret;
	}

	ret = bq24192_set_vclamp_mv(chip, IRCOMP_VCLAMP_MAX_MV);
	if (ret) {
		pr_err("failed to set ir vclamp voltage\n");
		return ret;
	}

	ret = bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
			BOOST_LIM_MASK, 0);
	if (ret) {
		pr_err("failed to set boost current limit\n");
		return ret;
	}
	bq24192_dump_register(chip);
	//gpio_direction_output(chip->en_gpio,0);
	return 0;
}
static int bq24192_config_usbsel_status(struct bq24192_chip *chip,bool reset)
{
	int ret;
	if(reset) {
		pr_debug("set usbsel to reset staus\n");
		ret = pinctrl_select_state(chip->pinctrl, chip->pin_sleep);
		if (ret) {
			dev_err(chip->dev, "Can't select pinctrl sleep state\n");
			return ret;
		}
		gpio_direction_output(chip->usbsel_gpio,0);
		chip->usb_switched = false;
	} else {
		pr_debug("set usbsel to output status\n");
		ret = pinctrl_select_state(chip->pinctrl, chip->pin_active);
		if (ret) {
			dev_err(chip->dev, "Can't select pinctrl sleep state\n");
			return ret;
		}
		gpio_direction_output(chip->usbsel_gpio,1);
		chip->usb_switched = true;
	}
	return 0;

}
static int bq24192_get_usbvol_voltage(struct bq24192_chip *chip,int *usbvol)
{
	/*int rc;
	struct qpnp_vadc_result result;
	rc = qpnp_vadc_read(chip->vadc_dev, chip->vadc_channel, &result);
	if (rc) {
		pr_err("error reading usbvol channel = %d, rc = %d\n",
					chip->vadc_channel, rc);
		return rc;
	}
	pr_debug("usbvol phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	*usbvol = div_s64(result.physical,1000);
	return rc;*/
	return 0;
}
#define VBUS_VOLTAGE_SCALE 4
#define VBUS_VOLTAGE_TOTAL_ADC 5
static int bq24192_get_vbus_voltage(struct bq24192_chip *chip,int *vbus_mv)
{
	int rc,value = 0;
	int vbus_total_mv = 0,index;

	for (index = 0;index < VBUS_VOLTAGE_TOTAL_ADC;index++) {
		rc = bq24192_get_usbvol_voltage(chip,&value);
		if (rc < 0){
			pr_err("get vbus error=%d\n",value);
			continue;
		}
		pr_debug("vusbvol vol=%dmv\n",value);
		value *= VBUS_VOLTAGE_SCALE;
		vbus_total_mv += value;
		//mdelay(200);
	}
	*vbus_mv = vbus_total_mv/VBUS_VOLTAGE_TOTAL_ADC;
	pr_debug("vbus vol=%dmv\n",*vbus_mv);
	return rc;
}

static int bq24192_dump_qc2p0_pin_status(struct bq24192_chip *chip)
{
	int usbsel_value,dp_value,dm_value;
	usbsel_value = gpio_get_value(chip->usbsel_gpio);
	dp_value = gpio_get_value(chip->qc2sw_dp_gpio);
	dm_value = gpio_get_value(chip->qc2sw_dm_gpio);
	pr_debug("default dp=%d,dm=%d,usbsel=%d\n",
			dp_value,dm_value,usbsel_value);
	return 0;
}
static int bq24192_determine_detect_continue(struct bq24192_chip *chip)
{
	bool usb_present;
	usb_present = bq24192_is_charger_present(chip);
	if (!usb_present) {
		pr_debug("usb off line in hvdcp detected,stop\n");
		return 0;
	}
	return 1;
}
static int bq24192_determine_voltage_request(struct bq24192_chip *chip)
{
	if (chip->hvdcp_retries >= 0 &&
		chip->hvdcp_retries < HVDCP_DETECT_MAX_RETRIES)
		chip->vol_request = USB_REQUEST_9V;//USB_REQUEST_12V;//USB_REQUEST_5V;
	else if (chip->hvdcp_retries >= HVDCP_DETECT_MAX_RETRIES &&
			chip->hvdcp_retries < HVDCP_DETECT_MAX_RETRIES * 2)
		chip->vol_request = USB_REQUEST_9V;
	else if (chip->hvdcp_retries >= HVDCP_DETECT_MAX_RETRIES * 2)
		chip->vol_request = USB_REQUEST_5V;
	pr_debug("request voltage=%dv\n",chip->vol_request);
	return 0;
}
static int bq24192_reduce_input_current(struct bq24192_chip *chip)
{
	int input_limit_ma,chg_limit_ma;
	input_limit_ma = 500;
	chg_limit_ma = IBAT_MIN_MA;
	bq24192_set_input_i_limit(chip, input_limit_ma);
	bq24192_set_ibat_max(chip, chg_limit_ma);
	bq24192_set_secondary_chg_cur(chip,input_limit_ma,chg_limit_ma);

	bq24192_dump_register(chip);
	return 0;
}
static int bq24192_send_voltage_request(struct bq24192_chip *chip,int request_voltage)
{
	bq24192_dump_qc2p0_pin_status(chip);
	switch (request_voltage) {
	case USB_REQUEST_12V:
		gpio_direction_output(chip->qc2sw_dp_gpio,0);
		gpio_direction_output(chip->qc2sw_dm_gpio,0);
		//mdelay(200);
		pr_debug("send request 12v\n");
		break;
	case USB_REQUEST_9V:
		gpio_direction_output(chip->qc2sw_dp_gpio,1);
		gpio_direction_output(chip->qc2sw_dm_gpio,0);
		//mdelay(200);
		pr_debug("send request 9v\n");
		break;
	case USB_REQUEST_5V:
		gpio_direction_output(chip->qc2sw_dp_gpio,0);
		gpio_direction_output(chip->qc2sw_dm_gpio,1);
		//mdelay(200);
		pr_debug("send request 5v\n");
		break;
	default:
		pr_debug("send invalid request\n");
		break;
	}
	bq24192_dump_qc2p0_pin_status(chip);
	return 0;

}

static int bq24192_get_hvdcp_type(struct bq24192_chip *chip,int vbus_mv)
{
	int hvdcp_type;
	int index;

	hvdcp_type = HVDCP_TYPE_UNKOWN;
	for (index = ARRAY_SIZE(hvdcp_vol)-1;index >= 0 ;index--) {
		if (is_between(vbus_mv,hvdcp_vol[index].min_mv,hvdcp_vol[index].max_mv)) {
			pr_debug("vol_mv=%d min_mv=%d,max_mv=%d\n",
					vbus_mv,hvdcp_vol[index].min_mv,hvdcp_vol[index].max_mv);
			hvdcp_type = hvdcp_vol[index].type;
		}
	}
	return hvdcp_type;
}
static int bq24192_read_request_result(struct bq24192_chip *chip)
{
	int rc = 0,vol_mv,vol_type;

	rc = bq24192_get_vbus_voltage(chip,&vol_mv);
	if (rc) {
		pr_err("get vbus error,rc=%d\n",rc);
		return 0;
	}

	chip->vol_output_mv = vol_mv;
	vol_type = bq24192_get_hvdcp_type(chip,vol_mv);
	rc = vol_type == (int)chip->vol_request;
	if (rc) {
		chip->hvdcp_type = (enum hvdcp_type)chip->vol_request;
		pr_debug("request successfully,hvdcp type=%dv",chip->hvdcp_type);
	} else {
		pr_debug("request failed,hvdcp type=%dv",chip->hvdcp_type);
	}
	return rc;
}
static int bq24192_start_ac_power_detect(struct bq24192_chip *chip)
{
	pr_debug("start ac power detect\n");
	wake_lock(&chip->icl_wake_lock);
	schedule_delayed_work(&chip->input_limit_work,
				msecs_to_jiffies(200));
	return 0;
}
#if 1
#define SECONDARY_CHG_INPUT_LIMIT_MAX_MA 900
static int bq24192_config_charger_based_updated_voltage(struct bq24192_chip *chip,
																		int input_limit_ma,int chg_limit_ma)
{
	bq24192_set_input_i_limit(chip, input_limit_ma);
	bq24192_set_ibat_max(chip, chg_limit_ma);
	if(input_limit_ma > SECONDARY_CHG_INPUT_LIMIT_MAX_MA) {
		input_limit_ma = SECONDARY_CHG_INPUT_LIMIT_MAX_MA;
	}
	bq24192_set_secondary_chg_cur(chip,input_limit_ma,chg_limit_ma);
	return 0;

}
#endif
static void bq24192_qc2p0_detect_init(struct bq24192_chip *chip)
{
	int vbat_mv;

	if (!chip->qc2p0_detect_support )
		return;
	//chip->is_hvdcp = false;
	chip->hvdcp_type = HVDCP_TYPE_5V;
	chip->vol_request = USB_REQUEST_5V;
	chip->vol_output_mv = 0;
	chip->hvdcp_retries = 0;
	chip->sec_chger_on = false;
	//chip->qc2p0_retries = 0;
	chip->hvdcp_state = QC2P0_DETECT_STATE_UNDEFINED;
	pr_debug("\n");
	//vbat_mv = bq24192_get_vbat_voltage(chip);
	bq24192_dump_qc2p0_pin_status(chip);
	bq24192_update_bms(chip);
	//vbat_mv = bq24192_get_prop_batt_voltage_now(chip);
	//vbat_mv = vbat_mv/1000;
	vbat_mv = bq24192_get_emulate_ocv(chip);
	if (vbat_mv <= 0) {
		pr_err("get vbat error!\n");
		return;
	}
	if (vbat_mv < chip->cv_thr_mv) {
		pr_debug("vbat=%d<%d=cv_mv,start qc2.0 detect after 5s\n",vbat_mv,chip->cv_thr_mv);
		cancel_delayed_work_sync(&chip->qc2p0_detect_work);
		schedule_delayed_work(&chip->qc2p0_detect_work,
		msecs_to_jiffies(5000));
	} else {
		pr_debug("vbat=%d>%d=cv_mv,changing in default\n",vbat_mv,chip->cv_thr_mv);
	}
	return;
}
static void bq24192_qc2p0_detect_deinit(struct bq24192_chip *chip)
{
	if (!chip->qc2p0_detect_support )
		return;
	pr_debug("stop qc2.0 detect\n");
	bq24192_config_usbsel_status(chip,1);
	bq24192_enable_secondary_chg(chip,0);
	bq24192_dump_qc2p0_pin_status(chip);
	cancel_delayed_work_sync(&chip->qc2p0_detect_work);
}
static void bq24192_qc2p0_detect_work(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work,
				struct bq24192_chip, qc2p0_detect_work.work);
	int rc;
	unsigned int delay = 0;
	bool usb_present;

	usb_present = bq24192_is_charger_present(chip);
	if (!usb_present || !chip->qc2p0_detect_support ||
		chip->usb_psy_type != POWER_SUPPLY_TYPE_USB_DCP) {
		pr_debug("usb off line in hvdcp detected,stop\n");
		return;
	}

	pr_debug("state=%d,qc2p0_retries=%d,hvdcp_retries=%d\n",
			chip->hvdcp_state,chip->qc2p0_retries,chip->hvdcp_retries);
	switch (chip->hvdcp_state) {
	case QC2P0_DETECT_STATE_UNDEFINED:
		rc = bq24192_determine_detect_continue(chip);
		if (rc) {
			if (!chip->usb_switched) {
				pr_debug("switch usb to qc2.0 path\n");
				bq24192_reduce_input_current(chip);
				bq24192_config_usbsel_status(chip,0);
				delay = 200;//1000;//0;//200;
			}
			bq24192_determine_voltage_request(chip);
			chip->hvdcp_state = QC2P0_DETECT_STATE_SEND_REQUEST;
		} else {
			//chip->is_hvdcp = false;
			//chip->hvdcp_state = QC2P0_DETECT_STATE_DETECT_DONE;
			//delay = 0;
			return;
		}
		break;
	case QC2P0_DETECT_STATE_SEND_REQUEST:
		bq24192_send_voltage_request(chip,chip->vol_request);
		chip->hvdcp_state = QC2P0_DETECT_STATE_READ_REQUEST;
		delay = 200;
		break;
	case QC2P0_DETECT_STATE_READ_REQUEST:
		rc = bq24192_read_request_result(chip);
		if (rc) {
			pr_debug("hvdcp request successful\n");
			chip->hvdcp_state = QC2P0_DETECT_STATE_AC_POWER_DETECT;
			delay = 0;
		} else {
			rc = ++chip->hvdcp_retries > HVDCP_DETECT_MAX_RETRIES * 2;
			if (rc) {
				pr_debug("hvdcp request failed\n");
				chip->hvdcp_state = QC2P0_DETECT_STATE_DETECT_DONE;
				delay = 0;
			} else {
				pr_debug("hvdcp request %dv failed,retry=%d\n",chip->vol_request,chip->hvdcp_retries);
				chip->hvdcp_state = QC2P0_DETECT_STATE_UNDEFINED;
				delay = 0;
			}
		}
		break;
	case QC2P0_DETECT_STATE_AC_POWER_DETECT:
		bq24192_enable_secondary_chg(chip,1);
		bq24192_start_ac_power_detect(chip);
		chip->sec_chger_on = true;
		bq24192_vbat_detect_init(chip);
		chip->hvdcp_state = QC2P0_DETECT_STATE_DETECT_DONE;
		delay = 0;
		break;
	case QC2P0_DETECT_STATE_DETECT_DONE:
		if (chip->hvdcp_retries > HVDCP_DETECT_MAX_RETRIES * 2) {
			pr_debug("hvdcp request failed,set to default charging(5v,1.2A)\n");
			bq24192_set_input_i_limit(chip, adap_tbl[2].input_limit);
			bq24192_set_ibat_max(chip, adap_tbl[2].chg_limit);
		}
		pr_debug("detect done hvdcp_type=%dv!\n",chip->hvdcp_type);
		//bq24192_config_charger_based_updated_voltage(chip);
		return;
	default:
		return;
	}
	schedule_delayed_work(&chip->qc2p0_detect_work,
				msecs_to_jiffies(delay));
}

static void bq24192_kick_watchdog(struct bq24192_chip *chip){
	bq24192_dump_register(chip);
	pr_err("kick watchdog\n");
	bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
			WATCHDOG_TIMER_RESET, WATCHDOG_TIMER_RESET);
}

static void bq24192_disable_watchdog(struct bq24192_chip *chip){
	pr_err("disable watchdog\n");
	bq24192_masked_write(chip->client, CHARGE_TERM_TIMER_CONT_REG,
			WATCHDOG_TIMER_MAST, 0x00);
}

static void bq24192_kick_watchdog_work(struct work_struct *work)
{
	struct bq24192_chip *chip =
	container_of(work, struct bq24192_chip, kick_watchdog_work.work);

	bq24192_kick_watchdog(chip);
	schedule_delayed_work(&chip->kick_watchdog_work, msecs_to_jiffies(20000));
}

#define CV_THRESHOLD_DELTA_MV 300
static void bq24192_vbat_work(struct work_struct *work)
{
	struct bq24192_chip *chip =
		container_of(work, struct bq24192_chip, vbat_work.work);
	int vbat_mv,chg_ma,vbus_mv,cv_threshold_mv;
	bool usb_present,chg_charging = true;

	/*bq24192_dump_register(chip);*/
#if 1
	bq24192_masked_write(chip->client, PWR_ON_CONF_REG,
			WATCHDOG_TIMER_RESET, WATCHDOG_TIMER_RESET);
#endif
	//usb_present = bq24192_is_charger_present(chip);
	usb_present = chip->parallel_charger_present;
	if (!usb_present) {
		bq24192_masked_write(chip->client, CHARGE_TERM_TIMER_CONT_REG,
			WATCHDOG_TIMER_MAST, 0x00);
		pr_debug("!usb_present\n");
		wake_unlock(&chip->chg_wake_lock);
		return;
	}
	if (!chip->qc2p0_detect_support) {
		pr_debug("!qc2p0_detect_support\n");
		wake_unlock(&chip->chg_wake_lock);
		goto restart;
	}

	cv_threshold_mv = chip->cv_thr_mv - CV_THRESHOLD_DELTA_MV;
	if (bq24192_get_prop_chg_status(chip) != POWER_SUPPLY_STATUS_CHARGING &&
		bq24192_get_secondary_chg_status(chip) != POWER_SUPPLY_STATUS_CHARGING) {
		chg_charging = false;
	}

	bq24192_dump_qc2p0_pin_status(chip);
	//bq24192_dump_register(chip);
	bq24192_get_vbus_voltage(chip,&vbus_mv);
	bq24192_update_bms(chip);

	vbat_mv = bq24192_get_emulate_ocv(chip);
	chg_ma = bq24192_get_prop_batt_current_now(chip);
	chg_ma /= 1000;
	if (chip->usb_psy_type == POWER_SUPPLY_TYPE_USB || !chip->hvdcp_type)
		goto restart;

	if ((vbat_mv > chip->cv_thr_mv && chg_ma * (-1) < chip->cv_thr_ma) ||
		(!chg_charging)) {
		if (chip->sec_chger_on) {
			pr_debug("turn off secondary charger\n");
			//bq24192_enable_secondary_chg(chip,0);
			chip->sec_chger_on = false;
		}
		if (chip->hvdcp_type == HVDCP_TYPE_12V) {
			pr_debug("request 5V\n");
			chip->vol_request = USB_REQUEST_5V;
			bq24192_send_voltage_request(chip,chip->vol_request);
			msleep(200);
			bq24192_read_request_result(chip);
		}
	} else if (chip->recharging && vbat_mv < cv_threshold_mv) {
		if (!chip->sec_chger_on) {
			pr_debug("turn on secondary charger\n");
			bq24192_enable_secondary_chg(chip,0);
			chip->sec_chger_on = true;
		}
		if (chip->hvdcp_type == HVDCP_TYPE_12V) {
			chip->vol_request = USB_REQUEST_12V;
			bq24192_send_voltage_request(chip,chip->vol_request);
			msleep(200);
			bq24192_read_request_result(chip);

		}
	}
	pr_debug("vbat=%dmv,vbus=%dmv,chg=%dma,\n",vbat_mv,vbus_mv,chg_ma);
restart:
	schedule_delayed_work(&chip->vbat_work, msecs_to_jiffies(10000));

}

static int bq24192_parse_dt(struct device_node *dev_node,
			   struct bq24192_chip *chip)
{
	int ret = 0;

	if (!dev_node) {
		dev_err(&chip->client->dev, "device tree info. missing\n");	
		return -EINVAL;
	}

	ret = of_property_read_u32(dev_node, "ti,chg-current-ma",
				   &(chip->chg_current_ma));
	if (ret) {
		pr_err("Unable to read chg_current.\n");
		return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,term-current-ma",
				   &(chip->term_current_ma));
	if (ret) {
		pr_err("Unable to read term_current_ma.\n");
		return ret;
	}
	//fuelsummary_of_property_put("ti,term-current-ma", PARAMS_TYPE_INT, &(chip->term_current_ma));

	ret = of_property_read_u32(dev_node, "ti,vbat-max-mv",
				   &chip->vbat_max_mv);
	if (ret) {
		pr_err("Unable to read vbat-max-mv.\n");
		return ret;
	}
	chip->default_vbat_max_mv = chip->vbat_max_mv;
	//fuelsummary_of_property_put("ti,vbat-max-mv", PARAMS_TYPE_INT, &(chip->default_vbat_max_mv));

	ret = of_property_read_u32(dev_node, "ti,pre-chg-current-ma",
				   &chip->pre_chg_current_ma);
	if (ret) {
		pr_err("Unable to read pre-chg-current-ma.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,sys-vimin-mv",
				   &chip->sys_vmin_mv);
	if (ret) {
		pr_err("Unable to read sys-vimin-mv.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,vin-limit-mv",
				   &chip->vin_limit_mv);
	if (ret) {
		pr_err("Unable to read vin-limit-mv.\n");
		return ret;
	}
	//fuelsummary_of_property_put("ti,vin-limit-mv", PARAMS_TYPE_INT, &(chip->vin_limit_mv));

	ret = of_property_read_u32(dev_node, "ti,wlc-vin-limit-mv",
				   &chip->wlc_vin_limit_mv);
	if (ret) {
		pr_err("Unable to read wlc-vin-limit-mv.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,step-dwn-current-ma",
				   &chip->step_dwn_currnet_ma);
	if (ret) {
		pr_err("Unable to read step-dwn-current-ma.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,cv-thr-mv",
				   &chip->cv_thr_mv);
	if (ret) {
		pr_err("Unable to read cv threshod voltage.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,icl-vbus-mv",
				   &chip->icl_vbus_mv);
	if (ret) {
		pr_err("Unable to read icl threshod voltage.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,wlc-step-dwn-i-ma",
				   &chip->wlc_dwn_i_ma);
	if (ret) {
		pr_err("Unable to read step down current for wlc.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,wlc-dwn-input-i-ma",
				   &chip->wlc_dwn_input_i_ma);
	if (ret) {
		pr_err("Unable to read step down input i limit for wlc.\n");
		return ret;
	}
	ret = of_property_read_u32(dev_node, "bbk,rsense-mohm",
				   &chip->rsense_mohm);
	if (ret) {
		pr_err("Unable to read step down input i limit for wlc.\n");
		return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,chg-tmout-mins",
				   &(chip->chg_tmout_mins));
	if (ret) {
		pr_err("Unable to read chg-tmout-mins.\n");
		return ret;
	}
	ret = of_property_read_u32(dev_node, "bbk,parallel-iindpm-ma",
				   &(chip->parallel_iindpm_ma));
	if (ret) {
		pr_err("Unable to read parallel-iindpm-ma.\n");
		return ret;
	}

	/* read the bms power supply name */
	ret = of_property_read_string(dev_node, "bbk,bms-psy-name",
						&chip->bms_psy_name);
	if (ret)
		chip->bms_psy_name = NULL;

	/* read the secondary ac power supply name */
	ret = of_property_read_string(dev_node, "bbk,secondary-chg-psy-name",
						&chip->sec_ac_psy_name);
	if (ret)
		chip->sec_ac_psy_name = NULL;

	chip->secondary_charger = of_property_read_bool(dev_node, "bbk,secondary_charger");
	chip->primary_charger = of_property_read_bool(dev_node, "bbk,primary_charger");
	chip->qc2p0_detect_support = of_property_read_bool(dev_node, "bbk,qc2p0_detect_support");
	chip->otg_func_support = of_property_read_bool(dev_node, "bbk,otg_func_support");
	chip->chg_enabled = !(of_property_read_bool(dev_node,
						"bbk,charging-disabled"));

	chip->wlc_support = of_property_read_bool(dev_node, "ti,wlc-support");
	chip->ext_ovp_otg_ctrl = of_property_read_bool(dev_node,"ti,ext-ovp-otg-ctrl");

	chip->int_gpio = of_get_named_gpio(dev_node, "ti,int-gpio", 0);
	if (chip->int_gpio < 0) {
		pr_err("failed to get int-gpio.\n");
		/*return chip->int_gpio;*/
	}

	if(chip->qc2p0_detect_support) {
		/*ret = of_property_read_u32(dev_node, "bbk,vadc_channel",&chip->vadc_channel);
		if (ret) {
			pr_err("Unable to read vadc channel.\n");
			return ret;
		}*/
		chip->usbsel_gpio = of_get_named_gpio(dev_node, "bbk,usbsel-gpio", 0);
		if (chip->usbsel_gpio < 0) {
			pr_err("failed to get int-gpio.\n");
			return chip->int_gpio;
		}
		chip->en_gpio = of_get_named_gpio(dev_node, "bbk,en-gpio", 0);
		if (chip->en_gpio < 0) {
			pr_err("failed to get int-gpio.\n");
			return chip->en_gpio;
		}

		chip->qc2sw_dp_gpio = of_get_named_gpio(dev_node, "bbk,qc2sw-dp-gpio", 0);
		if (chip->qc2sw_dp_gpio < 0) {
			pr_err("failed to get qc2sw_dp_gpio.\n");
			return chip->qc2sw_dp_gpio;
		}

		chip->qc2sw_dm_gpio = of_get_named_gpio(dev_node, "bbk,qc2sw-dm-gpio", 0);
		if (chip->qc2sw_dm_gpio < 0) {
			pr_err("failed to get qc2sw_dm_gpio.\n");
			return chip->qc2sw_dm_gpio;
		}

	}
	if (chip->ext_ovp_otg_ctrl) {
		chip->otg_en_gpio =
			of_get_named_gpio(dev_node, "ti,otg-en-gpio", 0);
		if (chip->otg_en_gpio < 0) {
			pr_err("Unable to get named gpio for otg_en_gpio.\n");
			return chip->otg_en_gpio;
		}
	}

	return ret;
}
static int bq24192_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	struct bq24192_chip *chip = rdev_get_drvdata(rdev);
	bq24192_enable_otg(chip,true);
	return 0;
}

static int bq24192_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	struct bq24192_chip *chip = rdev_get_drvdata(rdev);
	bq24192_enable_otg(chip,false);
	return 0;
}

static int bq24192_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct bq24192_chip *chip = rdev_get_drvdata(rdev);
	ret = bq24192_is_otg_mode(chip);
	return ret;
}

static struct regulator_ops bq24192_chg_otg_reg_ops = {
	.enable		= bq24192_chg_otg_regulator_enable,
	.disable	= bq24192_chg_otg_regulator_disable,
	.is_enabled	= bq24192_chg_otg_regulator_is_enable,
};
static int bq24192_gpio_init(struct bq24192_chip *chip)
{
	int ret = 0;

	if(chip->qc2p0_detect_support) {
		ret = gpio_request(chip->usbsel_gpio,"bq24192_usbsel_gpio");
		if (ret) {
			pr_err("failed to request usbsel_gpio,rc=%d\n",ret);
			//return ret;
		}
		ret = gpio_request(chip->en_gpio,"bq24192_en_gpio");
		if (ret) {
			pr_err("failed to request en_gpio,rc=%d\n",ret);
			//return ret;
		}

		ret = gpio_request(chip->qc2sw_dp_gpio,"bq24192_dp_gpio");
		if (ret) {
			pr_err("failed to request qc2sw_dp_gpio,rc=%d\n",ret);
			//return ret;
		}
		ret = gpio_request(chip->qc2sw_dm_gpio,"bq24192_dm_gpio");
		if (ret) {
			pr_err("failed to request qc2sw_dm_gpio,rc=%d\n",ret);
			//return ret;
		}
	}

	if (chip->int_gpio > 0) {
		ret = gpio_request_one(chip->int_gpio, GPIOF_DIR_IN, "bq24192_int");
		if (ret) {
			pr_err("failed to request int_gpio\n");
			return ret;
		}

		chip->irq = gpio_to_irq(chip->int_gpio);

		if (chip->otg_en_gpio) {
			ret = gpio_request_one(chip->otg_en_gpio,
					GPIOF_OUT_INIT_LOW, "otg_en");
			if (ret) {
				pr_err("otg_en_gpio request failed for %d ret=%d\n",
						chip->otg_en_gpio, ret);
				return ret;
			}
		}
	}
	return ret;
}

static int bq24192_pinctrl_init(struct bq24192_chip *chip)
{
	int ret;

	if (!chip->qc2p0_detect_support)
		return 0;
	chip->pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		dev_err(chip->dev, "Failed to get pinctrl\n");
		return PTR_ERR(chip->pinctrl);
	}

	chip->pin_active= pinctrl_lookup_state(chip->pinctrl,
			"usbsel_active");
	if (IS_ERR_OR_NULL(chip->pin_active)) {
		dev_err(chip->dev, "Failed to look up active state\n");
		return PTR_ERR(chip->pin_active);
	}
	chip->pin_sleep = pinctrl_lookup_state(chip->pinctrl,
			"usbsel_sleep");
	if (IS_ERR_OR_NULL(chip->pin_sleep)) {
		dev_err(chip->dev, "Failed to look up sleep state\n");
		return PTR_ERR(chip->pin_sleep);
	}
	ret = pinctrl_select_state(chip->pinctrl, chip->pin_sleep);
	if (ret) {
		dev_err(chip->dev, "Can't select pinctrl sleep state\n");
		return ret;
	}
	return 0;
}

static int is_parallel_charger(struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;

	return of_property_read_bool(node, "qcom,parallel-charger");
}
static int bq24192_regulator_init(struct bq24192_chip *chip)
{

	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	int ret = 0;

	if (!chip->otg_func_support) {
		dev_err(chip->dev, "otg not supported\n");
		return 0;
	}
	init_data = of_get_regulator_init_data(chip->dev, chip->dev->of_node,&(chip->otg_vreg.rdesc));
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}
	pr_debug("name=%s\n",init_data->constraints.name);
	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &bq24192_chg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = chip->dev->of_node;

		init_data->constraints.valid_ops_mask
		 |= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
				 &chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			ret = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (ret != -EPROBE_DEFER)
			 dev_err(chip->dev,
				 "OTG reg failed, rc=%d\n", ret);
		}

	}
	return ret;
}
static int charger_device_detect(struct bq24192_chip* chip)
{
    int ret;
    u8 data;
    int part_num  = -1;
    /*bq24192 charger detect*/
    ret = bq24192_read_reg(chip->client, VENDOR_PART_REV_STATUS_REG, &data);
    if(ret == 0){
        part_num = (data & BQ24192_PN_MASK) >> BQ24192_PN_SHIFT;
		pr_warn("bq2419x product number:0xA=0x%x\n",part_num);
		if(part_num == BQ24192)
			return part_num;
    }else{
    	pr_err("bq2419x device no found:%d\n",ret);
		part_num = -1;
    }
    /*bq25892 charger detect*/
    ret = bq24192_read_reg(chip->client, BQ25890_REG_14, &data);
    if(ret == 0){
        part_num = (data & BQ25890_PN_MASK) >> BQ25890_PN_SHIFT;
		pr_warn("bq2589x product number:0x14=0x%x\n",part_num);
		if(part_num == BQ25892)
			return part_num;
	} else {
		pr_err("bq2589x device no found:%d\n", ret);
		part_num = -1;
	}

	/* bq25601d charger detect */
	ret = bq24192_read_reg(chip->client, BQ25601D_REG_0B, &data);
	if (ret == 0) {
		part_num = (data & BQ25601D_PN_MASK) >> BQ25601D_PN_SHIFT;
		pr_warn("bq25601d product number:0x0B=0x%x\n", part_num);
		if (part_num == BQ25601D)
			return part_num;
	} else {
		pr_err("bq2589x device no found:%d\n", ret);
		part_num = -1;
	}

	/* bq24192 charger detect */
	ret = bq24192_read_reg(chip->client, VENDOR_PART_REV_STATUS_REG, &data);
	if (ret == 0) {
		part_num = (data & BQ24192_PN_MASK) >> BQ24192_PN_SHIFT;
		pr_warn("bq2419x product number:0xA=0x%x\n", part_num);
		if (part_num == BQ24192)
			return part_num;
	} else {
		pr_err("bq2419x device no found:%d\n", ret);
		part_num = -1;
	}

	return part_num;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = bq24192_battery_properties,
	.num_properties = ARRAY_SIZE(bq24192_battery_properties),
	.get_property = bq24192_battery_get_property,
	.set_property = bq24192_battery_set_property,
	.property_is_writeable = bq24192_battery_is_writeable,
	.external_power_changed = bq24192_external_battery_changed,
};

static int bq24192_init_batt_psy(struct bq24192_chip* chip)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	batt_cfg.drv_data = chip;
	batt_cfg.of_node = chip->dev->of_node;
	batt_cfg.supplied_to = bq24192_battery_supplied_to;
	batt_cfg.num_supplicants = ARRAY_SIZE(bq24192_battery_supplied_to);
	chip->batt_psy = devm_power_supply_register(chip->dev,
						   &batt_psy_desc,
						   &batt_cfg);
	if (IS_ERR(chip->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chip->batt_psy);
	}

	return rc;
}

static int bq24192_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct device_node *dev_node = client->dev.of_node;
	struct bq24192_chip *chip;
	struct power_supply *usb_psy = NULL;
	int ret = 0;
	int part_num = -1;
	int ram_sz = 0;
	char *board_version = NULL;	/* vivo add */
pr_err("bq24192_probe start\n");

	if (of_property_read_bool(dev_node, "vivo,1957-dual-version-compatible")) {
		pr_err("Enter PD1957F_EX slave charger determination\n");
		ram_sz = totalram_pages * 4096 / (1024 * 1024 * 1024);
		if (!ram_sz) {
			pr_info("failed to get ram_sz\n");
			return -ENODEV;
		} else {
			/*Dual charger ram_sz: 3(4-1), single charger ram_sz: 2(3-1) */
			pr_info("ram_sz=%d\n", ram_sz);
			if (ram_sz == 2) {
				pr_info("PD1957 3G RAM version, disable slave charger driver\n");
				return -ENODEV;
			}
		}
	}

	if (of_property_read_bool(dev_node, "qcom,pd1965-flag")) {
		pr_err("Enter PD1965 slave charger determination\n");
		board_version = get_bbk_board_version();
		if (!board_version) {
			pr_info("failed to get board_version\n");
			return -ENODEV;
		} else {
			/*Dual charger board: "11111101", single charger board: "11111100"*/
			pr_info("board_version=%s\n", board_version);
			if(board_version[7] == '0') {
				pr_info("Disable slave charger driver\n");
				return -ENODEV;
			}
		}
	}

	if (!client) {
		pr_err("i2c client null.\n");
		return -EIO;
	}
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c func fail.\n");
		return -EIO;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("usb supply not found\n");
		//return -EPROBE_DEFER;
	}

	chip = kzalloc(sizeof(struct bq24192_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}
	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;
	chip->cv_thr_mv = 4300;
	chip->cv_thr_ma = 2000;

	ret = bq24192_parse_dt(dev_node, chip);
	if (ret) {
		pr_err("failed to parse dt\n");
		goto error;
	}

	if (chip->qc2p0_detect_support) {
		/*chip->vadc_dev = qpnp_get_vadc(chip->dev, "usbvol");
		if (IS_ERR(chip->vadc_dev)) {
			ret = PTR_ERR(chip->vadc_dev);
			pr_err("failed to get vadc,defer\n");
			if (ret != -EPROBE_DEFER)
				pr_err("vadc property missing\n");
			goto error;
		}*/
	}

	chip->set_chg_current_ma = chip->chg_current_ma;
	chip->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	chip->max_input_i_ma = INPUT_CURRENT_LIMIT_MAX_MA;
	chip->irq_scheduled_time_status = 0;
	chip->irq = 0;
	
	i2c_set_clientdata(client, chip);

	part_num = charger_device_detect(chip);
	if(part_num == BQ24192){
		pr_err("bq24192 device found!\n");
		parallel_charger_type = BQ24192;
		//fuelsummary_collect_value(ID_SIC_VENDOR, CHGIC_BQ24192);
	} else if (part_num == BQ25601D) {
		kfree(chip);
		parallel_charger_type = BQ25601D;
		ret = bq25601d_probe(client,id);
		return ret;
	} else if (part_num == BQ25892) {
		kfree(chip);
		parallel_charger_type = BQ25892;
		ret = bq25890_probe(client,id);
		return ret;
	} else {
		pr_err("no charger device found:%d\n",ret);
		goto error;
	}
	the_chip = chip;
	
	spin_lock_init(&chip->irq_work_lock);

	wake_lock_init(&chip->chg_wake_lock,
		       WAKE_LOCK_SUSPEND, BQ24192_NAME);
	wake_lock_init(&chip->icl_wake_lock,
		       WAKE_LOCK_SUSPEND, "icl_wake_lock");
	wake_lock_init(&chip->irq_wake_lock,
			WAKE_LOCK_SUSPEND, BQ24192_NAME "irq");

	INIT_DELAYED_WORK(&chip->vbat_work, bq24192_vbat_work);
	INIT_DELAYED_WORK(&chip->input_limit_work, bq24192_input_limit_worker);
	INIT_DELAYED_WORK(&chip->therm_work, bq24192_therm_mitigation_work);
	INIT_WORK(&chip->irq_work, bq24192_irq_worker);
	INIT_DELAYED_WORK(&chip->qc2p0_detect_work, bq24192_qc2p0_detect_work);
	INIT_DELAYED_WORK(&chip->kick_watchdog_work, bq24192_kick_watchdog_work);

	ret = bq24192_regulator_init(chip);
	if (ret) {
		pr_err("Couldn't initialize regulator rc=%d\n", ret);
	}

	ret = bq24192_pinctrl_init(chip);
	if (ret) {
		pr_err("Couldn't initialize pinctrl rc=%d\n", ret);
		goto free_regulator;
	}

	ret = bq24192_gpio_init(chip);
	if (ret) {
		pr_err("Couldn't initialize gpio rc=%d\n", ret);
		goto free_gpio;
	}
	bq24192_dump_register(chip);
	ret = bq24192_hw_init(chip);

	if (!chip->parallel_charger_present) {
			bq24192_masked_write(chip->client, CHARGE_TERM_TIMER_CONT_REG,
					WATCHDOG_TIMER_MAST, 0x00);
			pr_debug("!usb_present disable watch dog\n");
	}

	if (ret) {
		//ret = -EPROBE_DEFER;
		pr_err("failed to init hw\n");
		goto free_gpio;
	}

	if (chip->primary_charger) {
		ret = bq24192_init_batt_psy(chip);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"Unable to register batt_psy rc = %d\n", ret);
			goto free_gpio;
		}
		if (chip->wlc_support) {
			chip->wlc_psy = power_supply_get_by_name("wireless");
			if (!chip->wlc_psy) {
				pr_err("wireless supply not found deferring probe\n");
				ret = -EPROBE_DEFER;
				goto unregister_ac_psy;
			}
		}
	}

	if (is_parallel_charger(client)) {
		chip->parallel_charger = true;
		ret = bq24192_init_parallel_psy(chip);
	} else {
		ret = bq24192_init_ac_psy(chip);
	}
	if (ret) {
		pr_err("bq24192_init_ac_psy failed\n");
		goto unregister_batt_psy;
	}


	if (chip->irq) {
		ret = request_irq(chip->irq, bq24192_irq,
				IRQF_TRIGGER_FALLING,
				"bq24192_irq", chip);
		if (ret) {
			pr_err("request_irq %d failed\n", chip->irq);
			goto unregister_ac_psy;
		}
		enable_irq_wake(chip->irq);
	}
	ret = bq24192_create_debugfs_entries(chip);
	if (ret) {
		pr_err("bq24192_create_debugfs_entries failed\n");
		goto unregister_ac_psy;
	}
	#if 0
	spin_lock_irqsave(&chip->irq_work_lock, flags);
	if (chip->irq_scheduled_time_status == 0) {
		schedule_work(&chip->irq_work);
		chip->irq_scheduled_time_status = 20;
	}
	spin_unlock_irqrestore(&chip->irq_work_lock, flags);
	#endif
	bq24192_enable_charging(chip, chip->chg_enabled);
	bq24192_enable_hiz(chip, !chip->chg_enabled);
	dev_info(&chip->client->dev,"probe success secondary charger=%d,"
		"primary charger=%d chg_enbaled=%d,parallel_charger=%d\n",
		chip->secondary_charger,chip->primary_charger,chip->chg_enabled,chip->parallel_charger);

	return 0;

unregister_ac_psy:
	power_supply_unregister(chip->ac_psy);
unregister_batt_psy:
	power_supply_unregister(chip->batt_psy);
free_gpio:
	if (chip->usbsel_gpio > 0)
		gpio_free(chip->usbsel_gpio);
	if (chip->qc2sw_dp_gpio > 0)
		gpio_free(chip->qc2sw_dp_gpio);
	if (chip->qc2sw_dm_gpio > 0)
		gpio_free(chip->qc2sw_dm_gpio);
	if (chip->int_gpio > 0)
		gpio_free(chip->int_gpio);
free_regulator:
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
	wake_lock_destroy(&chip->chg_wake_lock);
	wake_lock_destroy(&chip->icl_wake_lock);
	wake_lock_destroy(&chip->irq_wake_lock);
error:
	kfree(chip);
	pr_warn("fail to probe\n");
	return ret;

}
static int bq24192_remove_interface(struct i2c_client *client)
{
	struct bq24192_chip *chip = i2c_get_clientdata(client);
	if (chip->irq)
		free_irq(chip->irq, chip);
	wake_lock_destroy(&chip->chg_wake_lock);
	wake_lock_destroy(&chip->icl_wake_lock);
	wake_lock_destroy(&chip->irq_wake_lock);

	if (chip->dent)
		debugfs_remove_recursive(chip->dent);
	power_supply_unregister(chip->ac_psy);
	if (chip->otg_en_gpio)
		   gpio_free(chip->otg_en_gpio);
	if (chip->int_gpio)
		gpio_free(chip->int_gpio);
	kfree(chip);

	return 0;
}


static int bq24192_suspend(struct device *dev)
{
	pr_err("parallel_charger_type:0x%x\n",parallel_charger_type);
	if (parallel_charger_type == BQ25601D)
		bq25601d_suspend(dev);
	pr_warn("suspend\n");
	return 0;
}

static int bq24192_resume(struct device *dev)
{
	pr_err("parallel_charger_type:0x%x\n",parallel_charger_type);
	if (parallel_charger_type == BQ25601D)
		bq25601d_resume(dev);
	pr_warn("resume\n");
	return 0;
}

static const struct dev_pm_ops bq24192_pm_ops = {
	.suspend	= bq24192_suspend,
	.resume		= bq24192_resume,
};

static int bq24192_remove(struct i2c_client *client)
{
	pr_err("parallel_charger_type:0x%x\n",parallel_charger_type);
	if (parallel_charger_type == BQ25892)
		bq25890_remove(client);
	else if (parallel_charger_type == BQ25601D)
		bq25601d_remove(client);
	else if (parallel_charger_type == BQ24192)
		bq24192_remove_interface(client);
	else
		return 0;
	return 0;
}

static const struct i2c_device_id bq24192_id[] = {
	{BQ24192_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq24192_id);

static const struct of_device_id bq24192_match[] = {
	{ .compatible = "ti,bq24192-bq25892-hybrid", },
	{ },
};

static struct i2c_driver bq24192_driver = {
	.driver	= {
			.name	= BQ24192_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = of_match_ptr(bq24192_match),
			.pm		= &bq24192_pm_ops,
	},
	.probe		= bq24192_probe,
	.remove		= bq24192_remove,
	.id_table	= bq24192_id,
};

static int __init bq24192_init(void)
{
	return i2c_add_driver(&bq24192_driver);
}
module_init(bq24192_init);

static void __exit bq24192_exit(void)
{
	return i2c_del_driver(&bq24192_driver);
}
module_exit(bq24192_exit);

MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_DESCRIPTION("BQ24192 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" BQ24192_NAME);
