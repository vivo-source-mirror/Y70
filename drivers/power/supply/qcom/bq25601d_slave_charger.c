/*****************************************************************************
 *
 * Filename:
 * ---------
 *       bq25601d.c
 *
 * Project:
 * --------
 *       Android
 *
 * Description:
 * ------------
 *       charge IC
 *
 * Author:
 * -------
 *       @vivo fuxi
 *
 ****************************************************************************/
#define pr_fmt(fmt) "bq25601d: %s: " fmt, __func__


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
//#include <linux/qpnp/qpnp-adc.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
//#include <linux/bbk_drivers_info.h>

#include "vivo-fuel_summary.h"

#undef pr_info
#define pr_info(fmt, ...) printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)


struct reg {
	uint8_t index;
	uint8_t addr;
	uint8_t mask;
	uint8_t shift;
};

#define INIT_REG(i, a, m, s) \
{ \
	.index = i, \
	.addr = a, \
	.mask = m, \
	.shift = s, \
}

typedef enum {
	REG0_EN_HIZ,
	REG0_EN_ICHG_MON,
	REG0_IINLIM,

	REG1_PFM_DIS,
	REG1_WD_RST,
	REG1_OTG_CONFIG,
	REG1_CHG_CONFIG,
	REG1_SYS_MIN,
	REG1_MIN_VBAT_SEL,

	REG2_BOOST_LIM,
	REG2_Q1_FULLON,
	REG2_ICHG,

	REG3_IPRECHG,
	REG3_ITERM,

	REG4_VREG,
	REG4_TOPOFF_TIMER,
	REG4_VRECHG,

	REG5_EN_TERM,
	REG5_WATCHDOG,
	REG5_EN_TIMER,
	REG5_CHG_TIMER,
	REG5_TREG,
	REG5_JEITA_ISET,

	REG6_OVP,
	REG6_BOOSTV,
	REG6_VINDPM,

	REG7_IINDET_EN,
	REG7_TMR2X_EN,
	REG7_BATFET_DIS,
	REG7_JEITA_VSET,
	REG7_BATFET_DLY,
	REG7_BATFET_RST_EN,
	REG7_VDPM_BAT_TRAC,

	REG8_VBUS_STAT,
	REG8_CHRG_STAT,
	REG8_PG_STAT,
	REG8_THERM_STAT,
	REG8_VSYS_STAT,

	REG9_WATCHDOG_FAULT,
	REG9_BOOST_FAULT,
	REG9_CHRG_FAULT,
	REG9_BAT_FAULT,
	REG9_NTC_FAULT,

	REGA_VBUS_GD,
	REGA_VINDPM_STAT,
	REGA_IINDPM_STAT,
	REGA_TOPOFF_ACTIVE,
	REGA_ACOV_STAT,
	REGA_VINDPM_INT_MASK,
	REGA_IINDPM_INT_MASK,

	REGB_REG_RST,
	REGB_PN,
	REGB_DEV_REV,
} REG_ENUM;

static struct reg regs[] = {
	INIT_REG(REG0_EN_HIZ,			0x0, 0x1, 7),
	INIT_REG(REG0_EN_ICHG_MON,		0x0, 0x3, 5),
	INIT_REG(REG0_IINLIM,			0x0, 0x1f, 0),

	INIT_REG(REG1_PFM_DIS,			0x1, 0x1, 7),
	INIT_REG(REG1_WD_RST,			0x1, 0x1, 6),
	INIT_REG(REG1_OTG_CONFIG,		0x1, 0x1, 5),
	INIT_REG(REG1_CHG_CONFIG,		0x1, 0x1, 4),
	INIT_REG(REG1_SYS_MIN,			0x1, 0x7, 1),
	INIT_REG(REG1_MIN_VBAT_SEL,		0x1, 0x1, 0),

	INIT_REG(REG2_BOOST_LIM,		0x2, 0x1, 7),
	INIT_REG(REG2_Q1_FULLON,		0x2, 0x1, 6),
	INIT_REG(REG2_ICHG,			0x2, 0x3f, 0),

