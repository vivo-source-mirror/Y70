/*
 * BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#define pr_fmt(fmt)	"BQ25890-SLAVE:%s: " fmt, __func__
#define BQ25890_NAME  "bq25890-slave-charger"
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
/*#include <linux/qpnp/qpnp-adc.h>*/
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "vivo-fuel_summary.h"

#ifdef pr_debug
#undef pr_debug
#define pr_debug(fmt, ...) \
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* Register 00h */
#define BQ25890_REG_00      		0x00
#define BQ25890_ENHIZ_MASK		    0x80
#define BQ25890_ENHIZ_SHIFT		    7
#define BQ25890_ENHIZ_VALUE			1
#define BQ25890_HIZ_ENABLE          1
#define BQ25890_HIZ_DISABLE         0
#define BQ25890_ENILIM_MASK		    0x40
#define BQ25890_ENILIM_SHIFT		6
#define BQ25890_ENILIM_ENABLE       1
#define BQ25890_ENILIM_DISABLE      0

#define BQ25890_IINLIM_MASK		    0x3F
#define BQ25890_IINLIM_SHIFT		0
#define BQ25890_IINLIM_BASE         100
#define BQ25890_IINLIM_LSB          50

/* Register 01h */
#define BQ25890_REG_01		    	0x01
#define BQ25890_BHOT_MASK           0xC0
#define BQ25890_BHOT_SHIFT          6
#define BQ25890_BCOLD_MASK          0x20
#define BQ25890_BCOLD_SHIFT         5
#define BQ25890_VINDPMOS_MASK       0x1F
#define BQ25890_VINDPMOS_SHIFT      0

#define BQ25890_VINDPMOS_BASE       0
#define BQ25890_VINDPMOS_LSB        100


/* Register 0x02 */
#define BQ25890_REG_02              0x02
#define BQ25890_CONV_START_MASK      0x80
#define BQ25890_CONV_START_SHIFT     7
#define BQ25890_CONV_START           0
#define BQ25890_CONV_RATE_MASK       0x40
#define BQ25890_CONV_RATE_SHIFT      6
#define BQ25890_ADC_CONTINUE_ENABLE  1
#define BQ25890_ADC_CONTINUE_DISABLE 0

#define BQ25890_BOOST_FREQ_MASK      0x20
#define BQ25890_BOOST_FREQ_SHIFT     5
#define BQ25890_BOOST_FREQ_1500K     0
#define BQ25890_BOOST_FREQ_500K      0

#define BQ25890_ICOEN_MASK          0x10
#define BQ25890_ICOEN_SHIFT         4
#define BQ25890_ICO_ENABLE          1
#define BQ25890_ICO_DISABLE         0
#define BQ25890_HVDCPEN_MASK        0x08
#define BQ25890_HVDCPEN_SHIFT       3
#define BQ25890_HVDCP_ENABLE        1
#define BQ25890_HVDCP_DISABLE       0
#define BQ25890_MAXCEN_MASK         0x04
#define BQ25890_MAXCEN_SHIFT        2
#define BQ25890_MAXC_ENABLE         1
#define BQ25890_MAXC_DISABLE        0

#define BQ25890_FORCE_DPDM_MASK     0x02
#define BQ25890_FORCE_DPDM_SHIFT    1
#define BQ25890_FORCE_DPDM          1
#define BQ25890_AUTO_DPDM_EN_MASK   0x01
#define BQ25890_AUTO_DPDM_EN_SHIFT  0
#define BQ25890_AUTO_DPDM_ENABLE    1
#define BQ25890_AUTO_DPDM_DISABLE   0


/* Register 0x03 */
#define BQ25890_REG_03              0x03
#define BQ25890_BAT_LOADEN_MASK     0x80
#define BQ25890_BAT_LOAEN_SHIFT     7
#define BQ25890_WDT_RESET_MASK      0x40
#define BQ25890_WDT_RESET_SHIFT     6
#define BQ25890_WDT_RESET           1

#define BQ25890_OTG_CONFIG_MASK     0x20
#define BQ25890_OTG_CONFIG_SHIFT    5
#define BQ25890_OTG_ENABLE          1
#define BQ25890_OTG_DISABLE         0

#define BQ25890_CHG_CONFIG_MASK     0x10
#define BQ25890_CHG_CONFIG_SHIFT    4
#define BQ25890_CHG_ENABLE          1
#define BQ25890_CHG_DISABLE         0


#define BQ25890_SYS_MINV_MASK       0x0E
#define BQ25890_SYS_MINV_SHIFT      1

#define BQ25890_SYS_MINV_BASE       3000
#define BQ25890_SYS_MINV_LSB        100


/* Register 0x04*/
#define BQ25890_REG_04              0x04
#define BQ25890_EN_PUMPX_MASK       0x80
#define BQ25890_EN_PUMPX_SHIFT      7
#define BQ25890_PUMPX_ENABLE        1
#define BQ25890_PUMPX_DISABLE       0
#define BQ25890_ICHG_MASK           0x7F
#define BQ25890_ICHG_SHIFT          0
#define BQ25890_ICHG_BASE           0
#define BQ25890_ICHG_LSB            64

/* Register 0x05*/
#define BQ25890_REG_05              0x05
#define BQ25890_IPRECHG_MASK        0xF0
#define BQ25890_IPRECHG_SHIFT       4
#define BQ25890_ITERM_MASK          0x0F
#define BQ25890_ITERM_SHIFT         0
#define BQ25890_IPRECHG_BASE        64
#define BQ25890_IPRECHG_LSB         64
#define BQ25890_ITERM_BASE          64
#define BQ25890_ITERM_LSB           64

/* Register 0x06*/
#define BQ25890_REG_06              0x06
#define BQ25890_VREG_MASK           0xFC
#define BQ25890_VREG_SHIFT          2
#define BQ25890_BATLOWV_MASK        0x02
#define BQ25890_BATLOWV_SHIFT       1
#define BQ25890_BATLOWV_2800MV      0
#define BQ25890_BATLOWV_3000MV      1
#define BQ25890_VRECHG_MASK         0x01
#define BQ25890_VRECHG_SHIFT        0
#define BQ25890_VRECHG_100MV        0
#define BQ25890_VRECHG_200MV        1
#define BQ25890_VREG_BASE           3840
#define BQ25890_VREG_LSB            16

/* Register 0x07*/
#define BQ25890_REG_07              0x07
#define BQ25890_EN_TERM_MASK        0x80
#define BQ25890_EN_TERM_SHIFT       7
#define BQ25890_TERM_ENABLE         1
#define BQ25890_TERM_DISABLE        0

#define BQ25890_WDT_MASK            0x30
#define BQ25890_WDT_SHIFT           4
#define BQ25890_WDT_DISABLE         0
#define BQ25890_WDT_40S             1
#define BQ25890_WDT_80S             2
#define BQ25890_WDT_160S            3
#define BQ25890_WDT_BASE            0
#define BQ25890_WDT_LSB             40

#define BQ25890_EN_TIMER_MASK       0x08
#define BQ25890_EN_TIMER_SHIFT      3

#define BQ25890_CHG_TIMER_ENABLE    1
#define BQ25890_CHG_TIMER_DISABLE   0

#define BQ25890_CHG_TIMER_MASK      0x06
#define BQ25890_CHG_TIMER_SHIFT     1
#define BQ25890_CHG_TIMER_5HOURS    0
#define BQ25890_CHG_TIMER_8HOURS    1
#define BQ25890_CHG_TIMER_12HOURS   2
#define BQ25890_CHG_TIMER_20HOURS   3

#define BQ25890_JEITA_ISET_MASK     0x01
#define BQ25890_JEITA_ISET_SHIFT    0
#define BQ25890_JEITA_ISET_50PCT    0
#define BQ25890_JEITA_ISET_20PCT    1


/* Register 0x08*/
#define BQ25890_REG_08              0x08
#define BQ25890_BAT_COMP_MASK       0xE0
#define BQ25890_BAT_COMP_SHIFT      5
#define BQ25890_VCLAMP_MASK         0x1C
#define BQ25890_VCLAMP_SHIFT        2
#define BQ25890_TREG_MASK           0x03
#define BQ25890_TREG_SHIFT          0
#define BQ25890_TREG_60C            0
#define BQ25890_TREG_80C            1
#define BQ25890_TREG_100C           2
#define BQ25890_TREG_120C           3

#define BQ25890_BAT_COMP_BASE       0
#define BQ25890_BAT_COMP_LSB        20
#define BQ25890_VCLAMP_BASE         0
#define BQ25890_VCLAMP_LSB          32


/* Register 0x09*/
#define BQ25890_REG_09              0x09
#define BQ25890_FORCE_ICO_MASK      0x80
#define BQ25890_FORCE_ICO_SHIFT     7
#define BQ25890_FORCE_ICO           1
#define BQ25890_TMR2X_EN_MASK       0x40
#define BQ25890_TMR2X_EN_SHIFT      6
#define BQ25890_BATFET_DIS_MASK     0x20
#define BQ25890_BATFET_DIS_SHIFT    5
#define BQ25890_BATFET_OFF          1

#define BQ25890_JEITA_VSET_MASK     0x10
#define BQ25890_JEITA_VSET_SHIFT    4
#define BQ25890_JEITA_VSET_N150MV   0
#define BQ25890_JEITA_VSET_VREG     1
#define BQ25890_BATFET_RST_EN_MASK  0x04
#define BQ25890_BATFET_RST_EN_SHIFT 2
#define BQ25890_PUMPX_UP_MASK       0x02
#define BQ25890_PUMPX_UP_SHIFT      1
#define BQ25890_PUMPX_UP            1
#define BQ25890_PUMPX_DOWN_MASK     0x01
#define BQ25890_PUMPX_DOWN_SHIFT    0
#define BQ25890_PUMPX_DOWN          1


/* Register 0x0A*/
#define BQ25890_REG_0A              0x0A
#define BQ25890_BOOSTV_MASK         0xF0
#define BQ25890_BOOSTV_SHIFT        4
#define BQ25890_BOOSTV_BASE         4550
#define BQ25890_BOOSTV_LSB          64
#define BQ25890_BOOSTV_MAX          5510


#define BQ25890_BOOST_LIM_MASK      0x07
#define BQ25890_BOOST_LIM_SHIFT     0
#define BQ25890_BOOST_LIM_500MA     0x00
#define BQ25890_BOOST_LIM_750MA     0x01
#define BQ25890_BOOST_LIM_1200MA    0x02
#define BQ25890_BOOST_LIM_1400MA    0x03
#define BQ25890_BOOST_LIM_1650MA    0x04
#define BQ25890_BOOST_LIM_1875MA    0x05
#define BQ25890_BOOST_LIM_2150MA    0x06
#define BQ25890_BOOST_LIM_2450MA    0x07
#define BQ25890_BOOST_IINLIM_MIN    500
#define BQ25890_BOOST_IINLIM_MAX    2450


/* Register 0x0B*/
#define BQ25890_REG_0B              0x0B
#define BQ25890_VBUS_STAT_MASK      0xE0
#define BQ25890_VBUS_STAT_SHIFT     5
#define BQ25890_CHRG_STAT_MASK      0x18
#define BQ25890_CHRG_STAT_SHIFT     3
#define BQ25890_CHRG_STAT_IDLE      0
#define BQ25890_CHRG_STAT_PRECHG    1
#define BQ25890_CHRG_STAT_FASTCHG   2
#define BQ25890_CHRG_STAT_CHGDONE   3

#define BQ25890_PG_STAT_MASK        0x04
#define BQ25890_PG_STAT_SHIFT       2
#define BQ25890_PG_STAT_VALUE       1
#define BQ25890_SDP_STAT_MASK       0x02
#define BQ25890_SDP_STAT_SHIFT      1
#define BQ25890_VSYS_STAT_MASK      0x01
#define BQ25890_VSYS_STAT_SHIFT     0
#define BQ25890_VBUS_HVDCP_SHIFT	7
#define BQ25890_VBUS_HVDCP_VALUE		4


/* Register 0x0C*/
#define BQ25890_REG_0C              0x0c
#define BQ25890_FAULT_WDT_MASK      0x80
#define BQ25890_FAULT_WDT_SHIFT     7
#define BQ25890_FAULT_BOOST_MASK    0x40
#define BQ25890_FAULT_BOOST_SHIFT   6
#define BQ25890_FAULT_CHRG_MASK     0x30
#define BQ25890_FAULT_CHRG_SHIFT    4
#define BQ25890_FAULT_CHRG_NORMAL   0
#define BQ25890_FAULT_CHRG_INPUT    1
#define BQ25890_FAULT_CHRG_THERMAL  2
#define BQ25890_FAULT_CHRG_TIMER    3

#define BQ25890_FAULT_BAT_MASK      0x08
#define BQ25890_FAULT_BAT_SHIFT     3
#define BQ25890_FAULT_NTC_MASK      0x07
#define BQ25890_FAULT_NTC_SHIFT     0
#define BQ25890_FAULT_NTC_TSCOLD    1
#define BQ25890_FAULT_NTC_TSHOT     2

#define BQ25890_FAULT_NTC_WARM      2
#define BQ25890_FAULT_NTC_COOL      3
#define BQ25890_FAULT_NTC_COLD      5
#define BQ25890_FAULT_NTC_HOT       6


/* Register 0x0D*/
#define BQ25890_REG_0D              0x0D
#define BQ25890_FORCE_VINDPM_MASK   0x80
#define BQ25890_FORCE_VINDPM_SHIFT  7
#define BQ25890_FORCE_VINDPM_ENABLE 1
#define BQ25890_FORCE_VINDPM_DISABLE 0
#define BQ25890_VINDPM_MASK         0x7F
#define BQ25890_VINDPM_SHIFT        0

#define BQ25890_VINDPM_BASE         2600
#define BQ25890_VINDPM_LSB          100


/* Register 0x0E*/
#define BQ25890_REG_0E              0x0E
#define BQ25890_THERM_STAT_MASK     0x80
#define BQ25890_THERM_STAT_SHIFT    7
#define BQ25890_BATV_MASK           0x7F
#define BQ25890_BATV_SHIFT          0
#define BQ25890_BATV_BASE           2304
#define BQ25890_BATV_LSB            20


/* Register 0x0F*/
#define BQ25890_REG_0F              0x0F
#define BQ25890_SYSV_MASK           0x7F
#define BQ25890_SYSV_SHIFT          0
#define BQ25890_SYSV_BASE           2304
#define BQ25890_SYSV_LSB            20


/* Register 0x10*/
#define BQ25890_REG_10              0x10
#define BQ25890_TSPCT_MASK          0x7F
#define BQ25890_TSPCT_SHIFT         0
#define BQ25890_TSPCT_BASE          21
#define BQ25890_TSPCT_LSB           465//should be 0.465,kernel does not support float

/* Register 0x11*/
#define BQ25890_REG_11              0x11
#define BQ25890_VBUS_GD_MASK        0x80
#define BQ25890_VBUS_GD_SHIFT       7
#define BQ25890_VBUS_GD_VALUE       1
#define BQ25890_VBUSV_MASK          0x7F
#define BQ25890_VBUSV_SHIFT         0
#define BQ25890_VBUSV_BASE          2600
#define BQ25890_VBUSV_LSB           100


/* Register 0x12*/
#define BQ25890_REG_12              0x12
#define BQ25890_ICHGR_MASK          0x7F
#define BQ25890_ICHGR_SHIFT         0
#define BQ25890_ICHGR_BASE          0
#define BQ25890_ICHGR_LSB           50


/* Register 0x13*/
#define BQ25890_REG_13              0x13
#define BQ25890_VDPM_STAT_MASK      0x80
#define BQ25890_VDPM_STAT_SHIFT     7
#define BQ25890_VDPM_STAT_VALUE		1
#define BQ25890_IDPM_STAT_MASK      0x40
#define BQ25890_IDPM_STAT_SHIFT     6
#define BQ25890_IDPM_STAT_VALUE		1
#define BQ25890_IDPM_LIM_MASK       0x3F
#define BQ25890_IDPM_LIM_SHIFT      0
#define BQ25890_IDPM_LIM_BASE       100
#define BQ25890_IDPM_LIM_LSB        50


/* Register 0x14*/
#define BQ25890_REG_14              0x14
#define BQ25890_RESET_MASK          0x80
#define BQ25890_RESET_SHIFT         7
#define BQ25890_RESET               1
#define BQ25890_ICO_OPTIMIZED_MASK  0x40
#define BQ25890_ICO_OPTIMIZED_SHIFT 6
#define BQ25890_PN_MASK             0x38
#define BQ25890_PN_SHIFT            3
#define BQ25890_TS_PROFILE_MASK     0x04
#define BQ25890_TS_PROFILE_SHIFT    2
#define BQ25890_DEV_REV_MASK        0x03
#define BQ25890_DEV_REV_SHIFT       0

enum bq25890_vbus_type {
	BQ25890_VBUS_NONE,
	BQ25890_VBUS_USB_SDP,
	BQ25890_VBUS_USB_CDP,
	BQ25890_VBUS_USB_DCP,
	BQ25890_VBUS_HVDCP,
	BQ25890_VBUS_UNKNOWN,
	BQ25890_VBUS_NONSTAND,
	BQ25890_VBUS_OTG,
    BQ25890_VBUS_TYPE_NUM,
};
enum wake_reason {
	PM_PARALLEL_CHECK = BIT(0),
	PM_IRQ_CHECK = BIT(1),
	PM_POWER_CHECK = BIT(2),
};
enum bq25890_part_no{
    BQ25890 = 0x03,
    BQ25892 = 0x00,
    BQ25895 = 0x07,
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
enum power_detect_state {
	POWER_DETECT_STATE_UNDEFINED = 0,
	POWER_DETECT_STATE_FIRST = 0,
	POWER_DETECT_STATE_FIRST_RESULT,
	POWER_DETECT_STATE_SECOND,
	POWER_DETECT_STATE_SECOND_RESULT,
	POWER_DETECT_STATE_FAIL,
	POWER_DETECT_STATE_DONE,
};
enum {
	bq25890_CHG_OV_STATUS = 0,
	bq25890_CHG_TIMEOUT_STATUS = 4,
};
struct bq25890_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct parallel_usb_cfg {
	struct power_supply		*psy;
	int				min_current_thr_ma;
	int				min_9v_current_thr_ma;
	int				allowed_lowering_ma;
	int				input_current_ma;
	int 			fastchg_current_ma;
	int				fastchg_current_max_ma;
	bool			avail;
	struct mutex	lock;
	int				initial_ico_ma;
	ktime_t			last_disabled;
	bool			enabled;
};

#define BQ25890_STATUS_PLUGIN      	0x0001    //plugin
#define BQ25890_STATUS_PG          	0x0002    //power good
//#define BQ25890_STATUS_CHG_DONE    	0x0004
#define BQ25890_STATUS_FAULT    	0x0008
#define BQ25890_STATUS_ATTACHED    	0x0010

#define BQ25890_STATUS_EXIST		0x0100
#define BQ25890_STATUS_CHARGE_ENABLE 0x0200

#define DEFAULT_WALL_CHG_MA	1800
#define DEFAULT_SDP_MA		500
#define DEFAULT_CDP_MA		1500
#define DEFAULT_UNKNOWN_MA	900

struct bq25890_chip {
	struct device *dev;
	struct i2c_client *client;

    enum	bq25890_part_no part_no;
    int		revision;
	int		irq;
	int  	irq_gpio;
	int		en_gpio;
	int		usbsel_gpio;

    int     rsoc;
	int		wake_reasons;

	struct work_struct irq_work;
	struct work_struct adapter_in_work;
	struct work_struct adapter_out_work;
	struct delayed_work monitor_work;
	struct delayed_work otg_monitor_work;
	struct delayed_work rerun_apsd_work;
	struct delayed_work chgdone_work;

	struct mutex		pm_lock;
	struct mutex		current_change_lock;
	struct mutex		fcc_lock;
	struct wake_lock  	irq_wake_lock;


	struct bq25890_regulator otg_vreg;
	/*struct qpnp_vadc_chip   *vadc_dev;*/
	bool 					is_support_otg;
	bool 					primary_charger;
	bool					chg_enabled;

	struct power_supply *usb_psy;
	struct power_supply  *ac_psy;
	struct power_supply  *batt_psy;
	struct power_supply	 *bms_psy;
	const char			 *bms_psy_name;

	struct dentry		*debug_root;

	/*parallel control config*/
	struct power_supply	 	*parallel_psy;
	struct parallel_usb_cfg	parallel;
	struct delayed_work		parallel_en_work;
	struct delayed_work		power_detect_work;
	bool					parallel_chgr_present;
	bool					parallel_chgr_enabled;

	/*input power detected config*/
	int						pd_state;
	int						pd_ico_retries;
	int						pd_retries;
	int						ico_ma;
	int						ico_idx;

	/* config  total current parameters */
	int				total_input_current_ma;
	int				cfg_input_current_ma;
	int				input_current_ma;
	int 			tl_input_current_ma;
	int				total_fastchg_current_ma;
	int				cfg_fastchg_current_ma;
	int				fastchg_current_ma;
	int				fastchg_current_max_ma;
	int				fastchg_acc;

	int  			vbat_max_mv;
	int  			term_current_ma;
	int  			pre_chg_current_ma;
	int  			ircomp_mom;
	int				vclamp_mv;

	int				vbat_mv;
	int				ibat_ma;
    int     		vbus_mv;
	int				vindpm_mv;
	int				vindpm_9v_thr_mv;
	int 			vindpm_5v_thr_mv;
	int 			vindpm_offset_mv;
	int				hvdcp_type;
	int				check_vbus_count;
	bool			use_absolute_vindpm;
	bool			hvdcp_enable;