	INIT_REG(REG3_IPRECHG,			0x3, 0xf, 4),
	INIT_REG(REG3_ITERM,			0x3, 0xf, 0),

	INIT_REG(REG4_VREG,			0x4, 0x1f, 3),
	INIT_REG(REG4_TOPOFF_TIMER,		0x4, 0x3, 1),
	INIT_REG(REG4_VRECHG,			0x4, 0x1, 0),

	INIT_REG(REG5_EN_TERM,			0x5, 0x1, 7),
	INIT_REG(REG5_WATCHDOG,			0x5, 0x3, 4),
	INIT_REG(REG5_EN_TIMER,			0x5, 0x1, 3),
	INIT_REG(REG5_CHG_TIMER,		0x5, 0x1, 2),
	INIT_REG(REG5_TREG,			0x5, 0x1, 1),
	INIT_REG(REG5_JEITA_ISET,		0x5, 0x1, 0),

	INIT_REG(REG6_OVP,			0x6, 0x3, 6),
	INIT_REG(REG6_BOOSTV,			0x6, 0x3, 4),
	INIT_REG(REG6_VINDPM,			0x6, 0xf, 0),

	INIT_REG(REG7_IINDET_EN,		0x7, 0x1, 7),
	INIT_REG(REG7_TMR2X_EN,			0x7, 0x1, 6),
	INIT_REG(REG7_BATFET_DIS,		0x7, 0x1, 5),
	INIT_REG(REG7_JEITA_VSET,		0x7, 0x1, 4),
	INIT_REG(REG7_BATFET_DLY,		0x7, 0x1, 3),
	INIT_REG(REG7_BATFET_RST_EN,		0x7, 0x1, 2),
	INIT_REG(REG7_VDPM_BAT_TRAC,		0x7, 0x3, 0),

	INIT_REG(REG8_VBUS_STAT,		0x8, 0x7, 5),
	INIT_REG(REG8_CHRG_STAT,		0x8, 0x3, 3),
	INIT_REG(REG8_PG_STAT,			0x8, 0x1, 2),
	INIT_REG(REG8_THERM_STAT,		0x8, 0x1, 1),
	INIT_REG(REG8_VSYS_STAT,		0x8, 0x1, 0),

	INIT_REG(REG9_WATCHDOG_FAULT,		0x9, 0x1, 7),
	INIT_REG(REG9_BOOST_FAULT,		0x9, 0x1, 6),
	INIT_REG(REG9_CHRG_FAULT,		0x9, 0x3, 4),
	INIT_REG(REG9_BAT_FAULT,		0x9, 0x1, 3),
	INIT_REG(REG9_NTC_FAULT,		0x9, 0x7, 0),

	INIT_REG(REGA_VBUS_GD,			0xa, 0x1, 7),
	INIT_REG(REGA_VINDPM_STAT,		0xa, 0x1, 6),
	INIT_REG(REGA_IINDPM_STAT,		0xa, 0x1, 5),
	INIT_REG(REGA_TOPOFF_ACTIVE,		0xa, 0x1, 3),
	INIT_REG(REGA_ACOV_STAT,		0xa, 0x1, 2),
	INIT_REG(REGA_VINDPM_INT_MASK,		0xa, 0x1, 1),
	INIT_REG(REGA_IINDPM_INT_MASK,		0xa, 0x1, 0),

	INIT_REG(REGB_REG_RST,			0xb, 0x1, 7),
	INIT_REG(REGB_PN,			0xb, 0xf, 3),
	INIT_REG(REGB_DEV_REV,			0xb, 0x3, 0),
};


#define BQ25601D_NAME			"bq25601d_charger"
#define BQ25601D_SLAVE_ADDR		0x6b

#define CHG_REG_SET(n, value) \
	do { \
		if (n == regs[n].index) \
			bq25601d_config_interface(regs[n].addr, value, regs[n].mask, regs[n].shift); \
		else \
			pr_info("! config reg err:%d\n", n); \
	} while(0)


#define CHG_REG_GET(n, value) \
	do { \
		if (n == regs[n].index) \
			bq25601d_read_interface(regs[n].addr, value, regs[n].mask, regs[n].shift); \
		else \
			pr_info("! read reg err:%d\n", n); \
	} while(0)


struct bq25601d_chip {
	struct i2c_client *client;
	struct device *dev;
	struct mutex i2c_mutex;
	struct power_supply *parallel_psy;
	int parallel_charger_present;
	int vbat_max_mv;
	int vin_limit_mv;
	int input_limit_ma;
	int set_chg_current_ma;
	int term_current_ma;
	int ircomp_mom;
	int vclamp_mv;
	int int_gpio;
	int irq;
	struct wake_lock  irq_wake_lock;
	struct work_struct  irq_work;
	int irq_scheduled_time_status;
	spinlock_t irq_work_lock;
	struct delayed_work  monitor_work;
	int			c_charger_temp_max;
	bool suspend;
};

static struct i2c_client *this_client = NULL;
static int charge_dump_register(struct bq25601d_chip *chip);

/**********************************************************
 *
 *   [I2C Function For Read/Write bq25601d]
 *
 *********************************************************/
int bq25601d_read_byte(uint8_t cmd, uint8_t *data)
{
	int ret = 0;
	struct bq25601d_chip *chip;

	if (!this_client) {
		pr_info("bq25601d i2c read client is not exist\n");
		*data = 0x0;
		return -EEXIST;
	}

	chip = i2c_get_clientdata(this_client);

	if (chip->suspend) {
		pr_err("device in suspend, skip IIC Control\n");
		return -EINVAL;
	}
	
	mutex_lock(&(chip->i2c_mutex));
	ret = i2c_smbus_read_byte_data(this_client, cmd);
	mutex_unlock(&(chip->i2c_mutex));
	if (ret < 0)
		pr_info("read error(%d)\n", ret);
	else
		*data = ret;

	return ret;
}

int bq25601d_write_byte(uint8_t cmd, uint8_t data)
{
	int ret = 0;
	struct bq25601d_chip *chip;

	if (!this_client) {
		pr_info("bq25601d i2c write client is not exist\n");
		return -EEXIST;
	}

	chip = i2c_get_clientdata(this_client);

	if (chip->suspend) {
		pr_err("device in suspend, skip IIC Control\n");
		return -EINVAL;
	}

	mutex_lock(&(chip->i2c_mutex));
	ret = i2c_smbus_write_byte_data(this_client, cmd, data);
	mutex_unlock(&(chip->i2c_mutex));
	if (ret < 0)
		pr_info("write error(%d)\n", ret);

	return ret;
}

int bq25601d_read_interface(uint8_t reg, uint8_t *data, uint8_t mask, uint8_t shift)
{
	int ret = 0;
	uint8_t value = 0;

	ret = bq25601d_read_byte(reg, &value);
	//pr_debug("reg[%x]=0x%x\n", reg, value);

	value &= (mask << shift);
	*data = (value >> shift);
	//pr_debug("data=0x%x\n", *data);

	return ret;
}

int bq25601d_config_interface(uint8_t reg, uint8_t data, uint8_t mask, uint8_t shift)
{
	int ret = 0;
	uint8_t value = 0;

	ret = bq25601d_read_byte(reg, &value);
	//pr_debug("reg[%x]=0x%x\n", reg, value);

	data &= mask;
	value &= ~(mask << shift);
	value |= (data << shift);

	ret = bq25601d_write_byte(reg, value);
	//pr_debug("write reg[%x]=0x%x\n", reg, value);

	return ret;
}


/************************************************************
 *
 *   [bq25601d control]
 *
 ***********************************************************/