	int				usb_online;
	int  			ac_online;
	int	 			usb_psy_type;
	int				vbus_type;
	unsigned int    status;
	bool			usb_present;

    int				chg_tmout_mins;
	int				chg_type;
    unsigned long   charge_begin;
	long			psy_status;


	/*battery soc config*/
	int  			batt_1C_current_ma;
	int				batt_ircomp_mom;
	int 			batt_health;
	int				fake_battery_soc;
	bool			recharging;
	bool			vbus_hized;
	bool			chg_done;
	int 			soc;
	int 			ui_soc;
	int 			cal_soc;
	int 			last_soc;
	bool  			full_flag;
	bool			batt_full;
	bool			batt_full_flag;
	int				adb_first_ibat_current[6];//main ibat ic
	int				adb_second_ibat_current[6];//second ibat ic
	int				adb_first_input_current;//main input ic
	int				adb_second_input_current;//second input ic
	int				adb_set_current_enable;//user set current enable
	int 			adb_index;
	int 			adb_timer;

	int				nor_first_ibat_current[2];//main ibat ic
	int				nor_second_ibat_current[2];//second ibat ic
	int				nor_set_current_enable;//user set current enable
	int 			nor_index;
	int 			nor_timer;
	int				nor_timer_limit;

	int				sw_first_ibat_current[2];//main ibat ic
	int				sw_second_ibat_current[2];//second ibat ic
	int				sw_set_current_enable;//user set current enable
	int 			sw_index;
	int 			sw_timer;
	int				sw_timer_limit;

	int				fbon_first_ibat_current[2];//main ibat ic
	int				fbon_second_ibat_current[2];//second ibat ic
	int				fbon_set_current_enable;//user set current enable

	int				calling_first_ibat_current[2];//main ibat ic
	int				calling_second_ibat_current[2];//second ibat ic
	int				calling_set_current_enable;//user set current enable

	int 			adjust_dpm_flag;
	bool 			adjust_dpm_enable;

	int				dcp_retries;
	bool			dcp_retries_flag;

	int				batt_core_acc;
	int				otg_boost_ilim;
	int				otg_boost_vlim;

	struct delayed_work rerun_base_fb_on_work;
	int				fb_on_ajust_adapt;
	int				iic_state;

	struct mutex		chgdone_lock;

	int				resume_hvdcp_count;

	bool			disable_parallel_charger;
	bool			hvdcp_to_5V_enable;
	int				hvdcp_to_5V_soc;
	bool			hvdcp_to_5V_flag;

	int			c_charger_temp_max;
};

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
struct current_limit_entry {
	int input_limit;
	int fastchg_limit;
};
#if 0
static struct current_limit_entry adap_tbl[] = {
	{2000, 512},
	{2000, 2048},
	//{2000, 1200},
	//{2000, 1500},
	//{2000, 1800},
	//{2000, 2000},
	//{2000, 2500},
	//{2000, 2500},
	//{2000, 2500},
};
#endif
static bool bbk_cmcc_attribute = false;
static int bbk_fixed_temp = 0;
static int bq25890_parallel_en = 1;
module_param_named(
	parallel_en, bq25890_parallel_en, int, S_IRUSR | S_IWUSR
);

static unsigned int power_off_charging_mode;

static struct bq25890_chip *the_chip;

static DEFINE_MUTEX(bq25890_i2c_lock);
static int is_between(int value, int left, int right)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}
static int bq25890_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;

	mutex_lock(&bq25890_i2c_lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read 0x%.2x\n", reg);
		mutex_unlock(&bq25890_i2c_lock);
		return ret;
	}

	*val = (u8)ret;
	mutex_unlock(&bq25890_i2c_lock);

	return 0;
}

static int bq25890_write_reg(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	mutex_lock(&bq25890_i2c_lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&bq25890_i2c_lock);

	if (ret < 0) {
		dev_err(&client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	//pr_debug("write 0x%02X,value=0x%02X\n",reg,val);
	return 0;
}
static int bq25890_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = bq25890_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("bq25890_read_reg failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	temp &= ~mask;
	temp |= val & mask;

	rc = bq25890_write_reg(client, reg, temp);
	if (rc) {
		pr_err("bq25890_write failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	return 0;
}
static void bq25890_stay_awake(struct bq25890_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons | reason;
	if (reasons != 0 && chip->wake_reasons == 0) {
		pr_debug("staying awake: 0x%02x (bit %d)\n",
				reasons, reason);
		pm_stay_awake(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
}

static void bq25890_relax(struct bq25890_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons & (~reason);
	if (reasons == 0 && chip->wake_reasons != 0) {
		pr_debug("relaxing: 0x%02x (bit %d)\n",
				reasons, reason);
		pm_relax(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
};


/******************************************************************************/
static enum bq25890_vbus_type bq25890_get_vbus_type(struct bq25890_chip *chip)
{
	u8 val = 0;
    int ret;
	ret = bq25890_read_reg(chip->client,BQ25890_REG_0B,&val);
    if (ret < 0) return 0;
	val &= BQ25890_VBUS_STAT_MASK;
	val >>= BQ25890_VBUS_STAT_SHIFT;

	return val;
}
static int bq25890_enable_otg(struct bq25890_chip *chip)
{
    u8 val = BQ25890_OTG_ENABLE << BQ25890_OTG_CONFIG_SHIFT;

	return bq25890_masked_write(chip->client, BQ25890_REG_03, BQ25890_OTG_CONFIG_MASK, val);

}
static int bq25890_disable_otg(struct bq25890_chip *chip)
{
    u8 val = BQ25890_OTG_DISABLE << BQ25890_OTG_CONFIG_SHIFT;

	return bq25890_masked_write(chip->client, BQ25890_REG_03, BQ25890_OTG_CONFIG_MASK, val);

}
static bool bq25890_is_otg_mode(struct bq25890_chip *chip)
{
	u8 temp;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_03, &temp);
	if (ret) {
		pr_err("failed to read OTG enable bits =%d\n", ret);
		return false;
	}

	return !!(temp & BQ25890_OTG_CONFIG_MASK);
}
static int bq25890_adc_start(struct bq25890_chip *chip, bool oneshot)
{
    u8 val;
    int ret;
	val = !!oneshot << BQ25890_CONV_START_SHIFT;

	ret = bq25890_masked_write(chip->client,BQ25890_REG_02,BQ25890_CONV_START_MASK, val);

    if(ret < 0){
        pr_err("failed to write register 0x02:%d\n",ret);
        return ret;
    }

  return ret;
}
static int bq25890_adc_rate(struct bq25890_chip *chip, bool oneshot)
{
	u8 val;
	int ret;
	val = !!oneshot << BQ25890_CONV_RATE_SHIFT;

	ret = bq25890_masked_write(chip->client,BQ25890_REG_02,BQ25890_CONV_RATE_MASK, val);

	if(ret < 0){
	  pr_err("failed to write register 0x02:%d\n",ret);
	  return ret;
	}
	return ret;
}
static int bq25890_adc_read_battery_volt(struct bq25890_chip *chip)
{
    uint8_t val;
    int volt;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_0E, &val);
    if(ret < 0){
        pr_err("read battery voltage failed :%d\n",ret);
        return ret;
    }
    else{
        volt = BQ25890_BATV_BASE + ((val & BQ25890_BATV_MASK) >> BQ25890_BATV_SHIFT) * BQ25890_BATV_LSB ;
        return volt;
    }
}

static int bq25890_adc_read_sys_volt(struct bq25890_chip *chip)
{
    uint8_t val;
    int volt;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_0F,&val);
    if(ret < 0){
        pr_err("read system voltage failed :%d\n",ret);
        return ret;
    }
    else{
        volt = BQ25890_SYSV_BASE + ((val & BQ25890_SYSV_MASK) >> BQ25890_SYSV_SHIFT) * BQ25890_SYSV_LSB ;
        return volt;
    }
}

static int bq25890_adc_read_vbus_volt(struct bq25890_chip *chip)
{
    uint8_t val;
    int volt;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_11, &val);
    if(ret < 0){
        pr_err("read vbus voltage failed :%d\n",ret);
        return ret;
    }
    else{
        volt = BQ25890_VBUSV_BASE + ((val & BQ25890_VBUSV_MASK) >> BQ25890_VBUSV_SHIFT) * BQ25890_VBUSV_LSB ;
        return volt;
    }
}
static int bq25890_adc_read_charge_current(struct bq25890_chip *chip)
{
    uint8_t val;
    int volt;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_12, &val);
    if(ret < 0){
        pr_err("read charge current failed :%d\n",ret);
        return ret;
    }
    else{
        volt = (int)(BQ25890_ICHGR_BASE + ((val & BQ25890_ICHGR_MASK) >> BQ25890_ICHGR_SHIFT) * BQ25890_ICHGR_LSB) ;
        return volt;
    }
}

static int bq25890_set_chargecurrent(struct bq25890_chip *chip,int curr)
{

	u8 ichg;

    ichg = (curr - BQ25890_ICHG_BASE)/BQ25890_ICHG_LSB;
	return bq25890_masked_write(chip->client, BQ25890_REG_04,BQ25890_ICHG_MASK, ichg << BQ25890_ICHG_SHIFT);

}

static int bq25890_set_term_current(struct bq25890_chip *chip,int curr)
{
    u8 iterm;

    iterm = (curr - BQ25890_ITERM_BASE) / BQ25890_ITERM_LSB;

    return bq25890_masked_write(chip->client, BQ25890_REG_05,BQ25890_ITERM_MASK, iterm << BQ25890_ITERM_SHIFT);
}

static int bq25890_set_prechg_current(struct bq25890_chip *chip,int curr)
{
    u8 iprechg;

    iprechg = (curr - BQ25890_IPRECHG_BASE) / BQ25890_IPRECHG_LSB;

    return bq25890_masked_write(chip->client, BQ25890_REG_05,BQ25890_IPRECHG_MASK, iprechg << BQ25890_IPRECHG_SHIFT);
}

static int bq25890_set_chargevoltage(struct bq25890_chip *chip,int volt)
{
	u8 val;
	/* volt = min(volt, 4350); */
    	val = (volt - BQ25890_VREG_BASE)/BQ25890_VREG_LSB;
	return bq25890_masked_write(chip->client, BQ25890_REG_06,BQ25890_VREG_MASK, val << BQ25890_VREG_SHIFT);
}


static int bq25890_set_input_volt_limit(struct bq25890_chip *chip,int volt)
{
	u8 val;
    val = (volt - BQ25890_VINDPM_BASE)/BQ25890_VINDPM_LSB;
    return bq25890_masked_write(chip->client, BQ25890_REG_0D,BQ25890_VINDPM_MASK, val << BQ25890_VINDPM_SHIFT);
}

static int bq25890_set_input_current_limit(struct bq25890_chip *chip,int curr)
{

	u8 val;
	curr = max(curr,BQ25890_IINLIM_BASE);
    val = (curr - BQ25890_IINLIM_BASE)/BQ25890_IINLIM_LSB;
	chip->input_current_ma = val * BQ25890_IINLIM_LSB + BQ25890_IINLIM_BASE;
	return bq25890_masked_write(chip->client, BQ25890_REG_00,BQ25890_IINLIM_MASK, val << BQ25890_IINLIM_SHIFT);
}

static int bq25890_set_vindpm_offset_reg(struct bq25890_chip *chip, int offset)
{
	u8 val;
	val = (offset - BQ25890_VINDPMOS_BASE)/BQ25890_VINDPMOS_LSB;
	return bq25890_masked_write(chip->client, BQ25890_REG_01,BQ25890_VINDPMOS_MASK, val << BQ25890_VINDPMOS_SHIFT);
}

static int bq25890_watchdog_timer_enable(struct bq25890_chip *chip,bool enable)
{
	u8 val = (u8)(!!enable << BQ25890_WDT_SHIFT);
	pr_debug("enable=%d\n",enable);
	return bq25890_masked_write(chip->client, BQ25890_REG_07, BQ25890_WDT_MASK, val);
}
#if 0
static int bq25890_set_watchdog_timer(struct bq25890_chip *chip,u8 timeout)
{
	u8 val = ((timeout - BQ25890_WDT_BASE)/BQ25890_WDT_LSB)<< BQ25890_WDT_SHIFT;

	return bq25890_masked_write(chip->client,BQ25890_REG_07,BQ25890_WDT_MASK, val);
}
#endif
static int bq25890_reset_watchdog_timer(struct bq25890_chip *chip)
{
	u8 val = BQ25890_WDT_RESET << BQ25890_WDT_RESET_SHIFT;

	return bq25890_masked_write(chip->client, BQ25890_REG_03, BQ25890_WDT_RESET_MASK, val);
}

static int bq25890_set_boost_vlim(struct bq25890_chip *chip,int volt)
{
	u8 val;

	if ((volt < BQ25890_BOOSTV_BASE) || (volt > BQ25890_BOOSTV_MAX)) {
		dev_err(chip->dev, "bad  voltage mv =%d asked to set\n", volt);
		return -EINVAL;
	}
	pr_debug("volt=%d\n",volt);

	val = (volt - BQ25890_BOOSTV_BASE) / BQ25890_BOOSTV_LSB;
	val = val<< BQ25890_BOOSTV_SHIFT;

	pr_debug("val=0x%x\n",val);

	return bq25890_masked_write(chip->client, BQ25890_REG_0A, BQ25890_BOOSTV_MASK, val);
}

static int boost_ilim[]={500,750,1200,1400,1650,1875,2150,2450};
static int bq25890_set_boost_ilim(struct bq25890_chip *chip,int curr)
{
	int index;

	curr = max(curr,BQ25890_BOOST_IINLIM_MIN);
	curr = min(curr,BQ25890_BOOST_IINLIM_MAX);

	for(index = ARRAY_SIZE(boost_ilim)-1; index >= 0; index--){
		if(curr >= boost_ilim[index]){
			break;
		}
	}

	pr_debug("curr=%d\n",boost_ilim[index]);
	return bq25890_masked_write(chip->client, BQ25890_REG_0A, BQ25890_BOOST_LIM_MASK, (u8)index);
}

static void bq25890_set_otg(struct bq25890_chip *chip,int enable)
{
    int ret;
	pr_debug("enable=%d\n",enable);
    if(enable){
		bq25890_set_boost_vlim(chip,chip->otg_boost_vlim);
		bq25890_set_boost_ilim(chip,chip->otg_boost_ilim);
		if (chip->usbsel_gpio) {
		   gpio_direction_output(chip->usbsel_gpio,0);
		   pr_debug("usbsel=%d\n",gpio_get_value(chip->usbsel_gpio));
		}
		bq25890_watchdog_timer_enable(chip,false);
        ret = bq25890_enable_otg(chip);
        if(ret < 0){
            pr_err("Failed to enable otg-%d\n",ret);
            return;
        }
		//cancel_delayed_work_sync(&chip->otg_monitor_work);
		//schedule_delayed_work(&chip->otg_monitor_work,msecs_to_jiffies(0));
    }
    else{
		if (chip->usbsel_gpio) {
			gpio_direction_output(chip->usbsel_gpio,1);
			pr_debug("usbsel=%d\n",gpio_get_value(chip->usbsel_gpio));
		}
		bq25890_watchdog_timer_enable(chip,true);
        ret = bq25890_disable_otg(chip);
        if(ret < 0){
            pr_err("Failed to disable otg-%d\n",ret);
        }
		//cancel_delayed_work_sync(&chip->otg_monitor_work);
	}
}
static int bq25890_reset_chip(struct bq25890_chip *chip)
{
    int ret;
	u8 val = BQ25890_RESET << BQ25890_RESET_SHIFT;
	pr_debug("val=%d\n",val);

	ret = bq25890_masked_write(chip->client, BQ25890_REG_14, BQ25890_RESET_MASK, val);
    return ret;
}
static int bq25890_force_ico(struct bq25890_chip *chip)
{
    u8 val;
    int ret;

    val = BQ25890_FORCE_ICO << BQ25890_FORCE_ICO_SHIFT;

    ret = bq25890_masked_write(chip->client, BQ25890_REG_09, BQ25890_FORCE_ICO_MASK, val);

    return ret;
}
static int bq25890_check_force_ico_done(struct bq25890_chip *chip)
{
    u8 val;
    int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_14, &val);
    if(ret) return ret;

    if(val & BQ25890_ICO_OPTIMIZED_MASK)
        return 1;
    else
        return 0;
}

static int bq25890_read_idpm_limit(struct bq25890_chip *chip)
{
    uint8_t val;
    int curr;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_13, &val);
    if(ret < 0){
        pr_err("read vbus voltage failed :%d\n",ret);
        return ret;
    }
    else{
        curr = BQ25890_IDPM_LIM_BASE + ((val & BQ25890_IDPM_LIM_MASK) >> BQ25890_IDPM_LIM_SHIFT) * BQ25890_IDPM_LIM_LSB ;
        return curr;
    }
}
static int bq25890_read_iinlimt(struct bq25890_chip *chip)
{
    uint8_t val;
    int curr;
    int ret;
	ret = bq25890_read_reg(chip->client, BQ25890_REG_00, &val);
    if(ret < 0){
        pr_err("read iinlimt failed :%d\n",ret);
        return ret;
    }
    else{
        curr = BQ25890_IINLIM_BASE + ((val & BQ25890_IINLIM_MASK) >> BQ25890_IINLIM_SHIFT) * BQ25890_IINLIM_LSB;
        return curr;
    }
}

#if 0

static bool bq25890_is_charge_done(struct bq25890_chip *chip)
{
    int ret;
    u8 val;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &val);
    if(ret < 0){
        pr_err("%s:read REG0B failed :%d\n",__func__,ret);
        return false;
    }
    val &= BQ25890_CHRG_STAT_MASK;
    val >>= BQ25890_CHRG_STAT_SHIFT;

    return(val == BQ25890_CHRG_STAT_CHGDONE);
}
#endif
/*******************************************************************************/
static struct power_supply *get_parallel_psy(struct bq25890_chip *chip)
{
	if (!chip->parallel.avail)
		return NULL;
	if (chip->parallel.psy)
		return chip->parallel.psy;
	chip->parallel.psy = power_supply_get_by_name("usb-parallel");
	if (!chip->parallel.psy)
		pr_debug("parallel charger not found\n");
	return chip->parallel.psy;
}
static int set_property_on_parallel(struct bq25890_chip *chip,
		enum power_supply_property prop, int val)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *parallel_psy = get_parallel_psy(chip);

	if (!parallel_psy) {
		pr_debug("no parallel psy found\n");
		return -EINVAL;
	}
	pr_debug("val=%d\n",val);
	ret.intval = val;
	power_supply_set_property(parallel_psy, prop, &ret);
	if (rc)
		pr_debug("parallel psy does not allow updating prop %d rc = %d\n",
			prop, rc);

	return rc;
}

static int get_property_from_parallel(struct bq25890_chip *chip,
		enum power_supply_property prop, int *val)
{
	int rc;
	union power_supply_propval ret = {0, };

	struct power_supply *parallel_psy = get_parallel_psy(chip);

	if (!parallel_psy) {
		pr_debug("no parallel psy found\n");
		return -EINVAL;
	}

	rc = power_supply_get_property(parallel_psy, prop, &ret);
	if (rc) {
		pr_debug(
			"parallel psy doesn't support reading prop %d rc = %d\n",
			prop, rc);
		return rc;
	}

	*val = ret.intval;
	return rc;
}

#define SUSPEND_CURRENT_MA	2
static int get_usb_supply_type(struct bq25890_chip *chip)
{
	int rc, usb_supply_type;
	u8 reg = 0;
	bq25890_get_vbus_type(chip);
	rc = bq25890_read_reg(chip->client, BQ25890_REG_0B, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		usb_supply_type = POWER_SUPPLY_TYPE_UNKNOWN;
	}
	reg = (reg & BQ25890_VBUS_STAT_MASK) >> BQ25890_VBUS_STAT_SHIFT;
	chip->vbus_type = reg;
	pr_debug("reg0x%02X=0x%02X\n",BQ25890_REG_0B,reg);
	switch (reg) {
	case BQ25890_VBUS_USB_SDP:
		usb_supply_type = POWER_SUPPLY_TYPE_USB;
		break;
	case BQ25890_VBUS_USB_CDP:
		usb_supply_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case BQ25890_VBUS_USB_DCP:
	case BQ25890_VBUS_HVDCP:
	case BQ25890_VBUS_NONSTAND:
		usb_supply_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case BQ25890_VBUS_UNKNOWN:
		usb_supply_type = POWER_SUPPLY_TYPE_USB_FLOAT;//POWER_SUPPLY_TYPE_USB;
		break;
	case BQ25890_VBUS_NONE:
		usb_supply_type = POWER_SUPPLY_TYPE_UNKNOWN;//POWER_SUPPLY_TYPE_USB;
		break;
	default:
		usb_supply_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}
	return usb_supply_type;
}
static void bq25890_dump_regs(struct bq25890_chip *chip)
{
	int rc;
	u8 temp = 0,reg = 0;
	uint8_t reg_fault = 0, reg_status = 0, reg_dpm = 0;
	for (reg = 0;reg <= 0x14;reg++) {
		rc = bq25890_read_reg(chip->client, reg, &temp);
		pr_debug("reg=0x%02X,value=0x%02X\n",reg,temp);
		if (reg == BQ25890_REG_0B) {
			reg_status = temp;
		} else if (reg == BQ25890_REG_0C) {
			reg_fault = temp;
		} else if (reg == BQ25890_REG_13) {
			reg_dpm = temp;
		}
	}
	//fuelsummary_collect_value(ID_SIC_FAULT, reg_fault);
	//fuelsummary_collect_value(ID_SIC_NPG, ((reg_status & 0x04) != 0x04));
	//fuelsummary_collect_value(ID_SIC_DPM, ((reg_dpm & 0x80) == 0x80));
}
#define ADC_TO_VBUS_SCALE	1
static int bq25890_get_usbid_voltage(struct bq25890_chip *chip)
{
	/*int rc = 0;
	int usbid_uv = 0;
	
	struct qpnp_vadc_result results;
	rc = qpnp_vadc_read(chip->vadc_dev, P_MUX2_1_1, &results);
	if (rc) {
		//chip->vbus_vol = 0;
		pr_err("Unable to read vbus rc=%d\n", rc);
		return 0;
	}
	pr_debug("usbid value:%lld, usbid voltage:%lld uv\n", results.physical, results.physical * ADC_TO_VBUS_SCALE);
	usbid_uv = results.physical * ADC_TO_VBUS_SCALE;
	*/
	return 0;
}

static int bq25890_get_hvdcp_type(struct bq25890_chip *chip)
{
	int vbus_mv,hvdcp_type;
	int index;

	hvdcp_type = HVDCP_TYPE_UNKOWN;
	vbus_mv = bq25890_adc_read_vbus_volt(chip);
	if (vbus_mv < 0){
		pr_err("read vbus err rc=%d\n",vbus_mv);
		return 0;
	}

	for (index = ARRAY_SIZE(hvdcp_vol)-1;index >= 0 ;index--) {
		if (is_between(vbus_mv,hvdcp_vol[index].min_mv,hvdcp_vol[index].max_mv)) {
			pr_debug("vbus_mv=%d min_mv=%d,max_mv=%d\n",
					vbus_mv,hvdcp_vol[index].min_mv,hvdcp_vol[index].max_mv);
			hvdcp_type = hvdcp_vol[index].type;
		}
	}
	return hvdcp_type;
}
static int bq25890_get_prop_charge_type(struct bq25890_chip *chip)
{
	int chg_type = 0;
	int ret = 0;
	u8 temp;

    ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &temp);
    if(ret < 0){
        pr_err("%s Failed to read register 0x0b:%d\n",__func__,ret);
        return 0;
    }

    temp &= BQ25890_CHRG_STAT_MASK;
    temp >>= BQ25890_CHRG_STAT_SHIFT;

	if (temp == BQ25890_CHRG_STAT_FASTCHG)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (temp == BQ25890_CHRG_STAT_PRECHG)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

     if (chip->chg_type != chg_type) {
		if (chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE) {
			pr_info("Charging stopped.\n");
		} else{
			pr_info("Charging started.\n");
 			chip->charge_begin = jiffies;
		}
	}

    chip->chg_type = chg_type;
    return chg_type;
}
#define IRCOMP_R_MIN_MOHM  0
#define IRCOMP_R_MAX_MOHM  140
static int bq25890_set_ir_comp_resister(struct bq25890_chip *chip, int mohm)
{
	u8 val;
	if (mohm < IRCOMP_R_MIN_MOHM
			|| mohm > IRCOMP_R_MAX_MOHM) {
		pr_err("bad r=%d asked to set\n", mohm);
		return -EINVAL;
	}

    val = (mohm - BQ25890_BAT_COMP_BASE)/BQ25890_BAT_COMP_LSB;
	return bq25890_masked_write(chip->client, BQ25890_REG_08,BQ25890_BAT_COMP_MASK, val << BQ25890_BAT_COMP_SHIFT);
}
#define IRCOMP_VCLAMP_MIN_MV  0
#define IRCOMP_VCLAMP_MAX_MV  224
static int bq25890_set_vclamp_mv(struct bq25890_chip *chip, int mv)
{
	u8 val;
	if (mv < IRCOMP_VCLAMP_MIN_MV
			|| mv > IRCOMP_VCLAMP_MAX_MV) {
		pr_err("bad v=%d asked to set\n", mv);
		return -EINVAL;
	}

    val = (mv - BQ25890_VCLAMP_BASE)/BQ25890_VCLAMP_LSB;
	return bq25890_masked_write(chip->client, BQ25890_REG_08,BQ25890_VCLAMP_MASK, val << BQ25890_VCLAMP_SHIFT);
}
static bool bq25890_is_hvdcp(struct bq25890_chip *chip)
{
	u8 val;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &val);
	if (ret) {
		pr_err("failed to read vbus stat rc=%d\n", ret);
		return false;
	}
	val &= BQ25890_VBUS_STAT_MASK;
    val >>= BQ25890_VBUS_STAT_SHIFT;
    return(val == BQ25890_VBUS_HVDCP_VALUE);
}
static int bq25890_hvdcp_enable(struct bq25890_chip *chip,bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << BQ25890_HVDCPEN_SHIFT);

	pr_debug("enable=%d\n", enable);

	ret = bq25890_masked_write(chip->client, BQ25890_REG_02,
				BQ25890_HVDCPEN_MASK, val);
	if (ret) {
		pr_err("failed to enable hvdcp rc=%d\n", ret);
		return ret;
	}

	return 0;
}
static int bq25890_maxc_enable(struct bq25890_chip *chip,bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << BQ25890_MAXCEN_SHIFT);

	pr_debug("enable=%d\n", enable);

	ret = bq25890_masked_write(chip->client, BQ25890_REG_02,
				BQ25890_MAXCEN_MASK, val);
	if (ret) {
		pr_err("failed to enable maxc rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq25890_pumpx_enable(struct bq25890_chip *chip,bool enable)
{
    int ret;
	u8 val = (u8)(!!enable << BQ25890_EN_PUMPX_SHIFT);

    ret = bq25890_masked_write(chip->client, BQ25890_REG_04, BQ25890_EN_PUMPX_MASK, val);
	if (ret) {
		pr_err("failed to enable pumpx rc=%d\n", ret);
		return ret;
	}
	return 0;
}
static int bq25890_termination_enable(struct bq25890_chip *chip,bool enable)
{
    int ret;
	u8 val = (u8)(!!enable << BQ25890_EN_TERM_SHIFT);

    ret = bq25890_masked_write(chip->client, BQ25890_REG_07, BQ25890_EN_TERM_MASK, val);
	if (ret) {
		pr_err("failed to set termination rc=%d\n", ret);
		return ret;
	}
	return 0;
}
static bool bq25890_is_vindpm(struct bq25890_chip *chip)
{
	u8 val;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_13, &val);
	if (ret) {
		pr_err("failed to read vbus stat rc=%d\n", ret);
		return false;
	}
	val &= BQ25890_VDPM_STAT_MASK;
    val >>= BQ25890_VDPM_STAT_SHIFT;

	return(val == BQ25890_VDPM_STAT_VALUE);

}
static bool bq25890_is_iindpm(struct bq25890_chip *chip)
{
	u8 val;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_13, &val);
	if (ret) {
		pr_err("failed to read vbus stat rc=%d\n", ret);
		return false;
	}

	val &= BQ25890_IDPM_STAT_MASK;
    val >>= BQ25890_IDPM_STAT_SHIFT;

	return(val == BQ25890_IDPM_STAT_VALUE);
}
static bool bq25890_absolue_vindpm_enable(struct bq25890_chip *chip,bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << BQ25890_FORCE_VINDPM_SHIFT);

	ret = bq25890_masked_write(chip->client, BQ25890_REG_0D,BQ25890_FORCE_VINDPM_MASK,val);
	if (ret) {
		pr_err("failed to enable ablsolue vindpm rc=%d\n", ret);
		return ret;
	}
	return 0;
}
static bool bq25890_is_vbus_attached(struct bq25890_chip *chip)
{
	u8 val;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_11, &val);
	if (ret) {
		pr_err("fail to read reg=%d,ret=%d\n",BQ25890_REG_11,ret);
		return ret;
	}

	val &= BQ25890_VBUS_GD_MASK;
	val >>= BQ25890_VBUS_GD_SHIFT;

	return(val == BQ25890_VBUS_GD_VALUE);
}

static bool bq25890_is_charger_present(struct bq25890_chip *chip)
{
	u8 val;
	int ret;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &val);
	if (ret) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", ret);
		return false;
	}
	val &= BQ25890_PG_STAT_MASK;
	val >>= BQ25890_PG_STAT_SHIFT;

	return(val == BQ25890_PG_STAT_VALUE);
}

#define EN_HIZ_SHIFT 7
static int bq25890_enable_hiz(struct bq25890_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << BQ25890_ENHIZ_SHIFT);

	pr_debug("enable=%d\n", enable);

	ret = bq25890_masked_write(chip->client, BQ25890_REG_00,
				BQ25890_ENHIZ_MASK, val);
	if (ret) {
		pr_err("failed to set HIZ rc=%d\n", ret);
		return ret;
	}
	chip->vbus_hized = enable;

	return 0;
}
static bool bq25890_is_hiz(struct bq25890_chip *chip)
{
	int ret;
	u8 val;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_00,&val);
	if (ret) {
		pr_err("failed to read HIZ rc=%d\n", ret);
		return false;
	}

	val &= BQ25890_ENHIZ_MASK;
	val >>= BQ25890_ENHIZ_SHIFT;

	return(val == BQ25890_ENHIZ_VALUE);
}
#define DISABLE_CHARGING_CURRENT_MA	 128
#define CHG_ENABLE_SHIFT  4
static int bq25890_enable_charging(struct bq25890_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << BQ25890_CHG_CONFIG_SHIFT);

	pr_debug("enable=%d\n", enable);
	ret = bq25890_masked_write(chip->client, BQ25890_REG_03,
						BQ25890_CHG_CONFIG_MASK, val);
	if (ret) {
		pr_err("failed to set EN_CHG rc=%d\n", ret);
		return ret;
	}
#if 0
	if (chip->primary_charger) {
		ret = bq25890_masked_write(chip->client, BQ25890_REG_03,
							BQ25890_CHG_CONFIG_MASK, val);
		if (ret) {
			pr_err("failed to set EN_CHG rc=%d\n", ret);
			return ret;
		}
	} else {
		if (!enable) {
			pr_debug("disable parallel charging set fastchg 128ma\n");
			bq25890_set_chargecurrent(chip,DISABLE_CHARGING_CURRENT_MA);
		} else {
			pr_debug("enable parallel charging do nothing\n");
		}
	}
#endif
	return 0;
}

static int bq25890_is_chg_enabled(struct bq25890_chip *chip)
{
	int ret;
	u8 temp;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_03, &temp);
	if (ret) {
		pr_err("failed to read BQ25890_REG_03 rc=%d\n", ret);
		return 0;
	}

	return (temp & BQ25890_CHG_CONFIG_MASK) == 0x10;
}
static int bq25890_force_dpdm(struct bq25890_chip *chip)
{
	int ret;
	u8 val = BQ25890_FORCE_DPDM << BQ25890_FORCE_DPDM_SHIFT;

	ret = bq25890_masked_write(chip->client, BQ25890_REG_02,
								BQ25890_FORCE_DPDM_MASK, val);
	if (ret) {
		pr_err("failed to force dpdm rc=%d\n", ret);
		return ret;
	}
	return 0;
}
static bool bq25890_check_force_dpdm_done(struct bq25890_chip *chip)
{
	int ret;
	u8 temp;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_02,&temp);
	if (ret) {
		pr_err("failed to read force dpdm rc=%d\n", ret);
		return false;//error,not done
	}

	if (!(temp & BQ25890_FORCE_DPDM_MASK))
		return true;//done

	return false;//in progress
}

static int bq25890_get_min_parallel_current_ma(struct bq25890_chip *chip)
{
	int hvdcp_type;

	hvdcp_type = bq25890_get_hvdcp_type(chip);

	if ((hvdcp_type == HVDCP_TYPE_9V) || (hvdcp_type == HVDCP_TYPE_12V))
		return chip->parallel.min_9v_current_thr_ma;
	return chip->parallel.min_current_thr_ma;
}
static int bq25890_set_vindpm_absolute(struct bq25890_chip *chip, int vindpm_mv)
{
	int rc = 0;

	if (!chip->use_absolute_vindpm){
		pr_debug("not configuring  absolute vindpm\n");
		return 0;
	}

	/* If the requested vindpm is same, do not configure it again */
	if (vindpm_mv == chip->vindpm_mv) {
		pr_debug("set the same value=%d,skipping\n",vindpm_mv);
		return 0;
	}

	bq25890_absolue_vindpm_enable(chip,true);
	rc = bq25890_set_input_volt_limit(chip, vindpm_mv);
	if (rc) {
		pr_err("configuring vindpm fail,rc=%d\n",rc);
		return rc;
	}
	pr_debug("vindpm set to=%d\n",vindpm_mv);
	chip->vindpm_mv = vindpm_mv;

	//bq25890_absolue_vindpm_enable(chip,true);
	return 0;

}
static int bq25890_set_vindpm_offset(struct bq25890_chip *chip, int offset_mv)
{
	int rc;

	/* If the requested vindpm is same, do not configure it again */
	if (offset_mv == chip->vindpm_offset_mv) {
		pr_debug("set the same value=%d,skipping\n",offset_mv);
		//return 0;
	}

	bq25890_absolue_vindpm_enable(chip,false);
	rc = bq25890_set_vindpm_offset_reg(chip,offset_mv);
	if(rc < 0){
		dev_err(chip->dev,"Failed to set vindpm offset:%d\n",rc);
		return rc;
	}
	pr_debug("vindpm offset to=%d\n",offset_mv);
	chip->vindpm_offset_mv = offset_mv;
	return 0;
}

static int bq25890_set_fastchg_current(struct bq25890_chip *chip,
							int current_ma)
{
	int rc = 0;

	if (current_ma > chip->fastchg_current_max_ma) {
		pr_debug("set fastchg=%d more than max=%d\n",
		current_ma,chip->fastchg_current_max_ma);
		current_ma = chip->fastchg_current_max_ma;
	}

	/* If the requested FCC is same, do not configure it again */
	if (current_ma == chip->fastchg_current_ma) {
		pr_debug("set the same value=%d,skipping\n",current_ma);
		return 0;
	}

	rc = bq25890_set_chargecurrent(chip, current_ma);
	pr_debug("fastchg_current_ma=%d\n",current_ma);
	chip->fastchg_current_ma = current_ma;
	return 0;
}
#define PARALLEL_REENABLE_TIMER_MS	100
#define PARALLEL_ENABLE_FASTCHG_THRE_MA	1000
#define PARALLEL_ENABLE_INPUT_THRE_MA	1000
#define INPUT_CURRENT_MAX_MA			2000
#define DEFAULT_FASTCHG_CURRENT_MA		500
#define DEFAULT_INPUT_CURRENT_MA		500

static bool bq25890_is_parallel_usb_ok(struct bq25890_chip *chip)
{
	int min_current_thr_ma,type;
	ktime_t kt_since_last_disable;

	if (!bq25890_parallel_en) {
		pr_debug( "Parallel charging not enabled\n");
		return false;
	}

	kt_since_last_disable = ktime_sub(ktime_get_boottime(),
					chip->parallel.last_disabled);
	if (chip->parallel.enabled && ktime_to_ms(kt_since_last_disable)
					< PARALLEL_REENABLE_TIMER_MS) {
		pr_debug( "Only been %lld since disable, skipping\n",
				ktime_to_ms(kt_since_last_disable));
		return false;
	}

	if (bq25890_get_prop_charge_type(chip) != POWER_SUPPLY_CHARGE_TYPE_FAST) {
		pr_debug( "Not in fast charge, skipping\n");
		return false;
	}
	/*check batt health*/

	type = get_usb_supply_type(chip);
	if (type != POWER_SUPPLY_TYPE_USB_DCP) {
		pr_debug( "not dcp adapter, skipping\n");
		return false;
	}

	min_current_thr_ma = bq25890_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_debug( "parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		return false;
	}
	if (chip->ico_ma < min_current_thr_ma) {
		pr_debug("ico_ma too small skip enable: %d < %d\n",
			chip->ico_ma, min_current_thr_ma);
		return false;
	}
	if (chip->total_input_current_ma < PARALLEL_ENABLE_INPUT_THRE_MA) {
		pr_debug( "total input current less than input threshold %d\n",
			PARALLEL_ENABLE_INPUT_THRE_MA);
		return false;
	}

	if (chip->total_fastchg_current_ma < PARALLEL_ENABLE_FASTCHG_THRE_MA) {
		pr_debug( "total fastchg current less than fastchg threshold %d\n",
			PARALLEL_ENABLE_FASTCHG_THRE_MA);
		return false;
	}
	return true;
}

#if 0
/*adjust the usb max current based cpu temp*/
static int  bq25890_calc_thermal_limited_current(struct bq25890_chip *chip,
						int current_ma)
{
#if 0

	int therm_ma;

	if (chip->therm_lvl_sel > 0
			&& chip->therm_lvl_sel < (chip->thermal_levels - 1)) {
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = (int)chip->thermal_mitigation[chip->therm_lvl_sel];
		if (therm_ma < current_ma) {
			pr_debug(
				"Limiting current due to thermal: %d mA",
				therm_ma);
			return therm_ma;
		}
	}
#endif
	return current_ma;
}
#endif
static int bq25890_parallel_enable_charging(struct bq25890_chip *chip,
						bool enable);
static int bq25890_set_usb_current_max(struct bq25890_chip *chip,int current_ma)
{
	int rc;
#if 1
	if (current_ma == chip->input_current_ma) {
		pr_debug("set the same value=%d,skipping\n",current_ma);
		return 0;
	}
#endif
	if (current_ma <= SUSPEND_CURRENT_MA) {
		bq25890_enable_charging(chip, false);
		bq25890_enable_hiz(chip, true);
		//chip->input_current_ma = current_ma;
		chip->input_current_ma = 0;
		pr_debug("bq25890 suspend\n");
		return 0;
	}

	bq25890_enable_hiz(chip, false);
	rc = bq25890_set_input_current_limit(chip, current_ma);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	bq25890_parallel_enable_charging(chip, true);
	//chip->input_current_ma = current_ma;
	pr_debug("input_current_ma=%d\n",chip->input_current_ma);

	return rc;
}
static bool bq25890_parallel_is_enabled(struct bq25890_chip *chip);
static int bq25890_adb_set_fastchg_current(struct bq25890_chip *chip)
{
	int usb_supply_type;
	int parallel_enable = 0;
	int parallel_status;

	pr_warn("### adb_set_current_enable : %d ###\n",chip->adb_set_current_enable);

	usb_supply_type = get_usb_supply_type(chip);
	if(usb_supply_type == POWER_SUPPLY_TYPE_USB_DCP
		&& chip->adb_set_current_enable){

		get_property_from_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,&parallel_enable);

		if(!parallel_enable){
			set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,true);
		}

		if(chip->adb_first_input_current)
			bq25890_set_usb_current_max(chip, chip->adb_first_input_current);

		if(chip->adb_second_input_current)
			set_property_on_parallel(chip,POWER_SUPPLY_PROP_CURRENT_MAX,
									chip->adb_second_input_current);

		if(chip->adb_set_current_enable == 1){
			chip->adb_index = 0;
			chip->adb_timer = 0;
		}else{
			if(chip->adb_timer++ >= 60){
				chip->adb_timer = 0;
				chip->adb_index++;
			}
			if(chip->adb_index == 6)
				chip->adb_index = 0;
		}

		bq25890_set_fastchg_current(chip,chip->adb_first_ibat_current[chip->adb_index]);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
								chip->adb_second_ibat_current[chip->adb_index]);
		pr_warn("### index : %d, ibat : first %d, second %d ### \n",chip->adb_index,chip->adb_first_ibat_current[chip->adb_index],chip->adb_second_ibat_current[chip->adb_index]);
		pr_warn("### adb set current ### \n");
		pr_warn("### input : first %d, second %d ### \n",chip->adb_first_input_current,chip->adb_second_input_current);
		//get dump-parallel-reg
		get_property_from_parallel(chip,POWER_SUPPLY_PROP_STATUS,&parallel_status);
		return 1;
	}
	pr_warn("### no adb set current ### \n");
	return 0;
}
#if 1
static int bq25890_adjust_dpm_limit(struct bq25890_chip *chip)
{
	int ret;
	int vbat_uv,mv,i;
	int vbat_uv_sum = 0;
	union power_supply_propval volt_now = {0,};

	//adjust dpm ,according to battery voltage
	if(chip->adjust_dpm_enable){
		for(i=0; i<3; i++){
			power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt_now);
			vbat_uv_sum += volt_now.intval;
		}
		vbat_uv = vbat_uv_sum/3;

		pr_err("### volt_now = %d ###\n",vbat_uv);

		if(vbat_uv < 4100000){
			if(!chip->adjust_dpm_flag)
				mv = 4400;
			else{
				if(vbat_uv < 3900000){
					mv = 4400;
					chip->adjust_dpm_flag = 0;
				}
				else
					mv = 4500;
			}
		}else{
			mv = 4500;
			chip->adjust_dpm_flag = 1;
		}
	}else{
		mv = 4500;
	}

	ret = bq25890_set_vindpm_absolute(chip,mv);
	if(ret < 0){
		dev_err(chip->dev,"Failed to set vindpm absolute:%d\n",ret);
		return ret;
	}

	return 0;
}
#endif
static int bq25890_set_fastchg_current_for_normal_temp(struct bq25890_chip *chip,
							int enable)
{
	int rc;

	if(enable){
		if(chip->nor_timer++ >= chip->nor_timer_limit){
			chip->nor_timer = 0;
			chip->nor_index++;
		}
		if(chip->nor_index == 2)
			chip->nor_index = 1;

		chip->cfg_fastchg_current_ma = chip->nor_first_ibat_current[chip->nor_index];
		chip->parallel.fastchg_current_ma = chip->nor_second_ibat_current[chip->nor_index];
		pr_warn("### nor index : %d, ibat : first %d, second %d ### \n",chip->nor_index,chip->nor_first_ibat_current[chip->nor_index],chip->nor_second_ibat_current[chip->nor_index]);
		rc = 1;
	}else{
		chip->nor_timer = 0;
		chip->nor_index = 0;
		rc = 0;
	}
	return rc;
}
static int bq25890_set_fastchg_current_for_switch(struct bq25890_chip *chip,
							int enable)
{
	int rc;

	if(enable){
		if(chip->sw_timer++ >= chip->sw_timer_limit){
			chip->sw_timer = 0;
			chip->sw_index++;
		}
		if(chip->sw_index == 2)
			chip->sw_index = 1;

		chip->cfg_fastchg_current_ma = chip->sw_first_ibat_current[chip->sw_index];
		chip->parallel.fastchg_current_ma = chip->sw_second_ibat_current[chip->sw_index];
		pr_warn("### sw index : %d, ibat : first %d, second %d ### \n",chip->sw_index,chip->sw_first_ibat_current[chip->sw_index],chip->sw_second_ibat_current[chip->sw_index]);
		rc = 1;
	}else{
		chip->sw_timer = 0;
		chip->sw_index = 0;
		rc = 0;
	}
	return rc;
}