static int charge_set_ilimit(int ilimit)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (ilimit >= 100 && ilimit <= 3200) {
		value = (ilimit - 100) / 100;
		CHG_REG_GET(REG0_IINLIM, &pre_value);
		if (value != pre_value) {
			pr_info("ilimit[%d], pre_value=0x%x, value=0x%x\n",
					ilimit, pre_value, value);
			CHG_REG_SET(REG0_IINLIM, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("ilimit[%d] is out overflow\n", ilimit);
	}

	return ret;
}

static int charge_set_ichg(int ichg)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (ichg >= 0 && ichg <= 3000) {
		value = ichg / 60;
		CHG_REG_GET(REG2_ICHG, &pre_value);
		if (value != pre_value) {
			pr_info("ichg[%d], pre_value=0x%x, value=0x%x\n",
					ichg, pre_value, value);
			CHG_REG_SET(REG2_ICHG, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("ichg[%d] is out overflow\n", ichg);
	}

	return ret;
}

static int charge_set_voreg(int voreg)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (voreg >= 3847 && voreg <= 4615) {
		value = (voreg - 3847) / 32;
		CHG_REG_GET(REG4_VREG, &pre_value);
		if (value != pre_value) {
			pr_info("voreg[%d], pre_value=0x%x, value=0x%x\n",
					voreg, pre_value, value);
			CHG_REG_SET(REG4_VREG, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("voreg[%d] is out overflow\n", voreg);
	}

	return ret;
}

static int charge_set_vinlimit(int vinlimit)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (vinlimit >= 3900 && vinlimit <= 5400) {
		value = (vinlimit - 3900) / 100;
		CHG_REG_GET(REG6_VINDPM, &pre_value);
		if (value != pre_value) {
			pr_info("vinlimit[%d], pre_value=0x%x, value=0x%x\n",
					vinlimit, pre_value, value);
			CHG_REG_SET(REG6_VINDPM, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("vinlimit[%d] is out overflow\n", vinlimit);
	}

	return ret;
}

static int charge_set_iterm(int iterm)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (iterm >= 60 && iterm <= 960) {
		value = (iterm - 60) / 60;
		CHG_REG_GET(REG3_ITERM, &pre_value);
		if (value != pre_value) {
			pr_info("iterm[%d], pre_value=0x%x, value=0x%x\n",
					iterm, pre_value, value);
			CHG_REG_SET(REG3_ITERM, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("iterm[%d] is out overflow\n", iterm);
	}

	return ret;
}

static int charge_set_IRcompensation(int bat_comp, int vclamp)
{
	int ret = 0;

	pr_info("bq25601d not suppert IRcompensation\n");
	return ret;
}

typedef enum {
	OVP_5_5V = 0x0,
	OVP_6_5V,
	OVP_10_5V,
	OVP_14V,
} OVP_ENUM;
static int charge_set_ovp(int v)
{
	int ret = 0;
	uint8_t value, pre_value;

	if (v >= OVP_5_5V && v <= OVP_14V) {
		value = v;
		CHG_REG_GET(REG6_OVP, &pre_value);
		if (value != pre_value) {
			pr_info("ovp: pre_value=0x%x, value=0x%x\n",
					pre_value, value);
			CHG_REG_SET(REG6_OVP, value);
		}
	} else {
		ret = -EOVERFLOW;
		pr_info("ovp[%d] is out overflow\n", v);
	}

	return ret;
}

static int charge_hw_init(struct bq25601d_chip *chip)
{
	int ret = 0;

	pr_err("bq25601d hw_init\n");

	/*watch dog reset*/
	CHG_REG_SET(REG1_WD_RST, 0x1);

	/*all register reset*/
	CHG_REG_SET(REGB_REG_RST, 0x1);

	/*watch dog timer 40s*/
	CHG_REG_SET(REG5_WATCHDOG, 0x1);

	/*set input ovp 10.5v*/
	charge_set_ovp(OVP_10_5V);

	/*vfloat 4.4 (4.391)*/
	charge_set_voreg(chip->vbat_max_mv);

	/*ibat current:500*/
	charge_set_ichg(500);

	/*termination current:128*/
	charge_set_iterm(chip->term_current_ma);

	/*not support*/
	charge_set_IRcompensation(chip->ircomp_mom, chip->vclamp_mv);
	//CHG_REG_SET(REG0_EN_HIZ, 0x1);

	charge_dump_register(chip);

	return ret;
}

static const int DUMP_REGS[]= {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb
};
static int charge_dump_register(struct bq25601d_chip *chip)
{
	int ret = 0, i;
	char str[512];
	char val[24];
	uint8_t value = 0;
	uint8_t reg_fault = 0, reg_status = 0, reg_dpm = 0;

	CHG_REG_SET(REG1_WD_RST, 0x1);
	memset(str, 0x0, 512);
	for (i=0; i<ARRAY_SIZE(DUMP_REGS); i++) {
		ret = bq25601d_read_byte(DUMP_REGS[i], &value);
		memset(val, 0x0, 24);
		sprintf(val, "[0x%02x]=0x%02x ", DUMP_REGS[i], value);
		strcat(str, val);
		if (0x8 == DUMP_REGS[i]) {
			reg_status = value;
		} else if (0x9 == DUMP_REGS[i]) {
			reg_fault = value;
		} else if (0xa == DUMP_REGS[i]) {
			reg_dpm = value;
		}
	}
	pr_info("%s\n", str);
	//fuelsummary_collect_value(ID_SIC_FAULT, reg_fault);
	//fuelsummary_collect_value(ID_SIC_NPG, ((reg_status & 0x04) != 0x04));
	//fuelsummary_collect_value(ID_SIC_DPM, ((reg_dpm & 0x40) >> 6));
	return ret;
}

static int bq25601d_init_watchdog(struct bq25601d_chip *chip)
{
	pr_info("reset watch dog and enable watch dog!\n");
	/*watch dog reset*/
	CHG_REG_SET(REG1_WD_RST, 0x1);
	/*watch dog timer 40s*/
	CHG_REG_SET(REG5_WATCHDOG, 0x1);

	return 0;
}

/************************************************************
 *
 *   [bq25601d function]
 *
 ***********************************************************/
static int charge_set_chg_ichg(struct bq25601d_chip *chip, int ichg)
{
	int ret = 0;

	pr_warn("fastchg_current=%d\n", ichg);
	ret = charge_set_vinlimit(chip->vin_limit_mv);
	ret = charge_set_ichg(ichg);
	chip->set_chg_current_ma = ichg;

	return ret;
}

#define SUSPEND_CURRENT_MA		2
static int charge_set_chg_ilimit(struct bq25601d_chip *chip, int ilimit)
{
	int ret = 0;

	if (ilimit <= SUSPEND_CURRENT_MA) {
		CHG_REG_SET(REG0_EN_HIZ, 1);
		msleep(250);
		CHG_REG_SET(REG1_CHG_CONFIG, 0);
		pr_info("bq25601d suspend\n");
		chip->input_limit_ma = 0;
	} else {
		CHG_REG_SET(REG0_EN_HIZ, 0);
		msleep(250);
		ret = charge_set_ilimit(ilimit);
		CHG_REG_SET(REG1_CHG_CONFIG, 1);
	}

	return ret;
}

#define	BQ25601D_STATUS_DEFAULT_OK	0
#define	BQ25601D_STATUS_I2C_ERROR	1
static int charge_get_slave_status(struct bq25601d_chip *chip)
{
	int ret = 0, i = 0;
	uint8_t value;

	ret = bq25601d_read_byte(i, &value);
	if (ret < 0) {
		pr_info("failed to read DEVICE_ID ret=%d\n", ret);
		for (i=0; i<3; i++) {
			msleep(1);
			ret = bq25601d_read_byte(i, &value);
			if (ret < 0) {
				pr_info("retry[%d] failed to read DEVICE_ID ret=%d\n", i, ret);
			} else {
				break;
			}
		}
	}

	if (i >= 3)
		return BQ25601D_STATUS_I2C_ERROR;
	else
		return BQ25601D_STATUS_DEFAULT_OK;
}

static int charge_get_chg_type(struct bq25601d_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	uint8_t chg_stat;

	CHG_REG_GET(REG8_CHRG_STAT, &chg_stat);

	if (chg_stat == 0x1)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	pr_info("CHG STATUS: %d\n", chg_type);
	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_FAST)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int charge_get_chg_enabled(struct bq25601d_chip *chip)
{
	int ret = 0;
	uint8_t value;

	CHG_REG_GET(REG1_CHG_CONFIG, &value);
	if (value == 0x1)
		ret = 1;

	return ret;
}

static int charge_parallel_set_chg_present(struct bq25601d_chip *chip, int present)
{
	int ret = 0;

	pr_err("present=%d\n", present);
	if (present == chip->parallel_charger_present) {
		pr_info("present %d -> %d, skipping\n",
				chip->parallel_charger_present, present);
		return 0;
	}

	if (present) {
		cancel_delayed_work_sync(&chip->monitor_work);
		bq25601d_init_watchdog(chip);
		schedule_delayed_work(&chip->monitor_work, msecs_to_jiffies(5000));
	} else {
		CHG_REG_SET(REG5_WATCHDOG, 0);
		CHG_REG_SET(REG1_CHG_CONFIG, 0);
		CHG_REG_SET(REG0_EN_HIZ, 1);
	}

	chip->parallel_charger_present = present;
	return ret;
}

static enum power_supply_property bq25601d_parallel_properties[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
};

static int bq25601d_parallel_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int ret = 0;
	struct bq25601d_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (chip->parallel_charger_present) {
			CHG_REG_SET(REG1_CHG_CONFIG, val->intval);
			CHG_REG_SET(REG0_EN_HIZ, !val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = charge_parallel_set_chg_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_charger_present) {
			ret = charge_set_chg_ichg(chip, val->intval / 1000);
			//fuelsummary_collect_value(ID_SIC_INPUT, val->intval / 1000);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_charger_present)
			ret = charge_set_chg_ilimit(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (chip->parallel_charger_present && (chip->vbat_max_mv != val->intval / 1000))
			ret = charge_set_voreg(val->intval / 1000);

		chip->vbat_max_mv = val->intval / 1000;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (chip->parallel_charger_present) {
			CHG_REG_SET(REG1_CHG_CONFIG, !val->intval);
			CHG_REG_SET(REG0_EN_HIZ, val->intval);
		}
		//if (chip->parallel_charger_present && !val->intval)
		//	fuelsummary_collect_value(ID_SIC_STAT, 1);
		//else
		//	fuelsummary_collect_value(ID_SIC_STAT, 0);
		break;
	case POWER_SUPPLY_PROP_WATCH_DOG_STATUS:
		CHG_REG_SET(REG5_WATCHDOG, !val->intval);
		break;
	case POWER_SUPPLY_PROP_SLAVE_SUSPEND_STATUS:
		CHG_REG_SET(REG0_EN_HIZ, val->intval);
		break;
	case POWER_SUPPLY_PROP_INIT_SLAVE_CHARGER:
		if (chip->parallel_charger_present)
			charge_hw_init(chip);
		break;
	case POWER_SUPPLY_PROP_OTG_DISABLE_PL:
		pr_info("otg disable pl =%d\n", val->intval);
		CHG_REG_SET(REG1_CHG_CONFIG, !val->intval);
		CHG_REG_SET(REG0_EN_HIZ, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		chip->c_charger_temp_max = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bq25601d_parallel_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		return 0;
	}
}

static int bq25601d_parallel_get_property(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	struct bq25601d_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charge_get_chg_enabled(chip);
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
			val->intval = charge_get_chg_type(chip);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_CHECK_SLAVE_CHARGER_STATUS:
		val->intval = charge_get_slave_status(chip);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN_EXT;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
		val->intval = POWER_SUPPLY_PL_NON_STACKED_BATFET;
		break;
//	case POWER_SUPPLY_PROP_PARALLEL_FCC_MIN:
//		val->intval = 0;
//		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		val->intval = chip->c_charger_temp_max;
		break;
	case POWER_SUPPLY_PROP_DUMP_REG:
		val->intval = charge_dump_register(chip);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct power_supply_desc parallel_psy_desc = {
	.name			= "parallel",
	.type			= POWER_SUPPLY_TYPE_PARALLEL,
	.properties		= bq25601d_parallel_properties,
	.num_properties		= ARRAY_SIZE(bq25601d_parallel_properties),
	.get_property		= bq25601d_parallel_get_property,
	.set_property		= bq25601d_parallel_set_property,
	.property_is_writeable	= bq25601d_parallel_is_writeable,
};

static int bq25601d_init_parallel_psy(struct bq25601d_chip *chip)
{
	struct power_supply_config parallel_cfg = {};

	parallel_cfg.drv_data = chip;
	parallel_cfg.of_node = chip->dev->of_node;
	chip->parallel_psy = devm_power_supply_register(chip->dev, &parallel_psy_desc, &parallel_cfg);
	if (IS_ERR(chip->parallel_psy)) {
		pr_info("Couldn't register parallel power supply\n");
		return PTR_ERR(chip->parallel_psy);
	}

	return 0;
}

static int bq25601d_parse_dt(struct bq25601d_chip *chip)
{
	int ret = 0;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		pr_info("device tree missing\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "ti,chg-current-ma", &(chip->set_chg_current_ma));
	if (ret) {
		pr_info("Unable to read chg_current\n");
	}
	ret = of_property_read_u32(node, "ti,term-current-ma", &(chip->term_current_ma));
	if (ret) {
		pr_info("Unable to read term_current\n");
	}
	//fuelsummary_of_property_put("ti,term-current-ma", PARAMS_TYPE_INT, &(chip->term_current_ma));

	ret = of_property_read_u32(node, "ti,vbat-max-mv", &(chip->vbat_max_mv));
	if (ret) {
		pr_info("Unable to read vbat_max\n");
	}
	//fuelsummary_of_property_put("ti,vbat-max-mv", PARAMS_TYPE_INT, &(chip->vbat_max_mv));

	ret = of_property_read_u32(node, "ti,vin-limit-mv", &(chip->vin_limit_mv));
	if (ret) {
		pr_info("Unable to read vin_limit\n");
	}
	//fuelsummary_of_property_put("ti,vin-limit-mv", PARAMS_TYPE_INT, &(chip->vin_limit_mv));

	ret = of_property_read_u32(node, "ti,ircomp-mom", &(chip->ircomp_mom));
	if (ret) {
		pr_info("Unable to read ircomp_mom\n");
	}
	ret = of_property_read_u32(node, "ti,vclamp-mv", &(chip->vclamp_mv));
	if (ret) {
		pr_info("Unable to read vclamp_mv\n");
	}

	chip->int_gpio = of_get_named_gpio(node, "ti,int-gpio", 0);
	if (chip->int_gpio < 0) {
		pr_err("not define int-gpio.\n");
		/*return chip->int_gpio;*/
	}

	return 0;
}

static int bq25601d_gpio_init(struct bq25601d_chip *chip)
{
	int ret = 0;

	if (chip->int_gpio > 0) {
		ret = gpio_request_one(chip->int_gpio, GPIOF_DIR_IN, "bq25601d_int");
		if (ret) {
			pr_err("failed to request int_gpio\n");
			return ret;
		}

		chip->irq = gpio_to_irq(chip->int_gpio);
	}
	return 0;
}

static void bq25601d_irq_work(struct work_struct *work)
{
	struct bq25601d_chip *chip =
		container_of(work, struct bq25601d_chip, irq_work);
	unsigned long flags;
	uint8_t pg_stat;

	wake_lock(&chip->irq_wake_lock);

	msleep(100 * chip->irq_scheduled_time_status);

	CHG_REG_GET(REG8_PG_STAT, &pg_stat);
	pr_err("power good stat = %d\n", pg_stat);

	spin_lock_irqsave(&chip->irq_work_lock, flags);
	chip->irq_scheduled_time_status = 0;
	spin_unlock_irqrestore(&chip->irq_work_lock, flags);

	wake_lock_timeout(&chip->irq_wake_lock, 2*HZ);
}

static irqreturn_t bq25601d_irq(int irq, void *dev_id)
{
	struct bq25601d_chip *chip = dev_id;
	unsigned long flags;

	spin_lock_irqsave(&chip->irq_work_lock, flags);
	if (chip->irq_scheduled_time_status == 0) {
		schedule_work(&chip->irq_work);
		chip->irq_scheduled_time_status = 1;
	}
	spin_unlock_irqrestore(&chip->irq_work_lock, flags);

	return IRQ_HANDLED;
}

static void bq25601d_monitor_work(struct work_struct *work)
{
	struct bq25601d_chip *chip =
		container_of(work, struct bq25601d_chip, monitor_work.work);
	bool usb_present;

	/*watch dog reset*/
	CHG_REG_SET(REG1_WD_RST, 0x1);

	usb_present = chip->parallel_charger_present;
	if (!usb_present) {
		pr_info("!usb_present\n");
		CHG_REG_SET(REG5_WATCHDOG, 0);
		return;
	}

	schedule_delayed_work(&chip->monitor_work, msecs_to_jiffies(10000));
}

static struct bq25601d_chip chg_chip = {
	.parallel_charger_present = 0,
	.vbat_max_mv = 4380,
	.vin_limit_mv = 4400,
	.input_limit_ma = 900,
	.set_chg_current_ma = 1500,
	.term_current_ma = 100,
	.ircomp_mom = 0,
	.vclamp_mv = 0,
};

/************************************************************
 *
 *   [bq25601d I2C driver]
 *
 ***********************************************************/
int bq25601d_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct bq25601d_chip *chip = &chg_chip;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_info("i2c func fail.\n");
		return -EIO;
	}

	chip->client = client;
	chip->dev = &(client->dev);
	i2c_set_clientdata(client, chip);
	mutex_init(&(chip->i2c_mutex));
	this_client = client;
	chip->suspend = false;

	if (!charge_get_slave_status(chip)) {
		pr_info("bq25601d device found!\n");
		//fuelsummary_collect_value(ID_SIC_VENDOR, CHGIC_BQ25601D);
	} else {
		pr_info("bq25601d device not found!\n");
		goto error;
	}

	ret = bq25601d_parse_dt(chip);
	if (ret) {
		pr_info("failed to parse dt\n");
		goto error;
	}

	spin_lock_init(&chip->irq_work_lock);
	wake_lock_init(&chip->irq_wake_lock,
			WAKE_LOCK_SUSPEND, "bq25601d_irq_wakelock");
	INIT_WORK(&chip->irq_work, bq25601d_irq_work);
	INIT_DELAYED_WORK(&chip->monitor_work, bq25601d_monitor_work);

	ret = bq25601d_gpio_init(chip);
	if (ret) {
		pr_err("Couldn't initialize gpio rc=%d\n", ret);
		if (chip->int_gpio > 0)
			gpio_free(chip->int_gpio);
	}

	if (chip->irq) {
		ret = request_irq(chip->irq, bq25601d_irq,
				IRQF_TRIGGER_FALLING,
				"bq25601d_irq", chip);
		if (ret) {
			pr_err("request_irq %d failed\n", chip->irq);
			if (chip->int_gpio > 0)
				gpio_free(chip->int_gpio);
		}
		enable_irq_wake(chip->irq);
	}

	ret = bq25601d_init_parallel_psy(chip);
	if (ret)
		pr_info("parallel_psy regist charge Fail\n");
	else
		pr_info("parallel_psy regist charge Success\n");

	charge_hw_init(chip);

	/*init charge status:disable_wd,disable_chg & hiz*/
	CHG_REG_SET(REG5_WATCHDOG, 0x0);
	CHG_REG_SET(REG1_CHG_CONFIG, 0x0);
	CHG_REG_SET(REG0_EN_HIZ, 0x1);

	return 0;
error:
	return ret;
}
EXPORT_SYMBOL(bq25601d_probe);

int bq25601d_suspend(struct device *dev)
{
	struct bq25601d_chip *chip = dev_get_drvdata(dev);
	chip->suspend = true;
	pr_err("suspend\n");
	return 0;
}
EXPORT_SYMBOL(bq25601d_suspend);
int bq25601d_resume(struct device *dev)
{
	struct bq25601d_chip *chip = dev_get_drvdata(dev);
	chip->suspend = false;
	pr_err("resume\n");
	return 0;
}
EXPORT_SYMBOL(bq25601d_resume);
int bq25601d_remove(struct i2c_client *client)
{
	struct bq25601d_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(chip->parallel_psy);

	return 0;
}
EXPORT_SYMBOL(bq25601d_remove);


MODULE_AUTHOR("Sif");
MODULE_DESCRIPTION("Charger IC ba25601d Device Driver");
MODULE_LICENSE("GPL");