static unsigned int is_atboot;
#define CFG_FASTCHG_THRE1_MA 	2048
#define CFG_FASTCHG_THRE2_MA 	1024
static int bq25890_set_fastchg_current_user(struct bq25890_chip *chip,
							int current_ma)
{
	bool parallel_enable;
	int total_fastchg_current_ma;
	int total_max_ma;
	int fastchg_acc_ma;
	int nor_flag = 0;
	int sw_flag = 0;
	int fb_flag = 0;
	int calling_flag = 0;

	if (chip->status & BQ25890_STATUS_ATTACHED && !(chip->status & BQ25890_STATUS_PG)) {
		pr_debug("fastchg configed before type detected\n");
		return 0;
	}
	if (chip->pd_state != POWER_DETECT_STATE_DONE) {
		pr_debug("power detected not finished\n");
		return 0;
	}

	if(bq25890_adb_set_fastchg_current(chip))
		return 1;

	fastchg_acc_ma = chip->batt_1C_current_ma * chip->batt_core_acc / 100;
	if (current_ma == fastchg_acc_ma) {
		current_ma = fastchg_acc_ma - (fastchg_acc_ma * chip->fastchg_acc / 100);
		pr_debug("adjust %dma->%dma based on fastchg accuracy\n",
			fastchg_acc_ma,current_ma);
		if(chip->nor_set_current_enable)
			nor_flag = bq25890_set_fastchg_current_for_normal_temp(chip,1);
	}else{
		nor_flag = bq25890_set_fastchg_current_for_normal_temp(chip,0);
	}

	if(chip->sw_set_current_enable && chip->batt_1C_current_ma * 60 / 100 == current_ma){
		sw_flag = bq25890_set_fastchg_current_for_switch(chip,1);
	}else{
		sw_flag = bq25890_set_fastchg_current_for_switch(chip,0);
	}

	if(chip->fbon_set_current_enable && chip->batt_1C_current_ma * 70 / 100 == current_ma){
		chip->cfg_fastchg_current_ma = chip->fbon_first_ibat_current[0];
		chip->parallel.fastchg_current_ma = chip->fbon_second_ibat_current[0];
		fb_flag = 1;
	}else{
		fb_flag = 0;
	}

	if(chip->calling_set_current_enable && chip->batt_1C_current_ma * 40 / 100 == current_ma){
		chip->cfg_fastchg_current_ma = chip->calling_first_ibat_current[0];
		chip->parallel.fastchg_current_ma = chip->calling_second_ibat_current[0];
		calling_flag = 1;
	}else{
		calling_flag = 0;
	}

	total_max_ma = chip->fastchg_current_max_ma + chip->parallel.fastchg_current_max_ma;
	if (current_ma > total_max_ma) {
		current_ma = total_max_ma;
	}
	pr_debug("current_ma=%d last_total_fastchg_ma=%d\n",current_ma,chip->total_fastchg_current_ma);

	if (current_ma == 0) {
		pr_debug("disable charging %d\n", current_ma);
		bq25890_enable_charging(chip,0);
		power_supply_changed(chip->batt_psy);
	}

	if (!chip->total_fastchg_current_ma && current_ma > 0) {
		pr_debug("enable charging %d\n", current_ma);
		bq25890_enable_charging(chip,1);
		power_supply_changed(chip->batt_psy);
	}

	mutex_lock(&chip->parallel.lock);
	total_fastchg_current_ma = current_ma;
	chip->total_fastchg_current_ma = current_ma;

	parallel_enable = bq25890_is_parallel_usb_ok(chip);

if(!calling_flag && !fb_flag && !nor_flag && !sw_flag){
	/*total_fastchg_current_ma >= PARALLEL_ENABLE_FASTCHG_THRE_MA*/
	if (total_fastchg_current_ma >= (CFG_FASTCHG_THRE1_MA+64)) {
		chip->cfg_fastchg_current_ma = CFG_FASTCHG_THRE1_MA;
		chip->parallel.fastchg_current_ma = total_fastchg_current_ma - CFG_FASTCHG_THRE1_MA;
	} else if (total_fastchg_current_ma >= (CFG_FASTCHG_THRE2_MA+64)) {
		chip->cfg_fastchg_current_ma = CFG_FASTCHG_THRE2_MA;
		chip->parallel.fastchg_current_ma = total_fastchg_current_ma - CFG_FASTCHG_THRE2_MA;
	} else {
		chip->cfg_fastchg_current_ma = total_fastchg_current_ma;
		chip->parallel.fastchg_current_ma = 0;
	}
}

	if(is_atboot){
		chip->cfg_fastchg_current_ma = 1024;
		chip->parallel.fastchg_current_ma = 1024;
		parallel_enable = true;
		pr_debug("In AT mode,set ibat current: 1a+1a \n");
	}

	pr_debug("enable=%d total =%d cfg_fastchg_current=%d parallel.fastchg_current=%d\n",
		parallel_enable,total_fastchg_current_ma,chip->cfg_fastchg_current_ma,chip->parallel.fastchg_current_ma);

	bq25890_set_fastchg_current(chip,chip->cfg_fastchg_current_ma);

	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
								chip->parallel.fastchg_current_ma);

	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,parallel_enable);

	chip->parallel_chgr_enabled = parallel_enable;

	mutex_unlock(&chip->parallel.lock);
	return 0;
}

static int bq25890_set_input_current_user(struct bq25890_chip *chip,
							int current_ma)
{
	bool parallel_enable;
	int total_input_current_ma;
	pr_debug("current_ma=%d,ico_ma=%d\n",current_ma,chip->ico_ma);
	if (chip->status & BQ25890_STATUS_ATTACHED && !(chip->status & BQ25890_STATUS_PG)) {
		pr_debug("fastchg configed before type detected\n");
		return 0;
	}

	if (chip->pd_state != POWER_DETECT_STATE_DONE) {
		pr_debug("power detected not finished\n");
		return 0;
	}

	//current_ma = bq25890_calc_thermal_limited_current(chip, current_ma);
	current_ma = min(current_ma,chip->ico_ma);

	mutex_lock(&chip->parallel.lock);
	total_input_current_ma = current_ma;
	chip->total_input_current_ma = current_ma;
	parallel_enable = bq25890_is_parallel_usb_ok(chip);

	if (total_input_current_ma >= PARALLEL_ENABLE_INPUT_THRE_MA) {
		chip->cfg_input_current_ma = total_input_current_ma;
		chip->parallel.input_current_ma = PARALLEL_ENABLE_INPUT_THRE_MA;

	} else {
		chip->cfg_input_current_ma = total_input_current_ma;
		chip->parallel.input_current_ma = 0;
	}
	pr_debug("enable=%d total =%d cfg_input_current_ma=%d parallel.input_current_ma=%d\n",
		parallel_enable,total_input_current_ma,chip->cfg_input_current_ma,chip->parallel.input_current_ma);

	bq25890_set_usb_current_max(chip, chip->cfg_input_current_ma);

	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CURRENT_MAX,
								chip->parallel.input_current_ma);
	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,parallel_enable);
	chip->parallel_chgr_enabled = parallel_enable;
	mutex_unlock(&chip->parallel.lock);
	return 0;
}
static int bq25890_system_temp_level_set(struct bq25890_chip *chip,
								int lvl_sel)
{
	//int input_current_ma;
	pr_debug("lvl_sel=%d\n",lvl_sel);
	//check lvl_sel and change chip->therm level here

	bq25890_set_input_current_user(chip,chip->total_input_current_ma);
	return 0;
}

static bool bq25890_power_ico_trigered(struct bq25890_chip *chip)
{
	int ico_ma;
	bool ico_done,is_vindpm,is_iindpm;

	is_vindpm = bq25890_is_vindpm(chip);
	is_iindpm = bq25890_is_iindpm(chip);
	ico_done = bq25890_check_force_ico_done(chip);

	pr_debug("is_vindpm=%d is_iindpm=%d ico_done=%d\n",
			is_vindpm,is_iindpm,ico_done);

	if (!ico_done) {
		pr_debug("ico done not trigger,use cfg input current=%d\n",chip->cfg_input_current_ma);
		return false;
	}

	if (ico_done && (is_vindpm || is_iindpm)) {
		pr_debug("input current configged reach max ico_ma=%d\n",ico_ma);
	}
	return true;
}
static void bq25890_power_init(struct bq25890_chip *chip)
{
	chip->pd_retries = 0;
	chip->pd_state = 0;
	chip->ico_idx = 0;
	chip->parallel.initial_ico_ma = 0;
	chip->parallel.input_current_ma = DEFAULT_INPUT_CURRENT_MA;
	chip->parallel.fastchg_current_ma = 0;
	chip->vindpm_mv = 0;

	pr_debug("start power detected after 5s hvdcp_type=%d\n",chip->hvdcp_type);
	bq25890_stay_awake(chip,PM_POWER_CHECK);
	cancel_delayed_work_sync(&chip->power_detect_work);
	schedule_delayed_work(&chip->power_detect_work,
	msecs_to_jiffies(5000));
}
static void bq25890_power_deinit(struct bq25890_chip *chip)
{
	chip->pd_retries = 0;
	chip->pd_ico_retries = 0;
	chip->pd_state = 0;
	chip->parallel.initial_ico_ma = 0;
	chip->parallel.input_current_ma = DEFAULT_INPUT_CURRENT_MA;
	chip->parallel.fastchg_current_ma = 0;
	chip->input_current_ma = 0;
	chip->fastchg_current_ma = 0;
	chip->cfg_fastchg_current_ma = 0;
	chip->cfg_input_current_ma = 0;
	chip->vindpm_mv = 0;
	chip->ico_ma = 0;
}

static int bq25890_power_config(struct bq25890_chip *chip)
{
	int rc = 0,hvdcp_type,vindpm_mv;
	bool is_hvdcp = false,parallel_enable = false;
	bool primary_enable = true;

	is_hvdcp = bq25890_is_hvdcp(chip);
	hvdcp_type = bq25890_get_hvdcp_type(chip);

	mutex_lock(&chip->parallel.lock);
	if (is_hvdcp && hvdcp_type == HVDCP_TYPE_9V) {
		vindpm_mv = chip->vindpm_9v_thr_mv;
	} else {
		vindpm_mv = chip->vindpm_5v_thr_mv;
	}
if(!chip->disable_parallel_charger){
	switch(chip->pd_state) {
	case POWER_DETECT_STATE_FIRST:
		parallel_enable = true;
		primary_enable = true;
		chip->cfg_fastchg_current_ma = 512;
		chip->cfg_input_current_ma = 2000;
		chip->parallel.fastchg_current_ma = 1536;
		chip->parallel.input_current_ma = 1000;
		break;
	case POWER_DETECT_STATE_SECOND:
		parallel_enable = true;
		primary_enable = true;
		chip->cfg_fastchg_current_ma = 2048;
		chip->cfg_input_current_ma = 2000;
		chip->parallel.fastchg_current_ma = 1536;
		chip->parallel.input_current_ma = 1000;
		break;
	default:
		parallel_enable = true;
		chip->cfg_fastchg_current_ma = 2048;
		chip->cfg_input_current_ma = 1000;
		break;
	}
}else{
	switch(chip->pd_state) {
	case POWER_DETECT_STATE_FIRST:
		parallel_enable = true;
		primary_enable = true;
		chip->cfg_fastchg_current_ma = 512;
		chip->cfg_input_current_ma = 1600;
		chip->parallel.fastchg_current_ma = 0;
		chip->parallel.input_current_ma = 0;
		break;
	case POWER_DETECT_STATE_SECOND:
		parallel_enable = true;
		primary_enable = true;
		chip->cfg_fastchg_current_ma = 2624;
		chip->cfg_input_current_ma = 1600;
		chip->parallel.fastchg_current_ma = 0;
		chip->parallel.input_current_ma = 0;
		break;
	default:
		parallel_enable = true;
		chip->cfg_fastchg_current_ma = 2048;
		chip->cfg_input_current_ma = 1000;
		break;
	}
}
	chip->total_fastchg_current_ma = chip->cfg_fastchg_current_ma + chip->parallel.fastchg_current_ma;
	mutex_unlock(&chip->parallel.lock);

	/*secondary charger vindpm always 4.4v*/
	if (chip->use_absolute_vindpm) {
		bq25890_set_vindpm_absolute(chip,vindpm_mv);
	} else {
		bq25890_set_vindpm_offset(chip,chip->vindpm_offset_mv);		
	}

	bq25890_set_usb_current_max(chip,chip->cfg_input_current_ma);
	bq25890_set_fastchg_current(chip,chip->cfg_fastchg_current_ma);
	//bq25890_enable_charging(chip,primary_enable);

	//set_property_on_parallel(chip,POWER_SUPPLY_PROP_PRESENT,chip->usb_present);
	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,parallel_enable);

	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
								chip->parallel.fastchg_current_ma);
	set_property_on_parallel(chip,POWER_SUPPLY_PROP_CURRENT_MAX,
								chip->parallel.input_current_ma);

	pr_debug("is_hvdcp=%d hvdcp_type=%d parallel_enable =%d fastchg_ma=%d input_ma=%d\n",
			is_hvdcp,hvdcp_type,parallel_enable,chip->cfg_fastchg_current_ma,
			chip->cfg_input_current_ma);

	return rc;
}
#define POWER_DETECT_RETRIES_MAX 2
#define SECOND_POWER_DETECT_RETRIES_MAX 1
static void bq25890_power_detect_work(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip,
								power_detect_work.work);
	int rc;
	unsigned int delay = 0,ico_ma;
	bool usb_present;
	static int tatol_ico_ma = 0;
	int usb_supply_type;

	usb_present = bq25890_is_charger_present(chip);
	usb_supply_type = get_usb_supply_type(chip);
	chip->hvdcp_type = bq25890_get_hvdcp_type(chip);

	if (!usb_present || usb_supply_type != POWER_SUPPLY_TYPE_USB_DCP ||
		chip->hvdcp_type == HVDCP_TYPE_12V) {
		pr_debug("!usb_present || !DCP || 12V detected stop\n");
		chip->pd_state = POWER_DETECT_STATE_DONE;
		bq25890_relax(chip,PM_POWER_CHECK);
		return;
	}

	pr_debug("state=%d,pd_retries=%d pd_ico_retries =%d ico_idx=%d\n",
			chip->pd_state,chip->pd_retries,chip->pd_ico_retries,chip->ico_idx);

	switch (chip->pd_state) {
	case POWER_DETECT_STATE_FIRST:
		rc = bq25890_power_config(chip);
		bq25890_force_ico(chip);
		chip->pd_state = POWER_DETECT_STATE_FIRST_RESULT;
		delay = 600;
		break;
	case POWER_DETECT_STATE_FIRST_RESULT:
		if (bq25890_power_ico_trigered(chip)){
			ico_ma = bq25890_read_idpm_limit(chip);

			tatol_ico_ma += ico_ma;
			if (++chip->pd_ico_retries >= POWER_DETECT_RETRIES_MAX) {
				pr_debug("trigered retries max ico_ma=%d pd_ico_retries=%d\n",chip->ico_ma,chip->pd_ico_retries);
				chip->ico_ma = tatol_ico_ma / POWER_DETECT_RETRIES_MAX;
				tatol_ico_ma = 0;
				chip->pd_retries = 0;
				chip->pd_ico_retries = 0;
				chip->ico_idx = 0;
				chip->pd_state = POWER_DETECT_STATE_DONE;				
			} else {
				pr_debug("ico triggered retries =%d\n",chip->pd_ico_retries);
				bq25890_force_ico(chip);
				chip->pd_state = POWER_DETECT_STATE_FIRST;
				delay = 600;
			}

		} else {
			chip->ico_ma = chip->cfg_input_current_ma;
			if (++chip->pd_retries >= POWER_DETECT_RETRIES_MAX){
				pr_debug("retries max ico_ma=%d pd_retries=%d\n",chip->ico_ma,chip->pd_retries);
				chip->pd_retries = 0;
				chip->pd_ico_retries = 0;
				tatol_ico_ma = 0;
				chip->pd_state = POWER_DETECT_STATE_SECOND;				
			} else {
				pr_debug("ico no triggered retries =%d\n",chip->pd_retries);
				bq25890_force_ico(chip);
				chip->pd_state = POWER_DETECT_STATE_FIRST;
				delay = 600;
			}
		}
		break;
	case POWER_DETECT_STATE_SECOND:
		rc = bq25890_power_config(chip);
		bq25890_force_ico(chip);
		chip->pd_state = POWER_DETECT_STATE_SECOND_RESULT;
		delay = 800;
		break;
	case POWER_DETECT_STATE_SECOND_RESULT:
		if (bq25890_power_ico_trigered(chip)){
			ico_ma = bq25890_read_idpm_limit(chip);
			tatol_ico_ma += ico_ma;
			if (++chip->pd_ico_retries >= SECOND_POWER_DETECT_RETRIES_MAX) {
				pr_debug("trigered retries max ico_ma=%d pd_ico_retries=%d\n",chip->ico_ma,chip->pd_ico_retries);
				chip->ico_ma = tatol_ico_ma / SECOND_POWER_DETECT_RETRIES_MAX;
				tatol_ico_ma = 0;
				chip->pd_retries = 0;
				chip->pd_ico_retries = 0;
				chip->pd_state = POWER_DETECT_STATE_DONE;				
			} else {
				pr_debug("ico triggered retries =%d\n",chip->pd_ico_retries);
				bq25890_force_ico(chip);
				chip->pd_state = POWER_DETECT_STATE_SECOND;
				delay = 600;
			}
		} else {
			chip->ico_ma = chip->cfg_input_current_ma;
			if (++chip->pd_retries >= SECOND_POWER_DETECT_RETRIES_MAX){
				pr_debug("retries max ico_ma=%d pd_retries=%d\n",chip->ico_ma,chip->pd_retries);
				chip->pd_retries = 0;
				chip->pd_ico_retries = 0;
				tatol_ico_ma = 0;
				chip->pd_state = POWER_DETECT_STATE_DONE;				
			} else {
				pr_debug("ico no triggered retries =%d\n",chip->pd_retries);
				bq25890_force_ico(chip);
				chip->pd_state = POWER_DETECT_STATE_SECOND;
				delay = 600;
			}
		}
		break;
	case POWER_DETECT_STATE_DONE:
		pr_debug("power detected done ico_ma=%d total_fastchg_current=%d\n",
			chip->ico_ma,chip->total_fastchg_current_ma);
		power_supply_changed(chip->batt_psy);
		/*secondary charger vindpm always 4.4v*/
		if (chip->use_absolute_vindpm) {
			bq25890_set_vindpm_absolute(chip,chip->vindpm_5v_thr_mv);
		} else {
			bq25890_set_vindpm_offset(chip,chip->vindpm_offset_mv); 	
		}
		bq25890_set_input_current_user(chip,chip->ico_ma);
		bq25890_set_fastchg_current_user(chip,chip->total_fastchg_current_ma);		
		bq25890_relax(chip,PM_POWER_CHECK);
		return;
	default:
		pr_debug("not support state=%d\n",chip->pd_state);
		bq25890_relax(chip,PM_POWER_CHECK);
		return;
	}

	schedule_delayed_work(&chip->power_detect_work,
				msecs_to_jiffies(delay));
}
static void bq25890_external_ac_changed(struct power_supply *psy);
#define FORCE_DPDM_RETRIES_MAX 		12
#define RERUN_APSD_PERIOD_MS 	5000
static void bq25890_rerun_apsd_work(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip,
								rerun_apsd_work.work);
	int usb_supply_type;
	bool dpdm_doned = false;
	static int retries = 0;
	static bool reruned = false;
	union power_supply_propval ret = {0,};

	int rerun_period_ms = msecs_to_jiffies(RERUN_APSD_PERIOD_MS);

	if (!chip->usb_present) {
		pr_debug("usb_present=%d\n",chip->usb_present);
		retries = 0;
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}

	usb_supply_type = get_usb_supply_type(chip);
	pr_debug("usb_supply_type=%d retries=%d period_ms=%d\n",usb_supply_type,retries,RERUN_APSD_PERIOD_MS);

	/*hvdcp detected,usb max current configged based on type*/
	if (usb_supply_type == POWER_SUPPLY_TYPE_USB ||
		usb_supply_type == POWER_SUPPLY_TYPE_USB_DCP ||
		usb_supply_type == POWER_SUPPLY_TYPE_USB_CDP) {
		if (reruned) {
			pr_debug("rerun success\n");
			chip->usb_psy_type = usb_supply_type;
                        /* @vivo add for Dual-engine */
                        power_supply_changed(chip->batt_psy);
			if(chip->usb_psy){
				ret.intval = usb_supply_type;
				power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
			}
			if (chip->pd_state == POWER_DETECT_STATE_DONE) {
				bq25890_power_init(chip);
			}
			retries = 0;
			reruned = false;
			bq25890_relax(chip,PM_IRQ_CHECK);
			return;
		} else {
			chip->usb_psy_type = usb_supply_type;
			pr_debug("type detected right,not rerun\n");
			retries = 0;
			reruned = false;
			bq25890_relax(chip,PM_IRQ_CHECK);
			return;
		}
	}

	/*ap cannot rerun w/h usb plugged*/
	if (++retries >= FORCE_DPDM_RETRIES_MAX) {
		pr_debug("start usb type detected by ap\n");
		//power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		//power_supply_set_present(chip->usb_psy, chip->usb_present);

		chip->ico_ma = DEFAULT_UNKNOWN_MA;
		chip->input_current_ma = 0;
		bq25890_set_input_current_user(chip,chip->ico_ma);

		retries = 0;
		reruned = false;
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}

	dpdm_doned = bq25890_check_force_dpdm_done(chip);
	pr_debug("dpdm_doned=%d\n",dpdm_doned);
	/*delay based on hvdcp_en*/
	bq25890_force_dpdm(chip);
	msleep(800);
	bq25890_force_dpdm(chip);
	reruned = true;
	schedule_delayed_work(&chip->rerun_apsd_work,
		round_jiffies_relative(rerun_period_ms));
}

static void bq25890_external_ac_changed(struct power_supply *psy)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);
	union power_supply_propval ret = {0,};
	//union power_supply_propval usb_type = {0,};
	int rc,current_limit = 0,iinlimt_ma;
	int usb_supply_type;

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
	#if 0
	power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &usb_type);
	#endif
	usb_supply_type = get_usb_supply_type(chip);
	pr_warn("usb_online=%d,last_type=%d,type=%d\n",
			chip->usb_online,chip->usb_psy_type,usb_supply_type);

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = ret.intval / 1000;

	pr_warn("current_limit = %d,usb_target_current=%d,\n",
		current_limit,chip->total_input_current_ma);

	if (chip->usb_online && bq25890_is_charger_present(chip)) {
		if(chip->usb_psy_type != usb_supply_type){
			chip->usb_psy_type = usb_supply_type;

			mutex_lock(&chip->current_change_lock);
			if (chip->usb_psy_type == POWER_SUPPLY_TYPE_USB)
				chip->ico_ma = DEFAULT_SDP_MA;
			else if (chip->usb_psy_type == POWER_SUPPLY_TYPE_USB_FLOAT)
				chip->ico_ma = DEFAULT_UNKNOWN_MA;
			else if (chip->usb_psy_type == POWER_SUPPLY_TYPE_USB_CDP)
				chip->ico_ma = DEFAULT_CDP_MA;
			else if (chip->usb_psy_type == POWER_SUPPLY_TYPE_USB_DCP)
				chip->ico_ma = DEFAULT_WALL_CHG_MA;
			else
				chip->ico_ma = DEFAULT_SDP_MA;

			if (chip->vbus_type == BQ25890_VBUS_NONSTAND) {
				iinlimt_ma = bq25890_read_iinlimt(chip);
				chip->ico_ma = min(iinlimt_ma,chip->ico_ma);
				pr_debug("iinlimt_ma=%d ico_ma=%d\n",iinlimt_ma,chip->ico_ma);
			}

			pr_debug("detected setting mA = %d\n",chip->ico_ma);
			bq25890_set_input_current_user(chip,chip->ico_ma);
			mutex_unlock(&chip->current_change_lock);
		}
	}
	power_supply_changed(chip->batt_psy);
}

#define USB_MAX_IBAT_MA 1500
static bool bq25890_is_recharging(struct bq25890_chip *chip)
{
	bool rechg = false;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	chg_type = bq25890_get_prop_charge_type(chip);

	if (chip->ui_soc == 100 && chip->batt_full_flag && (chg_type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE || chg_type == POWER_SUPPLY_CHARGE_TYPE_FAST))
		rechg = true;
	else
		rechg = false;

	pr_info("rechargering:%d\n", rechg);

	return rechg;
}

static int RECHARGING_DELTA_MV = 100;
static int RECHARGING_THRESHOLD_SOC = 98;
static int bq25890_start_recharging(struct bq25890_chip *chip)
{
	bool recharging = false;
	//bool is_hvdcp;

	if(power_off_charging_mode){
		RECHARGING_THRESHOLD_SOC = 99;
		RECHARGING_DELTA_MV = 80;
	}

	if (chip->ui_soc == 100 && chip->batt_full_flag){
		if (chip->cal_soc <= RECHARGING_THRESHOLD_SOC) {
			recharging = true;
		} else {
			recharging = false;
		}
	}

	if (chip->recharging != recharging) {
		mutex_lock(&chip->chgdone_lock);
		if (recharging) {
			pr_warn("start recharging\n");
			bq25890_enable_hiz(chip,false);
			bq25890_termination_enable(chip,true);
			chip->chg_done = false;
			/*is_hvdcp = bq25890_is_hvdcp(chip);
			if (is_hvdcp) {
				pr_warn("adjust from 5v to 9v\n");
				bq25890_hvdcp_enable(chip,true);
				bq25890_force_dpdm(chip);
				msleep(800);
				bq25890_force_dpdm(chip);
				msleep(3000);
			}*/

			chip->input_current_ma = 0;
			chip->fastchg_current_ma = 0;
			set_property_on_parallel(chip,POWER_SUPPLY_PROP_PRESENT,0);
			set_property_on_parallel(chip,POWER_SUPPLY_PROP_PRESENT,1);
		}
		chip->recharging = recharging;
		mutex_unlock(&chip->chgdone_lock);
	}

	pr_debug("vbat_mv=%d vbat_max_m=%d vcal_soc=%d rechargering:%d\n",
			chip->vbat_mv,chip->vbat_max_mv,chip->cal_soc,recharging);

	return 0;
}
static int bq25890_get_prop_chg_status(struct bq25890_chip *chip)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	bool recharging = false;

	if (chip->status & BQ25890_STATUS_ATTACHED && !(chip->status & BQ25890_STATUS_PG)) {
		pr_debug("charging reported before type detected\n");
		chg_status = POWER_SUPPLY_STATUS_CHARGING;
		return chg_status;
	}

	recharging = bq25890_is_recharging(chip);

	if(chip->batt_full_flag && chip->cal_soc >= 95){
		chg_status = POWER_SUPPLY_STATUS_FULL;
		return chg_status;
	}

	if(recharging && chip->cal_soc < 95){
		chip->batt_full = false;
		chip->batt_full_flag = false;
	}

	chg_type = bq25890_get_prop_charge_type(chip);
	switch (chg_type) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		if (bq25890_is_charger_present(chip)) {
			if(chip->batt_full && chip->ui_soc >= 100){
				chg_status = POWER_SUPPLY_STATUS_FULL;
				chip->batt_full_flag = true;
			}
			else if(chip->batt_full && chip->ui_soc < 100)
				chg_status = POWER_SUPPLY_STATUS_CHARGING;
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

	pr_warn("chg status = %d\n", chg_status);
	return chg_status;
}
static int bq25890_get_prop_chg_status_ex(struct bq25890_chip *chip)
{
	int chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret = 0;
	u8 temp;

	ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &temp);
	if(ret < 0){
		pr_err("%s Failed to read register 0x0b:%d\n",__func__,ret);
		return 0;
	}

	temp &= BQ25890_CHRG_STAT_MASK;
	temp >>= BQ25890_CHRG_STAT_SHIFT;

	if (temp == BQ25890_CHRG_STAT_FASTCHG || temp == BQ25890_CHRG_STAT_PRECHG)
		chg_status = POWER_SUPPLY_STATUS_CHARGING;
	else if(temp == BQ25890_CHRG_STAT_CHGDONE)
		chg_status = POWER_SUPPLY_STATUS_FULL;
	else if(bq25890_is_charger_present(chip))
		chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		chg_status = POWER_SUPPLY_STATUS_DISCHARGING;

	pr_warn("ex chg status = %d\n", chg_status);
	return chg_status;
}

static int bq25890_get_prop_batt_present(struct bq25890_chip *chip)
{
	return 1;
}

#define DEFAULT_BATT_CAPACITY	50
static int bq25890_get_prop_batt_capacity(struct bq25890_chip *chip)
{
	union power_supply_propval ret = {0, };
	static bool the_flag = true;
	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;
	if (!chip->bms_psy && chip->bms_psy_name){
		chip->bms_psy = power_supply_get_by_name((char *)chip->bms_psy_name);
	}

	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		chip->soc= ret.intval;
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, &ret);
		chip->cal_soc = ret.intval;

		 pr_err("soc=%d,ui_soc=%d,vbat_mv=%d,chg_present=%d,cal_soc=%d\n",
				chip->soc,chip->ui_soc,chip->vbat_mv,chip->usb_present,chip->cal_soc);	 
			if(chip->batt_full == true)
			{
				the_flag = false;
				chip->ui_soc = chip->soc;
				chip->full_flag = true;
			    return chip->ui_soc;
			}else{

			}
			chip->ui_soc = chip->soc;
			if(chip->usb_present){
				if(the_flag == true)
					if(chip->ui_soc == 100 && chip->full_flag == false)
						chip->full_flag = true;
				if(chip->ui_soc < 100){
					the_flag = false;
					chip->full_flag = false;
				}
			    if(chip->ui_soc >=100 && !chip->full_flag)
			        chip->ui_soc = 99;
			}else{
				if(the_flag == true)
					if(chip->ui_soc == 100 && chip->full_flag == false)
						chip->full_flag = true;
			    if(chip->ui_soc<100){
					the_flag = false;
			        if(chip->full_flag==true)
			            chip->full_flag=false;
			    }else if(chip->ui_soc>=100 && chip->full_flag==false){
			        chip->ui_soc = 99;
			    }
			}
			//add for factory test
			if(!chip->usb_present){
				if(chip->vbat_mv > 3600000 && !chip->ui_soc)
					return 1;
			}
			//end
			return chip->ui_soc;
	}

	return DEFAULT_BATT_CAPACITY;
}
#define DEFAULT_TEMP 250
static int bq25890_get_prop_batt_temp(struct bq25890_chip *chip)
{
	/*int rc = 0;
	
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &results);
	if (rc) {
		pr_debug("Unable to read batt temperature rc=%d\n", rc);
		return DEFAULT_TEMP;
	}
	//pr_debug("get_bat_temp %d, %lld\n",
	//	results.adc_code, results.physical);
	return (int)results.physical;*/
	return DEFAULT_TEMP;
}
#define DEFAULT_BATT_CURRENT_NOW	0
static int bq25890_get_prop_batt_current_now(struct bq25890_chip *chip)
{
	int ibat_ma,parallel_ma = 0;
	int rc;

	bq25890_adc_start(chip,true);
	msleep(80);
	rc = bq25890_adc_read_charge_current(chip);
	if (rc < 0) {
		pr_debug("Couldn't get current rc = %d\n", rc);
		ibat_ma = DEFAULT_BATT_CURRENT_NOW;
	} else {
		ibat_ma = rc;
	}
	//pr_debug("ibat_ma=%d\n",ibat_ma);
	if (!chip->primary_charger) {
		return ibat_ma;
	}

	get_property_from_parallel(chip,POWER_SUPPLY_PROP_CURRENT_NOW,&parallel_ma);
	//pr_debug("parallel_ma=%d\n",parallel_ma);
	ibat_ma = ibat_ma + parallel_ma;
	return ibat_ma;
}

static int bq25890_get_prop_battery_voltage_now(struct bq25890_chip *chip)
{
	/*int rc = 0;
	struct qpnp_vadc_result results;
	int vbat_uv,ir_vbat_uv,i;
	int vbat_uv_sum = 0;
	rc = bq25890_adc_read_battery_volt(chip);
	if (rc > 0) {
		vbat_uv = rc * 1000;
		//pr_debug("original vbat_uv=%d\n",vbat_uv);
	}
	for(i=0; i<3; i++){
		rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &results);
		if (rc) {
			chip->vbat_mv = 0;
			pr_err("Unable to read vbat rc=%d\n", rc);
			return 0;
		}
		vbat_uv_sum += results.physical;
	}
	vbat_uv = vbat_uv_sum/3;
	//IR compensate
	ir_vbat_uv = vbat_uv - chip->batt_ircomp_mom * chip->ibat_ma;
	//pr_debug("vbat_uv=%d, after %d IR compensate, ir_vbat_uv=%d\n",vbat_uv,chip->batt_ircomp_mom,ir_vbat_uv);

	chip->vbat_mv = ir_vbat_uv / 1000;
	return ir_vbat_uv;*/
	return 0;
}
static void bq25890_set_prop_charging_empty(struct bq25890_chip *chip,
	int enabled)
{
	int empty_enabled = !!enabled;
	union power_supply_propval ret = {0,};

	pr_info("set charge empty:%d\n", empty_enabled);
	if(empty_enabled){
		if(chip->usb_psy){
			ret.intval = 0;
			power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
		}
		bq25890_enable_hiz(chip, true);
		//chip->vbus_hized = true;
	} else {
		bq25890_enable_hiz(chip, false);
		//chip->vbus_hized = false;
		if(chip->usb_psy){
			ret.intval = get_usb_supply_type(chip);
			power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
		}
	}
}

static int bq25890_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq25890_get_prop_chg_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq25890_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq25890_is_chg_enabled(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq25890_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq25890_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
#if 1/* @vivo add for Dual-engine */
                if (get_usb_supply_type(chip) == POWER_SUPPLY_TYPE_USB_DCP &&
                                bq25890_get_hvdcp_type(chip) == HVDCP_TYPE_9V) {
                        printk("battery DUAL-ENGINE\n");
                        val->intval = POWER_SUPPLY_HEALTH_DUAL_ENGINE;
                } else {
                        val->intval = chip->batt_health;
                }
#else
		val->intval = chip->batt_health;
#endif
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
	case POWER_SUPPLY_PROP_TEMP:
		if(!bbk_fixed_temp){
			val->intval = bq25890_get_prop_batt_temp(chip);
		}else{
			val->intval = bbk_fixed_temp * 10;
			pr_err("### bbk_fixed_temp=%d ###\n",bbk_fixed_temp);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq25890_get_prop_battery_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	    val->intval = chip->vbat_max_mv * 1000;
	    break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq25890_get_prop_batt_current_now(chip);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = chip->hvdcp_type;
		break;
	case POWER_SUPPLY_PROP_WEAK_CHARGER:
		if(chip->ico_ma && chip->ico_ma <= 500)
			val->intval = 1;
		else
			val->intval = 0;
		pr_err("### ico_ma=%d, WeakCharger=%d ###\n",chip->ico_ma,val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->batt_full;
		break;
	case POWER_SUPPLY_PROP_IIC_STATE:
		val->intval = chip->iic_state;
		break;
	case POWER_SUPPLY_PROP_RECHARGE_STATE:
		val->intval = chip->recharging;
		break;
	case POWER_SUPPLY_PROP_FIXED_TEMP:
		val->intval = bbk_fixed_temp;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static void bq25890_rerun_base_fb_on_work(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip,
								rerun_base_fb_on_work.work);
	int usb_supply_type;
	bool dpdm_doned = false;
	static int retries = 0;
	int rerun_period_ms = msecs_to_jiffies(RERUN_APSD_PERIOD_MS);

	if (!chip->usb_present) {
		pr_debug("usb_present=%d\n",chip->usb_present);
		retries = 0;
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}

	usb_supply_type = get_usb_supply_type(chip);
	pr_debug("usb_supply_type=%d retries=%d period_ms=%d\n",usb_supply_type,retries,RERUN_APSD_PERIOD_MS);

	/*hvdcp detected,usb max current configged based on type*/
	if (usb_supply_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pr_debug("rerun fb-on success\n");
		chip->usb_psy_type = usb_supply_type;
		retries = 0;
		chip->input_current_ma = 0;
		bq25890_set_input_current_user(chip,chip->ico_ma);
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}

	/*ap cannot rerun w/h usb plugged*/
	if (++retries >= FORCE_DPDM_RETRIES_MAX) {
		pr_debug("usb rerun fb-on max \n");

		retries = 0;
		chip->input_current_ma = 0;
		bq25890_set_input_current_user(chip,chip->ico_ma);
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}

	dpdm_doned = bq25890_check_force_dpdm_done(chip);
	pr_debug("fb-on dpdm_doned=%d\n",dpdm_doned);
	/*delay based on hvdcp_en*/
	bq25890_force_dpdm(chip);
	msleep(800);
	bq25890_force_dpdm(chip);
	schedule_delayed_work(&chip->rerun_base_fb_on_work,
		round_jiffies_relative(rerun_period_ms));
}

/*extern char* get_bbk_board_version(void);*/
static int bq25890_battery_set_property(struct power_supply *psy,
				  enum power_supply_property prop,
				  const union power_supply_propval *val)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);
	char *board_version = NULL;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		bq25890_enable_charging(chip,val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_DC_CUTOFF:
		bq25890_set_prop_charging_empty(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_FORCE_DPDM:

		board_version = 0;//get_bbk_board_version();
		if(!board_version){
			pr_err("failed to get board_version\n");
			break;
		}else{
			pr_err("get board_version=%s,iic_state=%d\n",board_version,chip->iic_state);
			if((board_version[0] == '1' && chip->iic_state == 0) ||
					(board_version[0] == '1' && chip->iic_state == 1)){
				pr_err("need adjust adapt volt 9->5\n");
			}else{
				pr_err("no need adjust adapt volt\n");
				break;
			}
		}

		if(chip->fb_on_ajust_adapt == val->intval){
			pr_debug("set the same fb_on_ajust_adapt=%d,skipping\n",val->intval);
			break;
		}

		if(chip->hvdcp_type == HVDCP_TYPE_9V){
			cancel_delayed_work_sync(&chip->rerun_base_fb_on_work);
			if(val->intval){
				pr_debug("fb-on:hvdcp_type=%d,adjust to 5v\n",chip->hvdcp_type);
				bq25890_hvdcp_enable(chip,false);
				bq25890_force_dpdm(chip);
				msleep(800);
				bq25890_force_dpdm(chip);
			}else if(!val->intval){
				pr_debug("fb-on:hvdcp_type=%d,adjust to 9v\n",chip->hvdcp_type);
				bq25890_hvdcp_enable(chip,true);
				bq25890_force_dpdm(chip);
				msleep(800);
				bq25890_force_dpdm(chip);
			}

			bq25890_stay_awake(chip,PM_IRQ_CHECK);
			schedule_delayed_work(&chip->rerun_base_fb_on_work,msecs_to_jiffies(3000));
		}
		chip->fb_on_ajust_adapt = val->intval;
		break;
	case POWER_SUPPLY_PROP_IIC_STATE:
		chip->iic_state = val->intval;
		break;
	case POWER_SUPPLY_PROP_FIXED_TEMP:
		bbk_fixed_temp = val->intval;
		pr_err("### bbk_fixed_temp=%d ###\n",bbk_fixed_temp);
		break;
	default:
		return -EINVAL;
	}
	return 0;

}
static int bq25890_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_IIC_STATE:
	case POWER_SUPPLY_PROP_FIXED_TEMP:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static char *bq25890_battery_supplied_to[] = {
	//"bms",
};
static enum power_supply_property bq25890_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	//POWER_SUPPLY_PROP_WEAK_CHARGER,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	//POWER_SUPPLY_PROP_IIC_STATE,
	POWER_SUPPLY_PROP_FIXED_TEMP,
};
static char *bq25890_ac_supplied_to[] = {
	"cms",
};

static enum power_supply_property bq25890_ac_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	//POWER_SUPPLY_PROP_LIMIT_INPUT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};
static int bq25890_get_prop_input_voltage(struct bq25890_chip *chip)
{
	int vbus_mv;
	bq25890_adc_start(chip,true);
	msleep(80);
	vbus_mv = bq25890_adc_read_vbus_volt(chip);
	if(vbus_mv < 0)
		vbus_mv = 0;
	return vbus_mv;

}
static int bq25890_get_prop_battery_health_status(struct bq25890_chip *chip)
{

	unsigned long timeout;
	if(chip->charge_begin > 0) {
		timeout= chip->charge_begin + chip->chg_tmout_mins * 60 * HZ;
		if(chip->chg_type > POWER_SUPPLY_CHARGE_TYPE_NONE &&
			time_after(jiffies, timeout)){
			if(bbk_cmcc_attribute)
				;//ignore for ccmc
			else
				set_bit(bq25890_CHG_TIMEOUT_STATUS, &chip->psy_status);
		}
		if(chip->chg_type > POWER_SUPPLY_CHARGE_TYPE_NONE){
			pr_err("charging runs %d seconds,status=%ld,chg_tmout_mins=%d\n", jiffies_to_msecs(jiffies-chip->charge_begin)/ 1000,chip->psy_status,chip->chg_tmout_mins);
		}
	}

	pr_err("the psy_status=%ld\n", chip->psy_status);
	return chip->psy_status;
}

static int bq25890_ac_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);
	int usb_type,hv_type;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq25890_get_prop_battery_health_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->batt_health;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		usb_type = get_usb_supply_type(chip);
		hv_type = bq25890_get_hvdcp_type(chip);
		if(usb_type == POWER_SUPPLY_TYPE_USB_DCP
			&& hv_type >= HVDCP_TYPE_9V){
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP;
		}else{
			val->intval = usb_type;
		}
		pr_err("#### %d,%d,%d ####\n",usb_type,hv_type,val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->batt_1C_current_ma;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq25890_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq25890_get_prop_input_voltage(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chip->vbat_max_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq25890_is_chg_enabled(chip);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
        val->intval = chip->ico_ma * 1000;
        break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pr_debug("get bbk_cmcc_attribute=%d\n",bbk_cmcc_attribute);
		val->intval = bbk_cmcc_attribute;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq25890_ac_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);
	//unsigned long flags;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		return 0;
	case POWER_SUPPLY_PROP_HEALTH:
		chip->batt_health = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		chip->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		//bq25890_set_ibat_max(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		bq25890_enable_charging(chip, val->intval);
		//if (val->intval)
			//bq25890_trigger_recharge(chip);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		bq25890_system_temp_level_set(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		bq25890_set_chargevoltage(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pr_debug("set fastchg from user=%d\n",val->intval);
		bq25890_set_fastchg_current_user(chip, val->intval);
		//bq25890_set_fastchg_current_user(chip, 2000);
		break;
	case POWER_SUPPLY_PROP_LIMIT_INPUT:
		pr_debug("set input from user=%d\n",val->intval);
		bq25890_set_input_current_user(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pr_debug("set bbk_cmcc_attribute=%d\n",val->intval);
		bbk_cmcc_attribute = !!val->intval;
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(chip->ac_psy);
	return 0;
}

static enum power_supply_property bq25890_parallel_properties[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
};
static int bq25890_parallel_enable_charging(struct bq25890_chip *chip,
						bool enable)
{
	if (enable == chip->parallel.enabled) {
		pr_debug("enable %d -> %d, skipping\n",
				chip->parallel.enabled, enable);
		return 0;
	}
	if (chip->en_gpio > 0) {
		if (enable)
			gpio_direction_output(chip->en_gpio,0);
		else
			gpio_direction_output(chip->en_gpio,1);
	}
	bq25890_enable_charging(chip, enable);

	chip->parallel.enabled = enable;
	return 0;
}
static bool bq25890_parallel_is_enabled(struct bq25890_chip *chip)
{
	bool is_enable;
#if 0
	is_enable = gpio_get_value(chip->en_gpio);
	if (!is_enable) {
		pr_debug("en_gpio=%d\n",is_enable);
		return false;
	}
#endif
	is_enable = bq25890_is_chg_enabled(chip);
	if (!is_enable) {
		pr_debug("is enable=%d\n",is_enable);
		return false;
	}

	is_enable = bq25890_is_hiz(chip);
	if (is_enable) {
		pr_debug("is hiz=%d\n",is_enable);
		return false;
	}

	return true;
}
static int bq25890_parallel_set_chg_present(struct bq25890_chip *chip,
						int present)
{
	pr_debug("present=%d\n",present);
	if (present == chip->parallel_chgr_present) {
		pr_debug("present %d -> %d, skipping\n",
				chip->parallel_chgr_present, present);
		return 0;
	}
	if (present) {
		/*only debug regs*/
		cancel_delayed_work_sync(&chip->monitor_work);
		schedule_delayed_work(&chip->monitor_work, msecs_to_jiffies(5000));
	} else {
		chip->input_current_ma = 0;
		chip->fastchg_current_ma = 0;
		chip->vindpm_mv = 0;
		chip->vindpm_offset_mv = 0;
		bq25890_parallel_enable_charging(chip, false);
		bq25890_enable_hiz(chip, true);
	}
	chip->parallel_chgr_present = present;

	return 0;
}
#define	BQ25890_PN_STATUS_DEFAULT_OK	0x00
#define	BQ25890_PN_STATUS_I2C_ERROR	0x01
static int bq25890_get_prop_batt_slave_charger_status(struct bq25890_chip *chip)
{
	int i = 0;
	int rc;
	u8 temp;

	rc = bq25890_read_reg(chip->client, BQ25890_REG_14, &temp);
	if (rc) {
		pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", rc);
		for(; i < 3; i++){
			msleep(1);
			rc = bq25890_read_reg(chip->client, BQ25890_REG_14, &temp);
			if(rc){
				pr_err("failed to read SYSTEM_STATUS_REG rc=%d\n", rc);
			}else{
				break;
			}
		}
	}
	//pr_err("i=%d\n", i);

	if(i >= 3)
		return BQ25890_PN_STATUS_I2C_ERROR;
	else
		return BQ25890_PN_STATUS_DEFAULT_OK;
}
#define VINDPM_OFFSET_MAX_MV 3100
static int bq25890_parallel_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		pr_err("parallel_chgr_present=%d,val=%d\n",chip->parallel_chgr_present,val->intval);
		if (chip->parallel_chgr_present) {
			bq25890_parallel_enable_charging(chip,val->intval);
			bq25890_enable_hiz(chip, !val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		pr_err("val=%d\n",val->intval);
		rc = bq25890_parallel_set_chg_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		pr_err("parallel_chgr_present=%d,current=%d\n",
				chip->parallel_chgr_present, val->intval);
		if (chip->parallel_chgr_present) {
			rc = bq25890_set_fastchg_current(chip,	val->intval /1000);
			//fuelsummary_collect_value(ID_SIC_INPUT, val->intval / 1000);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		pr_err("parallel_chgr_present=%d, usb current=%d\n",chip->parallel_chgr_present,val->intval);
		if (chip->parallel_chgr_present) {
			rc = bq25890_set_usb_current_max(chip, val->intval / 1000);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		/*
		pr_err("parallel_chgr_present=%d,vbat_max_mv=%d\n",chip->parallel_chgr_present,chip->vbat_max_mv);
		if (chip->parallel_chgr_present &&
			(chip->vbat_max_mv != val->intval)) {
			rc = bq25890_set_chargevoltage(chip, val->intval);
			chip->vbat_max_mv = val->intval;
		} else {
			chip->vbat_max_mv = val->intval;
		}
		*/
		break;
	case POWER_SUPPLY_PROP_VINDPM:
		pr_err("parallel_chgr_present=%d,vindpm=%d\n",chip->parallel_chgr_present,val->intval);
		if (chip->parallel_chgr_present) {
			if (val->intval > VINDPM_OFFSET_MAX_MV) {
				chip->use_absolute_vindpm = true;
				bq25890_set_vindpm_absolute(chip, val->intval);
			} else {
				chip->use_absolute_vindpm = false;
				bq25890_set_vindpm_offset(chip,val->intval);
			}
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		pr_err("input suspend =%d\n",val->intval);
		if (chip->parallel_chgr_present) {
			bq25890_parallel_enable_charging(chip,!val->intval);
			bq25890_enable_hiz(chip, val->intval);
		}
		//if (chip->parallel_chgr_present && !val->intval)
		//	fuelsummary_collect_value(ID_SIC_STAT, 1);
		//else
		//	fuelsummary_collect_value(ID_SIC_STAT, 0);
		break;
	case POWER_SUPPLY_PROP_OTG_DISABLE_PL:
		pr_err("otg disable pl =%d\n", val->intval);
		bq25890_parallel_enable_charging(chip, !val->intval);
		bq25890_enable_hiz(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		chip->c_charger_temp_max = val->intval;
		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static int bq25890_parallel_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		return 0;
	}
}
static int bq25890_parallel_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct bq25890_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq25890_parallel_is_enabled(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_chgr_present)
			val->intval = chip->input_current_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->parallel_chgr_present;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_chgr_present)
			val->intval = chip->fastchg_current_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->parallel_chgr_present)
			val->intval = bq25890_get_prop_chg_status_ex(chip);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		//pr_err("parallel dump regs \n");
		//bq25890_dump_regs(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq25890_get_prop_batt_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_DUMP_REG:
		pr_err("parallel dump regs \n");
		bq25890_dump_regs(chip);
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHECK_SLAVE_CHARGER_STATUS:
		val->intval = bq25890_get_prop_batt_slave_charger_status(chip);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN_EXT;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
		val->intval = POWER_SUPPLY_PL_NON_STACKED_BATFET;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		val->intval = chip->c_charger_temp_max;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct power_supply_desc ac_psy_desc = {
	.name			= "smb-dc",
	.type			= POWER_SUPPLY_TYPE_MAINS,
	.properties		= bq25890_ac_props,
	.num_properties		= ARRAY_SIZE(bq25890_ac_props),
	.get_property		= bq25890_ac_get_property,
	.set_property		= bq25890_ac_set_property,
	.external_power_changed = bq25890_external_ac_changed,
};

static int bq25890_init_ac_psy(struct bq25890_chip *chip)
{
	struct power_supply_config ac_cfg = {};

	ac_cfg.drv_data = chip;
	ac_cfg.of_node = chip->dev->of_node;
	ac_cfg.supplied_to = bq25890_ac_supplied_to;
	ac_cfg.num_supplicants = ARRAY_SIZE(bq25890_ac_supplied_to);
	chip->ac_psy = devm_power_supply_register(chip->dev,
						   &ac_psy_desc,
						   &ac_cfg);
	if (IS_ERR(chip->ac_psy)) {
		pr_err("Couldn't register ac power supply\n");
		return PTR_ERR(chip->ac_psy);
	}

	return 0;
}

static const struct power_supply_desc parallel_psy_desc = {
	.name			= "parallel",
	.type			= POWER_SUPPLY_TYPE_PARALLEL,
	.properties		= bq25890_parallel_properties,
	.num_properties		= ARRAY_SIZE(bq25890_parallel_properties),
	.get_property		= bq25890_parallel_get_property,
	.set_property		= bq25890_parallel_set_property,
	.property_is_writeable	= bq25890_parallel_is_writeable,
};

static int bq25890_init_parallel_psy(struct bq25890_chip *chip)
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

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = bq25890_battery_properties,
	.num_properties = ARRAY_SIZE(bq25890_battery_properties),
	.get_property = bq25890_battery_get_property,
	.set_property = bq25890_battery_set_property,
	.property_is_writeable = bq25890_battery_is_writeable,
};

static int bq25890_init_batt_psy(struct bq25890_chip* chip)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	batt_cfg.drv_data = chip;
	batt_cfg.of_node = chip->dev->of_node;
	batt_cfg.supplied_to = bq25890_battery_supplied_to;
	batt_cfg.num_supplicants = ARRAY_SIZE(bq25890_battery_supplied_to);
	chip->batt_psy = devm_power_supply_register(chip->dev,
						   &batt_psy_desc,
						   &batt_cfg);
	if (IS_ERR(chip->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chip->batt_psy);
	}

	return rc;
}

static int bq25890_psy_register(struct bq25890_chip *chip)
{
    int ret;

	if (chip->primary_charger) {
		ret = bq25890_init_batt_psy(chip);
		if (ret) {
			dev_err(&chip->client->dev,
				"Unable to register batt_psy rc = %d\n", ret);
			return ret;
		}
	}

	if (chip->primary_charger) {
		ret = bq25890_init_ac_psy(chip);
	} else {
		ret = bq25890_init_parallel_psy(chip);
	}
	if (ret) {
		pr_err("bq25890_init_ac_psy failed\n");
		goto unregister_batt_psy;
	}

    return 0;

unregister_batt_psy:
	if (chip->primary_charger)
    	power_supply_unregister(chip->batt_psy);

    return ret;
}
static void bq25890_psy_unregister(struct bq25890_chip *chip)
{
    power_supply_unregister(chip->batt_psy);

	if (chip->primary_charger)
		power_supply_unregister(chip->ac_psy);
	else
		power_supply_unregister(chip->parallel_psy);
}

static ssize_t bq25890_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = sprintf(buf,"%s:\n","Charger 1");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq25890_read_reg(the_chip->client, addr, &val);
		if(ret == 0){
			len = sprintf(tmpbuf,"Reg[0x%.2x] = 0x%.2x\n",addr,val);
			memcpy(&buf[idx],tmpbuf,len);
			idx += len;
		}
	}

	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, bq25890_show_registers, NULL);

static struct attribute *bq25890_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq25890_attr_group = {
	.attrs = bq25890_attributes,
};
static ssize_t Show_CMCC_Attribute(struct device *dev,struct device_attribute *attr, char *buf)
{

	pr_err("[CMCC_Attribute] Show_CMCC_Attribute,bbk_cmcc_attribute=%d\n", bbk_cmcc_attribute);
	return sprintf(buf, "%d\n", bbk_cmcc_attribute);
}

static ssize_t Store_CMCC_Attribute(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{

	if(buf!=NULL && size!=0 && strstr(buf, "LG4")!= NULL)
		bbk_cmcc_attribute = true;
	else
		bbk_cmcc_attribute = false;

	pr_err("[CMCC_Attribute] Store_CMCC_Attribute ,bbk_cmcc_attribute =%d\n",bbk_cmcc_attribute);

	return size;
}

static DEVICE_ATTR(CMCC_Attribute,0664,Show_CMCC_Attribute,Store_CMCC_Attribute);
static struct attribute *cmcc_attrs[] = {
	&dev_attr_CMCC_Attribute.attr,
	NULL
};

static const struct attribute_group cmcc_attr_group = {
	.attrs = cmcc_attrs,
};

static int bq25890_hw_init(struct bq25890_chip *chip)
{

    int ret;

	bq25890_reset_watchdog_timer(chip);

	ret = bq25890_set_term_current(chip,chip->term_current_ma);
	if(ret < 0){
		dev_err(chip->dev,"Failed to set termination current:%d\n",ret);
		return ret;
	}
	ret = bq25890_set_chargevoltage(chip,chip->vbat_max_mv);
	if(ret < 0){
		dev_err(chip->dev,"Failed to set charge voltage:%d\n",ret);
		return ret;
	}

	ret = bq25890_set_prechg_current(chip,chip->pre_chg_current_ma);
	if(ret < 0){
		dev_err(chip->dev,"Failed to set pre chg current:%d\n",ret);
		return ret;
	}

	if (!chip->use_absolute_vindpm) {
		ret = bq25890_set_vindpm_offset(chip,chip->vindpm_offset_mv);
	} else {
		ret = bq25890_set_vindpm_absolute(chip,chip->vindpm_5v_thr_mv);
	}
	if(ret < 0){
		dev_err(chip->dev,"Failed to set vindpm offset:%d\n",ret);
		return ret;
	}

	bq25890_adc_start(chip,true);
	bq25890_adc_rate(chip,false);
	//bq25890_adc_rate(chip,true);

	ret = bq25890_pumpx_enable(chip,false);
	if(ret){
		dev_err(chip->dev,"Failed to disable pumpx:%d\n",ret);
		return ret;
	}
	ret = bq25890_maxc_enable(chip,false);
	if(ret){
		dev_err(chip->dev,"Failed to disable maxc:%d\n",ret);
		return ret;
	}

	ret = bq25890_hvdcp_enable(chip,chip->hvdcp_enable);
	if(ret){
		dev_err(chip->dev,"Failed to enable hvdcp:%d\n",ret);
		return ret;
	}
	ret = bq25890_set_ir_comp_resister(chip, chip->ircomp_mom);
	if (ret) {
		pr_err("failed to set ir compensation resister\n");
		return ret;
	}

	ret = bq25890_set_vclamp_mv(chip, chip->vclamp_mv);
	if (ret) {
		pr_err("failed to set ir vclamp voltage\n");
		return ret;
	}

	ret = bq25890_set_fastchg_current(chip,chip->cfg_fastchg_current_ma);
	if(ret < 0){
		dev_err(chip->dev,"Failed to set charger charge current:%d\n",ret);
		return ret;
	}

	if (chip->primary_charger) {
		bq25890_watchdog_timer_enable(chip,true);
		//bq25890_set_boost_ilim(chip,chip->otg_boost_ilim);
	} else {
		bq25890_watchdog_timer_enable(chip,false);
		//bq25890_termination_enable(chip,false);
	}

	ret = bq25890_enable_charging(chip,chip->chg_enabled);
	if(ret < 0){
		dev_err(chip->dev,"Failed to enable charger:%d\n",ret);
		return ret;
	}
	//bq25890_set_watchdog_timer(chip,40);

    return ret;
}
static int bq25890_detect_device(struct bq25890_chip* chip)
{
    int ret;
    u8 data;

    ret = bq25890_read_reg(chip->client, BQ25890_REG_14, &data);
    if(ret == 0){
        chip->part_no = (data & BQ25890_PN_MASK) >> BQ25890_PN_SHIFT;
        chip->revision = (data & BQ25890_DEV_REV_MASK) >> BQ25890_DEV_REV_SHIFT;
    }
    return ret;
}
static void bq25890_adapter_in_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip,
										adapter_in_work);
	int usb_supply_type;
	union power_supply_propval ret = {0,};

	if (!wake_lock_active(&chip->irq_wake_lock)) {
		pr_debug("get irq lock\n");
		wake_lock(&chip->irq_wake_lock);
	}

	bq25890_hw_init(chip);
	msleep(200);

	pr_debug("triggered\n");
	usb_supply_type = get_usb_supply_type(chip);

	if (usb_supply_type == POWER_SUPPLY_TYPE_USB ||
		usb_supply_type == POWER_SUPPLY_TYPE_USB_CDP){
		if (chip->usbsel_gpio) {
		   gpio_direction_output(chip->usbsel_gpio,0);
		   pr_debug("usbsel=%d\n",gpio_get_value(chip->usbsel_gpio));
		}
	} else {
		/*bq detected maybe error,float charger reported,as default charger*/
		usb_supply_type = POWER_SUPPLY_TYPE_USB_DCP;
	}

	if (chip->usb_psy) {
		pr_debug("setting usb_type = %d,usb_present=%d\n",
				usb_supply_type,chip->usb_present);
		ret.intval = usb_supply_type;
		power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
		ret.intval = chip->usb_present;
		power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_PRESENT, &ret);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_PRESENT,chip->usb_present);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,true);
	}

	chip->use_absolute_vindpm = true;
	chip->check_vbus_count = 0;
	bq25890_power_init(chip);

	bq25890_stay_awake(chip,PM_IRQ_CHECK);
	cancel_delayed_work_sync(&chip->monitor_work);
	schedule_delayed_work(&chip->monitor_work,msecs_to_jiffies(0));
}

static void bq25890_adapter_out_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip,
										adapter_out_work);
	union power_supply_propval ret = {0,};

	if (chip->usbsel_gpio) {
		gpio_direction_output(chip->usbsel_gpio,1);
		pr_debug("usbsel=%d\n",gpio_get_value(chip->usbsel_gpio));
	}

	chip->use_absolute_vindpm = true;
	if (chip->use_absolute_vindpm) {
		bq25890_set_vindpm_absolute(chip,chip->vindpm_5v_thr_mv);
	} else {
		bq25890_set_vindpm_offset(chip,chip->vindpm_offset_mv);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_VINDPM, chip->vindpm_offset_mv);
	}

	bq25890_hvdcp_enable(chip,true);
	bq25890_reset_chip(chip);
	bq25890_watchdog_timer_enable(chip,false);
	bq25890_power_deinit(chip);
	chip->hvdcp_type = HVDCP_TYPE_UNKOWN;

	if (chip->usb_psy) {
		pr_debug("setting usb psy present = %d\n",chip->usb_present);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_CHARGING_ENABLED,false);
		set_property_on_parallel(chip,POWER_SUPPLY_PROP_PRESENT,chip->usb_present);
		ret.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
		ret.intval = chip->usb_present;
		power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_PRESENT, &ret);
		chip->usb_psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
		power_supply_changed(chip->usb_psy);
		power_supply_changed(chip->batt_psy);
	}
	cancel_delayed_work_sync(&chip->monitor_work);
	if (wake_lock_active(&chip->irq_wake_lock)){
		pr_info("disable charger release irq lock\n");
		wake_unlock(&chip->irq_wake_lock);
	}
}
static int bq25890_adjust_ico_func(struct bq25890_chip* chip)
{
	int tmp_ico = chip->ico_ma;
	if(tmp_ico > 1700)
		tmp_ico = 1700;
	else if(tmp_ico > 1200)
		tmp_ico -= 50;
	pr_debug("change ico %d to %d \n",chip->ico_ma,tmp_ico);
	return tmp_ico;
}

static void bq25890_otg_monitor_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip, otg_monitor_work.work);
	int parallel_status;

	bq25890_dump_regs(chip);

	pr_debug("## otg work ##\n");

	//dump parallel reg
	get_property_from_parallel(chip,POWER_SUPPLY_PROP_DUMP_REG,&parallel_status);
	pr_debug("parallel_status = 0x%02X \n",parallel_status);

	schedule_delayed_work(&chip->otg_monitor_work,msecs_to_jiffies(10000));
}

#define ADJUST_HVDCP_RETRIES_MAX 3
#define CHECK_USB_UNPLUGGED_MAX 20
#define SWITCH_DCP_RETRIES_MAX  3
#define RESUME_HVDCP_RETRIES_MAX 3
static void bq25890_monitor_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip, monitor_work.work);
	int vbus_mv,ibat_ma,vbat_mv,vsys_mv;
	int is_hvdcp,hvdcp_type,delay = 10000;
	int usb_supply_type,usbsel = 0,en_gpio = 0;
	bool dpdm_doned = false,vbus_attached;
	//static int count = 0;
	static int retries = 0;
	static int last_type = 0;
	static int first_type = 0;
	int parallel_status = 0;

	/* as slave-driver , only dump regs */
	if (!chip->parallel_chgr_present) {
		pr_debug("!usb_present\n");
		return;
	}
	/*bq25890_dump_regs(chip);*/
	goto restart;
	/* end */

	vbus_attached = bq25890_is_vbus_attached(chip);

	if (!vbus_attached) {
		pr_debug("usb_present=%d hvdcp_enable=%d\n",vbus_attached,chip->hvdcp_enable);
		chip->check_vbus_count = 0;
		retries = 0;
		last_type = 0;
		first_type = 0;
		chip->resume_hvdcp_count = 0;
		chip->hvdcp_type = HVDCP_TYPE_UNKOWN;
		bq25890_hvdcp_enable(chip,chip->hvdcp_enable);
		schedule_work(&chip->irq_work);
		bq25890_relax(chip,PM_IRQ_CHECK);
		return;
	}
	pr_debug("cont=%d\n",chip->check_vbus_count);
	if (chip->primary_charger && chip->check_vbus_count < CHECK_USB_UNPLUGGED_MAX) {
		chip->check_vbus_count++;
		delay = 500;
		goto restart;
	} else {
		delay = 10000;
	}

	if (chip->usbsel_gpio)
		usbsel = gpio_get_value(chip->usbsel_gpio);
	if (chip->en_gpio > 0)
		en_gpio = gpio_get_value(chip->en_gpio);
	bq25890_adc_start(chip,true);
	msleep(80);
	vsys_mv = bq25890_adc_read_sys_volt(chip);
	vbat_mv = bq25890_adc_read_battery_volt(chip);
	vbus_mv = bq25890_adc_read_vbus_volt(chip);
	ibat_ma = bq25890_get_prop_batt_current_now(chip);
	bq25890_get_usbid_voltage(chip);

	is_hvdcp = bq25890_is_hvdcp(chip);
	dpdm_doned = bq25890_check_force_dpdm_done(chip);
	hvdcp_type = bq25890_get_hvdcp_type(chip);
	usb_supply_type = get_usb_supply_type(chip);

	chip->ibat_ma = ibat_ma;

	bq25890_dump_regs(chip);
	bq25890_reset_watchdog_timer(chip);
	bq25890_start_recharging(chip);

	pr_debug("vbus_mv=%d vsys_mv=%d vbat_mv=%d IR_vbat=%d ibat_ma=%d soc=%d is_hvdcp=%d "
			"hvdcp_type=%d dpdm_doned=%d usbsel=%d en_gpio=%d ico_ma=%d\n",
			vbus_mv,vsys_mv,vbat_mv,chip->vbat_mv,ibat_ma,chip->cal_soc,is_hvdcp,hvdcp_type,
			dpdm_doned,usbsel,en_gpio,chip->ico_ma);

	if (chip->pd_state == POWER_DETECT_STATE_DONE && chip->use_absolute_vindpm &&
					usb_supply_type == POWER_SUPPLY_TYPE_USB_DCP && chip->hvdcp_type == HVDCP_TYPE_5V) {
		bq25890_adjust_dpm_limit(chip);
	}

	//dump parallel reg
	get_property_from_parallel(chip,POWER_SUPPLY_PROP_DUMP_REG,&parallel_status);
	pr_debug("parallel_status = 0x%02X \n",parallel_status);

	//soc >= 85% , 9v->5v
	if(chip->hvdcp_to_5V_enable){
		if(chip->hvdcp_to_5V_flag && (hvdcp_type == HVDCP_TYPE_5V) && (chip->hvdcp_type == HVDCP_TYPE_9V)){
			chip->hvdcp_type = HVDCP_TYPE_5V;
		}
		if(chip->hvdcp_type == HVDCP_TYPE_9V){
			if(chip->ui_soc >= chip->hvdcp_to_5V_soc){
				pr_err("soc >= %d : hvdcp_type=%d,adjust to 5v\n",chip->hvdcp_to_5V_soc,chip->hvdcp_type);
				bq25890_hvdcp_enable(chip,false);
				bq25890_force_dpdm(chip);
				msleep(800);
				bq25890_force_dpdm(chip);
				chip->hvdcp_to_5V_flag = true;
			}
		}
	}

	//rerun 12v->5v
	if (last_type == HVDCP_TYPE_12V && hvdcp_type != HVDCP_TYPE_12V ) {
		pr_debug("adjust successful\n");
		last_type = 0;
		bq25890_hvdcp_enable(chip,true);
		usb_supply_type = get_usb_supply_type(chip);
		chip->usb_psy_type = usb_supply_type;
		if (chip->pd_state == POWER_DETECT_STATE_DONE) {
			bq25890_power_init(chip);
		}
		delay = 5000;
	}
	if (is_hvdcp && hvdcp_type == HVDCP_TYPE_12V) {
		pr_debug("hvdcp_type=%d,adjust to 5v\n",hvdcp_type);
		bq25890_hvdcp_enable(chip,false);
		bq25890_force_dpdm(chip);
		msleep(800);
		bq25890_force_dpdm(chip);
		last_type = hvdcp_type;
		first_type = hvdcp_type;
		if (++retries >= ADJUST_HVDCP_RETRIES_MAX) {
			retries = 0;
			pr_debug("adjust failed\n");
		}
		delay = 2000;
	}

	/*rerun for dcp:always 5v*/
	if (!chip->hvdcp_to_5V_flag && first_type != HVDCP_TYPE_12V && chip->hvdcp_type == HVDCP_TYPE_5V) {
		if (hvdcp_type == HVDCP_TYPE_5V) {
			if (!chip->dcp_retries_flag) {
				chip->dcp_retries++;
				if (chip->dcp_retries == SWITCH_DCP_RETRIES_MAX) {
					bq25890_hvdcp_enable(chip,true);
					bq25890_force_dpdm(chip);
					msleep(800);
					bq25890_force_dpdm(chip);
					msleep(800);
					bq25890_force_dpdm(chip);

					pr_debug("## rerun dcp ##\n");
					//chip->dcp_retries = 0;
					//chip->dcp_retries_flag = true;
					delay = 2000;
				}else if (chip->dcp_retries > SWITCH_DCP_RETRIES_MAX){
					chip->input_current_ma = 0;
					bq25890_set_input_current_user(chip,chip->ico_ma);
					chip->dcp_retries = 0;
					chip->dcp_retries_flag = true;
					delay = 2000;
				}
			}
		} else if (hvdcp_type == HVDCP_TYPE_9V){
			chip->hvdcp_type = HVDCP_TYPE_9V;
			chip->dcp_retries = 0;
			chip->dcp_retries_flag = true;
		}else if (hvdcp_type == HVDCP_TYPE_12V){
			chip->hvdcp_type = HVDCP_TYPE_12V;
			chip->dcp_retries = 0;
			chip->dcp_retries_flag = true;
		}
	}

	//rerun 9v->5v->9v
	if (!chip->hvdcp_to_5V_flag && !chip->fb_on_ajust_adapt && chip->hvdcp_type == HVDCP_TYPE_9V && vbus_mv < 6800 &&
			!chip->vbus_hized && chip->pd_state == POWER_DETECT_STATE_DONE) {
		if (chip->resume_hvdcp_count < RESUME_HVDCP_RETRIES_MAX) {
			chip->resume_hvdcp_count++;
			pr_debug("hvdcp_type=%d,adjust 9v->5v->9v!\n",hvdcp_type);
			if(!chip->recharging)
				chip->ico_ma = bq25890_adjust_ico_func(chip);
			bq25890_set_input_current_user(chip,chip->ico_ma);
			bq25890_hvdcp_enable(chip,true);
			bq25890_force_dpdm(chip);
			msleep(800);
			bq25890_force_dpdm(chip);
			delay = 2000;
		}
	}else{
		chip->resume_hvdcp_count = 0;
	}

restart:
	schedule_delayed_work(&chip->monitor_work,msecs_to_jiffies(delay));
}
static void bq25890_chgdone_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip, chgdone_work.work);
	int work_period_ms = msecs_to_jiffies(5000);
	int parallel_status;
	bool the_status = false;

	if (!chip->usb_present) {
		pr_debug("usb_present is not exist!\n");
		return;
	}

	if(!chip->parallel_chgr_enabled){
		pr_debug("parallel_chgr_enabled disable!\n");
		if(bq25890_get_prop_chg_status_ex(chip) == POWER_SUPPLY_STATUS_FULL){
			pr_debug("only main ic charge done!\n");
			the_status = true;
		}else{
			pr_debug("not real charge done!\n");
			return;
		}
	}

	get_property_from_parallel(chip,POWER_SUPPLY_PROP_STATUS,&parallel_status);
	pr_warn("## parallel_status = %d ##\n",parallel_status);

	if(parallel_status == POWER_SUPPLY_STATUS_FULL ||
		parallel_status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
		parallel_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		the_status){
		chip->batt_full = true;
		power_supply_changed(chip->batt_psy);
		pr_warn("charge done!!\n");
		return;
	}
	schedule_delayed_work(&chip->chgdone_work,round_jiffies_relative(work_period_ms));
}

/*extern void charger_connect_judge(char on_or_off);//add by zhj*/

static void bq25890_charger_irq_workfunc(struct work_struct *work)
{
	struct bq25890_chip *chip = container_of(work, struct bq25890_chip, irq_work);
	u8 status = 0,vbus_stat = 0;
	u8 fault = 0;
	int ret;
	bool chg_done = false;

	/* Read STATUS and FAULT registers */
	ret = bq25890_read_reg(chip->client, BQ25890_REG_0B, &status);
	if (ret) {
		pr_err("fail to read reg=%d,ret=%d\n",BQ25890_REG_0B,ret);
		return;
	}

	ret = bq25890_read_reg(chip->client, BQ25890_REG_11, &vbus_stat);
	if (ret) {
		pr_err("fail to read reg=%d,ret=%d\n",BQ25890_REG_11,ret);
		return;
	}

	ret = bq25890_read_reg(chip->client, BQ25890_REG_0C, &fault);
	if (ret) {
		pr_err("fail to read reg=%d,ret=%d\n",BQ25890_REG_0C,ret);
		return;
	}

	if (!chip->primary_charger) {
		pr_err("vbus is %s\n", (vbus_stat & BQ25890_VBUS_GD_MASK) ?
				"attached" : "detached");
		return;
	}

	if (vbus_stat & BQ25890_VBUS_GD_MASK && !(chip->status & BQ25890_STATUS_ATTACHED)) {
		chip->status |= BQ25890_STATUS_ATTACHED;
		chip->usb_present = true;
		/*charger_connect_judge(1);	//add by zhj*/
		if (chip->usbsel_gpio) {
			gpio_direction_output(chip->usbsel_gpio,1);
			pr_debug("usbsel=%d\n",gpio_get_value(chip->usbsel_gpio));
		}
		schedule_work(&chip->adapter_in_work);
		pr_debug("vbus attached\n");
	} else if (!(vbus_stat & BQ25890_VBUS_GD_MASK) && (chip->status & BQ25890_STATUS_ATTACHED)) {
		chip->status &=~BQ25890_STATUS_ATTACHED;

		/*charger_connect_judge(0);	//add by zhj*/

		chip->adb_index = 0;
		chip->adb_timer = 0;
		chip->ibat_ma = 0;
		chip->nor_timer = 0;
		chip->nor_index = 0;
		chip->sw_timer = 0;
		chip->sw_index = 0;
		chip->chg_done = 0;
		chip->adjust_dpm_flag = 0;
		chip->dcp_retries = 0;
		chip->dcp_retries_flag = false;
		chip->recharging = false;
		chip->fb_on_ajust_adapt = 0;
		chip->resume_hvdcp_count = 0;
		chip->hvdcp_to_5V_flag = false;

		chip->batt_full = false;
		chip->batt_full_flag = false;
		chip->usb_psy_type = 0;
		clear_bit(bq25890_CHG_TIMEOUT_STATUS, &chip->psy_status);
		cancel_delayed_work_sync(&chip->chgdone_work);
		cancel_delayed_work_sync(&chip->rerun_base_fb_on_work);

		chip->usb_present = false;
		bq25890_enable_hiz(chip,false);
		//chip->vbus_hized = false;
		bq25890_termination_enable(chip,true);

		if (chip->use_absolute_vindpm) {
			bq25890_set_vindpm_absolute(chip,chip->vindpm_5v_thr_mv);
		}

		schedule_work(&chip->adapter_out_work);
		pr_debug("vbus not attached\n");
	}
    if((status & BQ25890_PG_STAT_MASK) && !(chip->status & BQ25890_STATUS_PG)){
        chip->status |= BQ25890_STATUS_PG;
		cancel_delayed_work_sync(&chip->rerun_apsd_work);
		bq25890_stay_awake(chip,PM_IRQ_CHECK);
		schedule_delayed_work(&chip->rerun_apsd_work,
				msecs_to_jiffies(3000));
                /* @vivo add for Dual-engine */
                power_supply_changed(chip->batt_psy);
		pr_debug("vbus power good\n");
    } else if(!(status & BQ25890_PG_STAT_MASK) && (chip->status & BQ25890_STATUS_PG)){
        chip->status &=~BQ25890_STATUS_PG;
		pr_debug("vbus not power good\n");
    }

	pr_debug("vbus_stat=0x%02X status=0x%02X fault=0x%02X usb_present=%d\n",
		vbus_stat,status,fault,chip->usb_present);

    if(fault && !(chip->status & BQ25890_STATUS_FAULT)){
        chip->status |= BQ25890_STATUS_FAULT;
    } else if(!fault &&(chip->status &BQ25890_STATUS_FAULT)){
        chip->status &=~BQ25890_STATUS_FAULT;
    }

	chg_done = (status & BQ25890_CHRG_STAT_MASK) == 0x18 ? true : false;
	pr_debug("chg_done=%d\n",chg_done);
	if (chip->chg_done != chg_done) {
		mutex_lock(&chip->chgdone_lock);
		if (chg_done) {
			pr_debug("primary chg done,in hiz mode\n");
			bq25890_termination_enable(chip,false);
			bq25890_enable_charging(chip,false);
			bq25890_enable_charging(chip,true);
			bq25890_enable_hiz(chip,true);
			chip->recharging = false;
			cancel_delayed_work_sync(&chip->chgdone_work);
			if(1){
				chip->batt_full = true;
				power_supply_changed(chip->batt_psy);
				pr_warn("charge done!!\n");
			}else{
				pr_warn("check parallel chgdone status in chgdone_work ! \n");
				schedule_delayed_work(&chip->chgdone_work, 0);
			}
		}
		chip->chg_done = chg_done;
		mutex_unlock(&chip->chgdone_lock);
	}

    if(((status & BQ25890_VBUS_STAT_MASK) == 0) && (chip->status & BQ25890_STATUS_PLUGIN)){// plug out
        //pr_debug("adapter removed\n");
        chip->status &= ~BQ25890_STATUS_PLUGIN;
		//schedule_work(&chip->adapter_out_work);
    } else if((status & BQ25890_VBUS_STAT_MASK) && !(chip->status & BQ25890_STATUS_PLUGIN)){
       // pr_debug("adapter plugged in\n");
        chip->status |= BQ25890_STATUS_PLUGIN;
		chip->vbus_type = (status & BQ25890_VBUS_STAT_MASK) >> BQ25890_VBUS_STAT_SHIFT;
		//schedule_work(&chip->adapter_in_work);
    }

}


static irqreturn_t bq25890_charger_interrupt(int irq, void *data)
{
	struct bq25890_chip *chip = data;

	schedule_work(&chip->irq_work);
	return IRQ_HANDLED;
}

static int bq25890_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	struct bq25890_chip *chip = rdev_get_drvdata(rdev);
	bq25890_set_otg(chip,true);
	return 0;
}

static int bq25890_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	struct bq25890_chip *chip = rdev_get_drvdata(rdev);
	bq25890_set_otg(chip,false);
	return 0;
}

static int bq25890_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct bq25890_chip *chip = rdev_get_drvdata(rdev);
	ret = bq25890_is_otg_mode(chip);
	return ret;
}

static struct regulator_ops bq25890_chg_otg_reg_ops = {
	.enable		= bq25890_chg_otg_regulator_enable,
	.disable	= bq25890_chg_otg_regulator_disable,
	.is_enabled	= bq25890_chg_otg_regulator_is_enable,
};
static int bq25890_regulator_init(struct bq25890_chip *chip)
{

	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	int ret = 0;

	if (!chip->is_support_otg) {
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
		chip->otg_vreg.rdesc.ops = &bq25890_chg_otg_reg_ops;
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
static int bq25890_gpio_init(struct bq25890_chip *chip)
{
	int ret = 0;

	if (chip->en_gpio > 0) {
		ret = gpio_request(chip->en_gpio,"bq25890_en_gpio");
		if (ret) {
			pr_err("failed to request en_gpio,rc=%d\n",ret);
			return ret;
		}
	}
	if (chip->usbsel_gpio > 0) {
		ret = gpio_request(chip->usbsel_gpio,"bq25890_usbsel_gpio");
		if (ret) {
			pr_err("failed to request usbsel_gpio,rc=%d\n",ret);
			return ret;
		}
	}

	if (!chip->irq_gpio) {
		pr_err("failed to request irq_gpio\n");
		return ret;
	}
	ret = gpio_request_one(chip->irq_gpio, GPIOF_DIR_IN,"bq25890_int");
	if (ret) {
		pr_err("failed to request irq_gpio\n");
		return ret;
	}
	chip->irq = gpio_to_irq(chip->irq_gpio);
	return ret;
}

static int adb_get_first_input_current(void *data, u64 *val)
{
	struct bq25890_chip *chip = data;
	*val = chip->adb_first_input_current;
	return 0;
}
static int adb_set_first_input_current(void *data, u64 val)
{
	struct bq25890_chip *chip = data;
	chip->adb_first_input_current = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(adb_first_input_current_debugfs_ops, adb_get_first_input_current, adb_set_first_input_current, "%lld\n");

static int adb_get_second_input_current(void *data, u64 *val)
{
	struct bq25890_chip *chip = data;
	*val = chip->adb_second_input_current;
	return 0;
}
static int adb_set_second_input_current(void *data, u64 val)
{
	struct bq25890_chip *chip = data;
	chip->adb_second_input_current = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(adb_second_input_current_debugfs_ops, adb_get_second_input_current, adb_set_second_input_current, "%lld\n");

static int adb_get_first_ibat_current(void *data, u64 *val)
{
	struct bq25890_chip *chip = data;
	*val = chip->adb_first_ibat_current[0];
	return 0;
}
static int adb_set_first_ibat_current(void *data, u64 val)
{
	struct bq25890_chip *chip = data;
	int i;
	static int index = 0;
	if(val == 1){
		for(i = 0; i<6; i++)
			chip->adb_first_ibat_current[i] = 0;
		index = 0;
		return 0;
	}
	if(index < 6){
		chip->adb_first_ibat_current[index++] = val;
		//pr_err("chip->adb_first_ibat_current[%d]=%d \n",index-1,chip->adb_first_ibat_current[index-1]);
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(adb_first_ibat_current_debugfs_ops, adb_get_first_ibat_current, adb_set_first_ibat_current, "%lld\n");

static int adb_get_second_ibat_current(void *data, u64 *val)
{
	struct bq25890_chip *chip = data;
	*val = chip->adb_second_ibat_current[0];
	return 0;
}
static int adb_set_second_ibat_current(void *data, u64 val)
{
	struct bq25890_chip *chip = data;
	int i;
	static int index = 0;
	if(val == 1){
		for(i = 0; i<6; i++)
			chip->adb_second_ibat_current[i] = 0;
		index = 0;
		return 0;
	}
	if(index < 6){
		chip->adb_second_ibat_current[index++] = val;
		//pr_err("chip->adb_second_ibat_current[%d]=%d \n",index-1,chip->adb_second_ibat_current[index-1]);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(adb_second_ibat_current_debugfs_ops, adb_get_second_ibat_current, adb_set_second_ibat_current, "%lld\n");

static int adb_get_enable_status(void *data, u64 *val)
{
	struct bq25890_chip *chip = data;
	*val = chip->adb_set_current_enable;
	return 0;
}
static int adb_set_enable_status(void *data, u64 val)
{
	struct bq25890_chip *chip = data;
	chip->adb_set_current_enable = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(adb_set_current_enable_debug_ops, adb_get_enable_status, adb_set_enable_status, "%lld\n");

static void bq25890_create_debugfs_entries(struct bq25890_chip *chip)
{
	struct dentry *dent;

	chip->debug_root = debugfs_create_dir("adb-charger-debug", NULL);
	if (!chip->debug_root)
		pr_err("Couldn't create debug dir\n");

	if (chip->debug_root) {

		dent = debugfs_create_file("adb_set_current_enable", S_IRUGO | S_IWUSR,
					  chip->debug_root, chip,
					  &adb_set_current_enable_debug_ops);
		if (!dent){
			pr_err("Couldn't create adb_set_current_enable debug file\n");
			goto error;
		}

		dent = debugfs_create_file("adb_first_ibat_current", S_IRUGO | S_IWUSR,
					  chip->debug_root, chip,
					  &adb_first_ibat_current_debugfs_ops);
		if (!dent){
			pr_err("Couldn't create adb_first_ibat_current debug file\n");
			goto error;
		}

		dent = debugfs_create_file("adb_second_ibat_current", S_IRUGO | S_IWUSR,
					  chip->debug_root, chip,
					  &adb_second_ibat_current_debugfs_ops);
		if (!dent){
			pr_err("Couldn't create adb_second_ibat_current debug file\n");
			goto error;
		}

		dent = debugfs_create_file("adb_first_input_current", S_IRUGO | S_IWUSR,
					  chip->debug_root, chip,
					  &adb_first_input_current_debugfs_ops);
		if (!dent){
			pr_err("Couldn't create adb_first_input_current debug file\n");
			goto error;
		}

		dent = debugfs_create_file("adb_second_input_current", S_IRUGO | S_IWUSR,
					  chip->debug_root, chip,
					  &adb_second_input_current_debugfs_ops);
		if (!dent){
			pr_err("Couldn't create adb_second_input_current debug file\n");
			goto error;
		}
	}

	return;
error:
	debugfs_remove_recursive(chip->debug_root);
	chip->debug_root = 0;
}

#define BATTERY_DEFAULT_FCC_1C_MA		2400
#define BATTERY_DEFAULT_IRCOMP_MOM		100
#define BATTERY_DEFAULT_CORE_ACC		150
#define NORMAL_TEMP_DEFAULT_TIMER_LIMIT 60
#define SWITCH_DEFAULT_TIMER_LIMIT 		60
static int bq25890_parse_dt(struct device_node *dev_node, struct bq25890_chip * chip)
{
    int ret = 0;
	int i,count = 0;
	int cur_temp[4];

	if (!dev_node) {
		dev_err(&chip->client->dev, "device tree info. missing\n");
		return -EINVAL;
	}
	chip->irq_gpio = of_get_named_gpio(dev_node, "ti,int-gpio", 0);
	if (chip->irq_gpio < 0) {
		pr_err("failed to get int-gpio.\n");
		/*return chip->irq_gpio;*/
	}

	chip->primary_charger = of_property_read_bool(dev_node, "bbk,primary_charger");
	if (!chip->primary_charger) {
		chip->en_gpio = of_get_named_gpio(dev_node, "bbk,en-gpio", 0);
		pr_debug("en_gpio=%d",chip->en_gpio);
		if (chip->en_gpio < 0) {
			pr_err("failed to get en_gpio.\n");
			//return chip->en_gpio;
		}
	}

	/* read the bms power supply name */
	ret = of_property_read_string(dev_node, "bbk,bms-psy-name",
						&chip->bms_psy_name);
	if (ret)
		chip->bms_psy_name = NULL;


	if (chip->primary_charger){
		chip->usbsel_gpio = of_get_named_gpio(dev_node, "bbk,usbsel-gpio", 0);
		if (chip->usbsel_gpio < 0) {
			pr_err("failed to get usbsel-gpio.\n");
			chip->usbsel_gpio = 0;
		}

		ret = of_property_read_u32(dev_node, "vivo,otg-boost-vlim", &chip->otg_boost_vlim);
		if (ret < 0)
			chip->otg_boost_vlim = 4998;

		ret = of_property_read_u32(dev_node, "vivo,otg-boost-ilim", &chip->otg_boost_ilim);
		if (ret < 0)
			chip->otg_boost_ilim = 1400;

		chip->disable_parallel_charger = of_property_read_bool(dev_node,
						"vivo,disable-parallel-charger");
	}

	chip->hvdcp_to_5V_enable = of_property_read_bool(dev_node,
							"vivo,hvdcp-to-5V-enable");
	ret = of_property_read_u32(dev_node, "vivo,hvdcp-to-5V-soc", &chip->hvdcp_to_5V_soc);
	if (ret < 0)
		chip->hvdcp_to_5V_soc = 85;

	chip->chg_enabled = !(of_property_read_bool(dev_node,
						"bbk,charging-disabled"));

	chip->use_absolute_vindpm = of_property_read_bool(dev_node, "ti,use-absolute-vindpm");

	chip->hvdcp_enable = of_property_read_bool(dev_node, "ti,hvdcp-enable");
	//fuelsummary_of_property_put("ti,hvdcp-enable", PARAMS_TYPE_BOOL, &(chip->hvdcp_enable));

	chip->is_support_otg = 	of_property_read_bool(dev_node, "vivo,support-otg");

	ret = of_property_read_u32(dev_node, "vivo,batt-1C-current-ma", &chip->batt_1C_current_ma);
	if (ret < 0)
		chip->batt_1C_current_ma = BATTERY_DEFAULT_FCC_1C_MA;

	ret = of_property_read_u32(dev_node, "vivo,batt-core-acc", &chip->batt_core_acc);
	if (ret < 0)
		chip->batt_core_acc = BATTERY_DEFAULT_CORE_ACC;

	ret = of_property_read_u32(dev_node, "vivo,normal-temp-timer-limit", &chip->nor_timer_limit);
	if (ret < 0)
		chip->nor_timer_limit = NORMAL_TEMP_DEFAULT_TIMER_LIMIT;

	chip->nor_set_current_enable = of_property_read_bool(dev_node, "vivo,normal-temp-set-current-enable");

	if (chip->nor_set_current_enable && of_get_property(dev_node, "vivo,normal-temp-chg-current-level", &count)){
		count /= sizeof(int);
		if(count == 4){
			ret = of_property_read_u32_array(dev_node, "vivo,normal-temp-chg-current-level",
					cur_temp, count);
			if (ret < 0) {
				pr_err("%s get normal temp chg current level failed %d\n", __func__, __LINE__);
			}
			for (i = 0; i < count/2; i++) {
				chip->nor_first_ibat_current[i] = cur_temp[i*2];
				chip->nor_second_ibat_current[i]= cur_temp[i*2+1];
				pr_debug("nor_first_ibat_current[%d]=%d, nor_second_ibat_current[%d]=%d! \n",
							i,chip->nor_first_ibat_current[i],i,chip->nor_second_ibat_current[i]);
			}
		}else{
			pr_err("%s count=%d err!\n", __func__,count);
		}
	}

	ret = of_property_read_u32(dev_node, "vivo,switch-timer-limit", &chip->sw_timer_limit);
	if (ret < 0)
		chip->sw_timer_limit = SWITCH_DEFAULT_TIMER_LIMIT;

	chip->sw_set_current_enable = of_property_read_bool(dev_node, "vivo,switch-set-current-enable");

	if (chip->sw_set_current_enable && of_get_property(dev_node, "vivo,switch-chg-current-level", &count)){
		count /= sizeof(int);
		if(count == 4){
			ret = of_property_read_u32_array(dev_node, "vivo,switch-chg-current-level",
					cur_temp, count);
			if (ret < 0) {
				pr_err("%s get switch chg current level failed %d\n", __func__, __LINE__);
			}
			for (i = 0; i < count/2; i++) {
				chip->sw_first_ibat_current[i] = cur_temp[i*2];
				chip->sw_second_ibat_current[i]= cur_temp[i*2+1];
				pr_debug("sw_first_ibat_current[%d]=%d, sw_second_ibat_current[%d]=%d! \n",
							i,chip->sw_first_ibat_current[i],i,chip->sw_second_ibat_current[i]);
			}
		}else{
			pr_err("%s count=%d err!\n", __func__,count);
		}
	}

	chip->fbon_set_current_enable = of_property_read_bool(dev_node, "vivo,fbon-set-current-enable");

	if (chip->fbon_set_current_enable && of_get_property(dev_node, "vivo,fbon-chg-current-level", &count)){
		count /= sizeof(int);
		if(count == 2){
			ret = of_property_read_u32_array(dev_node, "vivo,fbon-chg-current-level",
					cur_temp, count);
			if (ret < 0) {
				pr_err("%s get fbon chg current level failed %d\n", __func__, __LINE__);
			}
			for (i = 0; i < count/2; i++) {
				chip->fbon_first_ibat_current[i] = cur_temp[i*2];
				chip->fbon_second_ibat_current[i]= cur_temp[i*2+1];
				pr_debug("fbon_first_ibat_current[%d]=%d, fbon_second_ibat_current[%d]=%d! \n",
							i,chip->fbon_first_ibat_current[i],i,chip->fbon_second_ibat_current[i]);
			}
		}else{
			pr_err("fbon %s count=%d err!\n", __func__,count);
		}
	}

	chip->calling_set_current_enable = of_property_read_bool(dev_node, "vivo,calling-set-current-enable");

	if (chip->calling_set_current_enable && of_get_property(dev_node, "vivo,calling-chg-current-level", &count)){
		count /= sizeof(int);
		if(count == 2){
			ret = of_property_read_u32_array(dev_node, "vivo,calling-chg-current-level",
					cur_temp, count);
			if (ret < 0) {
				pr_err("%s get calling chg current level failed %d\n", __func__, __LINE__);
			}
			for (i = 0; i < count/2; i++) {
				chip->calling_first_ibat_current[i] = cur_temp[i*2];
				chip->calling_second_ibat_current[i]= cur_temp[i*2+1];
				pr_debug("calling_first_ibat_current[%d]=%d, calling_second_ibat_current[%d]=%d! \n",
							i,chip->calling_first_ibat_current[i],i,chip->calling_second_ibat_current[i]);
			}
		}else{
			pr_err("calling %s count=%d err!\n", __func__,count);
		}
	}

	ret = of_property_read_u32(dev_node, "vivo,batt-ircomp-mom", &chip->batt_ircomp_mom);
	if (ret < 0)
		chip->batt_ircomp_mom = BATTERY_DEFAULT_IRCOMP_MOM;

	//adjust dpm according to battery voltage
	chip->adjust_dpm_enable = of_property_read_bool(dev_node,
											"vivo,adjust-dpm-enable");

	ret = of_property_read_u32(dev_node, "ti,ircomp-mom",
				   &(chip->ircomp_mom));
	if (ret) {
		pr_err("Unable to read ircomp-mom.\n");
		chip->ircomp_mom = 0;
	}

	ret = of_property_read_u32(dev_node, "ti,vclamp-mv",
				   &(chip->vclamp_mv));
	if (ret) {
		pr_err("Unable to read vclamp_mvs.\n");
		chip->vclamp_mv = IRCOMP_VCLAMP_MAX_MV;
	}

	ret = of_property_read_u32(dev_node, "ti,chg-tmout-mins",
				   &(chip->chg_tmout_mins));
	if (ret) {
		pr_err("Unable to read chg-tmout-mins.\n");
	}

	ret = of_property_read_u32(dev_node, "ti,term-current-ma",
				   &(chip->term_current_ma));
	if (ret) {
		pr_err("Unable to read term_current_ma.\n");
		return ret;
	}
	//fuelsummary_of_property_put("ti,term-current-ma", PARAMS_TYPE_INT, &(chip->term_current_ma));

	ret = of_property_read_u32(dev_node, "ti,pre-chg-current-ma",
				   &chip->pre_chg_current_ma);
	if (ret) {
		pr_err("Unable to read pre-chg-current-ma.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,vbat-max-mv",
				   &chip->vbat_max_mv);
	if (ret) {
		pr_err("Unable to read vbat-max-mv.\n");
		return ret;
	}
	//fuelsummary_of_property_put("ti,vbat-max-mv", PARAMS_TYPE_INT, &(chip->vbat_max_mv));

	ret = of_property_read_u32(dev_node, "ti,vindpm-5v-thr-mv",
				   &chip->vindpm_5v_thr_mv);
	if (ret) {
		pr_err("Unable to read vindpm-5v-thr-mv.\n");
		return ret;
	}
	if(chip->primary_charger && power_off_charging_mode == 1)
		chip->vindpm_5v_thr_mv = 4600;

	ret = of_property_read_u32(dev_node, "ti,vindpm-9v-thr-mv",
				   &chip->vindpm_9v_thr_mv);
	if (ret) {
		pr_err("Unable to read vindpm-9v-thr-mv.\n");
		//return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,vindpm-offset-mv",
				   &chip->vindpm_offset_mv);
	if (ret) {
		pr_err("Unable to read vindpm-offset-mv.\n");
		//return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,cfg-fastchg-current-ma",
				   &chip->cfg_fastchg_current_ma);
	if (ret) {
		pr_err("Unable to read cfg-fastchg-current-ma.\n");
		//return ret;
	}
	chip->total_fastchg_current_ma = chip->cfg_fastchg_current_ma;

	ret = of_property_read_u32(dev_node, "ti,parallel-usb-9v-min-current-ma",
				   &chip->parallel.min_9v_current_thr_ma);
	if (ret) {
		pr_err("Unable to read parallel-usb-9v-min-current-ma.\n");
		//return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,parallel-usb-min-current-ma",
				   &chip->parallel.min_current_thr_ma);
	if (ret) {
		pr_err("Unable to read parallel-usb-min-current-ma.\n");
		//return ret;
	}
	if (chip->parallel.min_current_thr_ma != -EINVAL
			&& chip->parallel.min_9v_current_thr_ma != -EINVAL)
		chip->parallel.avail = true;

	ret = of_property_read_u32(dev_node, "ti,parallel-fastchg-current-max-ma",
				   &chip->parallel.fastchg_current_max_ma);
	if (ret) {
		pr_err("Unable to read parallel-fastchg-current-max-ma.\n");
		//return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,fastchg-current-max-ma",
				   &chip->fastchg_current_max_ma);
	if (ret) {
		pr_err("Unable to read fastchg-current-max-ma.\n");
		//return ret;
	}
	ret = of_property_read_u32(dev_node, "ti,fastchg-accuracy",
				   &chip->fastchg_acc);
	if (ret) {
		pr_err("Unable to read fastchg-accuracy.\n");
		//return  ret;
	}

	return ret;
}

 int bq25890_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct bq25890_chip *chip;
	struct device_node *dev_node = client->dev.of_node;
	struct power_supply *usb_psy;
	int ret;

	dev_warn(&client->dev, "\n");
	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("usb supply not found \n");
		//return -EPROBE_DEFER;
	}

    chip = kzalloc(sizeof(struct bq25890_chip),GFP_KERNEL);
    if(!chip){
        dev_err(&client->dev,"out of memory\n");
        return -ENOMEM;
    }

	chip->usb_psy = usb_psy;
    chip->dev = &client->dev;
    chip->client = client;
	chip->irq = 0;
    i2c_set_clientdata(client,chip);
/*
	chip->vadc_dev = qpnp_get_vadc(chip->dev, "chg");
	if (IS_ERR(chip->vadc_dev)) {
		ret = PTR_ERR(chip->vadc_dev);
		if (ret != -EPROBE_DEFER)
			pr_err("vadc property missing\n");
		goto err_mem;
	}
*/
    ret = bq25890_detect_device(chip);
    if(ret == 0){
        if(chip->part_no == BQ25890){
			chip->status |= BQ25890_STATUS_EXIST;
            pr_err("charger device bq25890 detected, revision:%d\n",chip->revision);
		//fuelsummary_collect_value(ID_SIC_VENDOR, CHGIC_BQ25890);
        }else if (chip->part_no == BQ25892){
			chip->status |= BQ25890_STATUS_EXIST;
            pr_err("charger device bq25892 detected, revision:%d\n",chip->revision);
		//fuelsummary_collect_value(ID_SIC_VENDOR, CHGIC_BQ25892);
        } else {
            pr_err("unexpected charger device detected\n");
            kfree(chip);
			return -ENODEV;
	}
    }else{
        pr_err("no bq25890 charger device found:%d\n",ret);
        kfree(chip);
        return -ENODEV;
    }
    the_chip = chip;

	chip->fake_battery_soc = -EINVAL;

	ret = bq25890_parse_dt(dev_node, chip);
	if (ret) {
		pr_err("failed to parse dt\n");
		goto err_mem;
	}
	ret = bq25890_regulator_init(chip);
	if (ret) {
		pr_err("Couldn't initialize regulator rc=%d\n", ret);
	}

	ret = bq25890_gpio_init(chip);
	if (ret) {
		pr_err("Couldn't initialize gpio rc=%d\n", ret);
		goto err_regulator;
	}
	//bq25890_dump_regs(chip);
	bq25890_reset_chip(chip);
	ret = bq25890_hw_init(chip);
	if (ret) {
		dev_err(chip->dev, "device init failure: %d\n", ret);
		goto err_regulator;
	}

	//bq25890_dump_regs(chip);

	if(chip->disable_parallel_charger){
		bq25890_parallel_en = 0;
		pr_err("disable bq25890_parallel_en\n");
	}

    ret = bq25890_psy_register(chip);
    if(ret)
		goto err_gpio;

	INIT_WORK(&chip->irq_work, bq25890_charger_irq_workfunc);
	INIT_WORK(&chip->adapter_in_work, bq25890_adapter_in_workfunc);
	INIT_WORK(&chip->adapter_out_work, bq25890_adapter_out_workfunc);
	INIT_DELAYED_WORK(&chip->monitor_work, bq25890_monitor_workfunc);
	INIT_DELAYED_WORK(&chip->otg_monitor_work, bq25890_otg_monitor_workfunc);
	INIT_DELAYED_WORK(&chip->power_detect_work,bq25890_power_detect_work);
	INIT_DELAYED_WORK(&chip->rerun_apsd_work,bq25890_rerun_apsd_work);
	INIT_DELAYED_WORK(&chip->chgdone_work, bq25890_chgdone_workfunc);
	INIT_DELAYED_WORK(&chip->rerun_base_fb_on_work,bq25890_rerun_base_fb_on_work);

	mutex_init(&chip->chgdone_lock);
	mutex_init(&chip->pm_lock);
	mutex_init(&chip->parallel.lock);
	mutex_init(&chip->fcc_lock);
	mutex_init(&chip->current_change_lock);
	wake_lock_init(&chip->irq_wake_lock,
		WAKE_LOCK_SUSPEND, BQ25890_NAME "irq");

	if(chip->primary_charger)
		bq25890_create_debugfs_entries(chip);

	ret = sysfs_create_group(&chip->dev->kobj, &bq25890_attr_group);
	if (ret) {
		dev_err(chip->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}
	ret = sysfs_create_group(&chip->dev->kobj,
					&cmcc_attr_group);
	if (ret) {
		pr_err("cmcc attribute register fail\n");
		goto err_irq;
	}

    if (chip->irq) {
		ret = request_irq(chip->irq, bq25890_charger_interrupt,IRQF_TRIGGER_FALLING | IRQF_ONESHOT,"bq25890_charger1_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Request IRQ %d failed: %d\n",client->irq, ret);
			goto err_irq;
		} else {
			enable_irq_wake(chip->irq);
	        pr_debug("irq = %d\n",chip->irq);
	    }
    }
    schedule_work(&chip->irq_work);

	bq25890_parallel_enable_charging(chip, chip->chg_enabled);
	bq25890_enable_hiz(chip, !chip->chg_enabled);

    pr_info("probe success,primary_charger=%d,chg_enabled=%d\n", chip->primary_charger, chip->chg_enabled);
	return 0;

err_irq:
	cancel_work_sync(&chip->irq_work);
	wake_lock_destroy(&chip->irq_wake_lock);
err_gpio:
	if (chip->irq_gpio)
		gpio_free(chip->irq_gpio);
	if (chip->en_gpio > 0)
		gpio_free(chip->en_gpio);
	if (chip->usbsel_gpio)
		gpio_free(chip->usbsel_gpio);
err_regulator:
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
err_mem:
	kfree(chip);
    the_chip = NULL;
	return ret;
}
 EXPORT_SYMBOL(bq25890_probe);
 int bq25890_remove(struct i2c_client *client)
{
	struct bq25890_chip *chip = i2c_get_clientdata(client);

	sysfs_remove_group(&chip->dev->kobj, &bq25890_attr_group);
	sysfs_remove_group(&chip->dev->kobj, &cmcc_attr_group);
	wake_lock_destroy(&chip->irq_wake_lock);
	cancel_work_sync(&chip->irq_work);
	cancel_work_sync(&chip->adapter_in_work);
	cancel_work_sync(&chip->adapter_out_work);
	cancel_delayed_work_sync(&chip->monitor_work);
	cancel_delayed_work_sync(&chip->otg_monitor_work);
	cancel_delayed_work_sync(&chip->power_detect_work);
	cancel_delayed_work_sync(&chip->rerun_apsd_work);
    bq25890_psy_unregister(chip);

	if (chip->irq)
		free_irq(chip->irq, chip);

	if (chip->irq_gpio)
		gpio_free(chip->irq_gpio);

	kfree(chip);
    the_chip = NULL;
	return 0;
}
  EXPORT_SYMBOL(bq25890_remove);
/*
static void bq25890_shutdown(struct i2c_client *client)
{
	struct bq25890_chip *chip = i2c_get_clientdata(client);

	pr_debug("vbus_hized=%d\n",chip->vbus_hized);
	if (chip->vbus_hized) {
		bq25890_enable_hiz(chip,false);
		//chip->vbus_hized = false;
	}
}
static const struct i2c_device_id bq25890_id[] = {
	{BQ25890_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25890_id);

static const struct of_device_id bq25890_match[] = {
	{ .compatible = "ti,bq25892-charger", },
	{ },
};

static struct i2c_driver bq25890_driver = {
	.driver	= {
			.name	= BQ25890_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = of_match_ptr(bq25890_match),
	},
	.probe		= bq25890_probe,
	.remove		= bq25890_remove,
	.shutdown   = bq25890_shutdown,
	.id_table	= bq25890_id,
};
static int __init bq25890_init(void)
{
	return i2c_add_driver(&bq25890_driver);
}
module_init(bq25890_init);

static void __exit bq25890_exit(void)
{
	return i2c_del_driver(&bq25890_driver);
}
module_exit(bq25890_exit);
*/
MODULE_DESCRIPTION("TI BQ2589x Dual Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
MODULE_ALIAS("i2c:" BQ25890_NAME);
