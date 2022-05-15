/*===========================================================================
* VENDOR_EDIT
* kme/wdt_monitor.c
*
* VIVO Kernel Monitor Engine(Wdt monitor)
*
* Copyright (C) 2017 VIVO Technology Co., Ltd
* ------------------------------ Revision History: --------------------------
* <version>           <date>               <author>                  <desc>
* Revision 1.0        2019-07-14       hankecai@vivo.com         Created file
*===========================================================================*/

#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sysfs.h>
#include <linux/typecheck.h>
#include <linux/percpu-defs.h>
#include <linux/kobject.h>
#include <linux/seq_file.h>
#include <linux/ratelimit.h>
#include <linux/mutex.h>

/*===========================================================================
*
*			Public part declarations
*
*===========================================================================*/
enum BOOT_STAGE {
	B_NONE			= 0x00,
	B_BOOTLOADER	= 0x01,
	B_KERNEL		= 0x02,
	B_FRAMEWORK 	= 0x03,
};

enum WDT_CMD_CTL {
	CMD_STOP				= 0,
	CMD_REBOOT				= -1,
	CMD_FORCE_OFFLINE_ON	= -101,
	CMD_FORCE_OFFLINE_OFF	= -100,
};

static bool wdt_force_offline;
module_param_named(wdt_force_offline, wdt_force_offline, bool, 0644);

extern int kme_merge_group(const struct attribute_group *);
extern void kme_remove_group(const struct attribute_group *);

static long enable_monitor;
static int boot_stage_flags = B_NONE;
static int max_crash_cnt_limit = 20;
static DEFINE_MUTEX(g_wdt_mutex_lock);

static int vivo_wdt_monitor_enable(void);
static int vivo_wdt_monitor_disable(void);
static int vivo_wdt_monitor_cfg(long timeout);

int vivo_wdt_monitor_pet(void);
int vivo_wdt_monitor_start(long timeout);
int vivo_wdt_monitor_start_kernel(long timeout);
int vivo_wdt_monitor_stop(void);
int vivo_wdt_monitor_reboot(void);
void vivo_wdt_monitor_setup(bool g_force_offline_init);
int vivo_wdt_monitor_force_offline(bool force_offline);

EXPORT_SYMBOL(vivo_wdt_monitor_start);
EXPORT_SYMBOL(vivo_wdt_monitor_start_kernel);
EXPORT_SYMBOL(vivo_wdt_monitor_stop);
EXPORT_SYMBOL(vivo_wdt_monitor_pet);
EXPORT_SYMBOL(vivo_wdt_monitor_reboot);
EXPORT_SYMBOL(vivo_wdt_monitor_setup);
EXPORT_SYMBOL(vivo_wdt_monitor_force_offline);

/*common ops of wdt monitor module*/
struct wdt_monitor_ops {
	int (*wdt_enable) (void);
	int (*wdt_disable) (void);
	int (*wdt_pet) (void);
	int (*wdt_cfg) (long timeout);
	int (*wdt_reboot) (void);
	int (*wdt_force_offline)(bool offline);
};

/*setup ops*/
static struct wdt_monitor_ops *wdt_monitor_common_ops;
static void vivo_wdt_monitor_ops_setup(struct wdt_monitor_ops *ops)
{
	wdt_monitor_common_ops = ops;
}

/*enable wdt*/
static int vivo_wdt_monitor_enable(void)
{
	int rc = 0;

	if (!wdt_monitor_common_ops)
		return 0;

	if (wdt_monitor_common_ops->wdt_enable)
		rc = wdt_monitor_common_ops->wdt_enable();

	return rc;
}

/*disable wdt*/
static int vivo_wdt_monitor_disable(void)
{
	int rc = 0;

	if (!wdt_monitor_common_ops)
		return 0;

	if (wdt_monitor_common_ops->wdt_disable)
		rc = wdt_monitor_common_ops->wdt_disable();

	return rc;
}

/*cfg wdt timeout*/
static int vivo_wdt_monitor_cfg(long timeout)
{
	int rc = 0;

	if (!wdt_monitor_common_ops)
		return 0;

	if (wdt_monitor_common_ops->wdt_cfg)
		rc = wdt_monitor_common_ops->wdt_cfg(timeout);

	return rc;
}

/*force_offline wdt*/
int vivo_wdt_monitor_force_offline(bool force_offline)
{
	int rc = 0;

	if (!wdt_monitor_common_ops)
		return 0;

	if (wdt_monitor_common_ops->wdt_force_offline)
		rc = wdt_monitor_common_ops->wdt_force_offline(force_offline);

	return rc;
}

/*start monitor*/
int vivo_wdt_monitor_start(long timeout)
{
	int rc = 0;

	mutex_lock(&g_wdt_mutex_lock);
	if (wdt_force_offline) {
		printk("wdt_monitor state: force_offline!\n");
		goto unlock;
	}

	printk("wdt_monitor_start 0x%02x...\n", boot_stage_flags);
	rc = vivo_wdt_monitor_disable();
	rc = vivo_wdt_monitor_cfg(timeout);
	rc = vivo_wdt_monitor_enable();

unlock:
	mutex_unlock(&g_wdt_mutex_lock);
	return rc;
}

/*start monitor from kernel*/
int vivo_wdt_monitor_start_kernel(long timeout)
{
	int rc = 0;

	if (wdt_force_offline) {
		printk("wdt_monitor state: force_offline!\n");
		return rc;
	}

	boot_stage_flags = B_KERNEL;
	enable_monitor = timeout;
	rc = vivo_wdt_monitor_start(timeout);

	return rc;
}

/*stop monitor*/
int vivo_wdt_monitor_stop(void)
{
	int rc = 0;

	mutex_lock(&g_wdt_mutex_lock);
	if (wdt_force_offline) {
		printk("wdt_monitor state: force_offline!\n");
		goto unlock;
	}

	printk("wdt_monitor_stop...\n");
	rc = vivo_wdt_monitor_disable();
	enable_monitor = 0;

unlock:
	mutex_unlock(&g_wdt_mutex_lock);
	return rc;
}

/*pet wdt*/
int vivo_wdt_monitor_pet(void)
{
	int rc = 0;

	mutex_lock(&g_wdt_mutex_lock);
	if (wdt_force_offline) {
		printk("wdt_monitor state: force_offline!\n");
		goto unlock;
	}

	printk("wdt_monitor_pet...\n");
	if (!wdt_monitor_common_ops)
		goto unlock;

	if (wdt_monitor_common_ops->wdt_pet)
		rc = wdt_monitor_common_ops->wdt_pet();

unlock:
	mutex_unlock(&g_wdt_mutex_lock);
	return rc;
}

/*reboot system*/
int vivo_wdt_monitor_reboot(void)
{
	int rc = 0;

	mutex_lock(&g_wdt_mutex_lock);
	printk("wdt_monitor_reboot...\n");
	if (!wdt_monitor_common_ops)
		goto unlock;

	if (wdt_monitor_common_ops->wdt_reboot)
		rc = wdt_monitor_common_ops->wdt_reboot();

unlock:
	mutex_unlock(&g_wdt_mutex_lock);
	return rc;
}

/*sys/kernel/kme/wdt/enable_monitor*/
static ssize_t enable_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n", enable_monitor);
}

extern unsigned int power_off_charging_mode;
extern unsigned int is_normal_mode;
/*sys/kernel/kme/wdt/enable_monitor*/
static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t count)
{
	int ret = 0;
	long temp_enable = 0;

	if (power_off_charging_mode)
		return count;

	if (is_normal_mode == 0)
		return count;

	/*validity check*/
	ret = kstrtol(buf, 10, &temp_enable);
	if (ret)
		return ret;

	/*take this value*/
	enable_monitor = temp_enable;

	/*Set flags to be framework*/
	boot_stage_flags = B_FRAMEWORK;

	/*reboot immediately if -1 or cnt excced*/
	if ((enable_monitor == CMD_REBOOT) ||
		(max_crash_cnt_limit == 0)) {
		vivo_wdt_monitor_reboot();
		return count;
	}

	/*force_offline ON, will take effect at next boot*/
	if (enable_monitor == CMD_FORCE_OFFLINE_ON) {
		vivo_wdt_monitor_force_offline(1);
		return count;
	}

	/*force_online OFF, will take effect at next boot*/
	if (enable_monitor == CMD_FORCE_OFFLINE_OFF) {
		vivo_wdt_monitor_force_offline(0);
		return count;
	}

	/*stop monitor if 0*/
	if (enable_monitor == CMD_STOP) {
		vivo_wdt_monitor_stop();
		return count;
	}

	/*start monitor if larger than 0*/
	max_crash_cnt_limit--;
	vivo_wdt_monitor_start(enable_monitor);
	return count;
}

/*create node /sys/kernel/kme/wdt*/
static struct kobj_attribute enable_attr =
	__ATTR(enable_monitor, 0640, enable_show, enable_store);

static struct attribute *wdt_attrs[] = {
	&enable_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.name = "wdt",
	.attrs = wdt_attrs,
};
/*===========================================================================
*
*			Platform implementation part declarations
*
*===========================================================================*/
#ifdef CONFIG_VIVO_WDT_MONITOR

#define WDT_MAX_COUNT 127
#define WDT_DELTA_UNIT 20

static int wdt_pet_cylcle;
static int wdt_pet_remainder;
static struct timer_list wdt_pet_timer;
static atomic_t wdt_pet_timer_enable = ATOMIC_INIT(0);

/*extern func from qpnp_pon module*/
extern int qpnp_pon_wd_pet(void);
extern int qpnp_pon_wd_enable(bool enable, u8 flags);
extern int qpnp_pon_wd_cfg_timer(u8 s1_timer, u8 s2_timer);
extern int qpnp_pon_wd_set_force_offline(bool force_offline);
extern int qpnp_pon_wd_set_test_mode(bool test_mode);

/*test_mode_ctl for wdt*/
static int wdt_test_mode_ctl;
static int wdt_test_mode_ctl_set(const char *val, const struct kernel_param *kp);
module_param_call(wdt_test_mode_ctl, wdt_test_mode_ctl_set, param_get_int,
			&wdt_test_mode_ctl, 0644);

/*test_mode_ctl set*/
static int wdt_test_mode_ctl_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	int old_val = wdt_test_mode_ctl;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	if (wdt_test_mode_ctl == 1)
		qpnp_pon_wd_set_test_mode(1);
	else if (wdt_test_mode_ctl == 0)
		qpnp_pon_wd_set_test_mode(0);
	else
		wdt_test_mode_ctl = old_val;

	return 0;
}

/*pmic pet timer start*/
void pmic_wdt_pet_timer_start(void)
{
	wdt_pet_timer.expires = jiffies + (WDT_MAX_COUNT - WDT_DELTA_UNIT)*HZ;
	add_timer(&wdt_pet_timer);
	atomic_set(&wdt_pet_timer_enable, 1);
}

/*pmic pet timer stop*/
void pmic_wdt_pet_timer_stop(void)
{
	if (timer_pending(&wdt_pet_timer))
		del_timer_sync(&wdt_pet_timer);
	atomic_set(&wdt_pet_timer_enable, 0);
}

/*pmic pet timer function*/
void pmic_wdt_pet_timer_fn(unsigned long data)
{
	long time_left = 0;

	qpnp_pon_wd_pet();

	wdt_pet_remainder = wdt_pet_remainder + WDT_DELTA_UNIT;
	wdt_pet_cylcle = wdt_pet_cylcle - 1 + (wdt_pet_remainder/WDT_MAX_COUNT);
	wdt_pet_remainder = wdt_pet_remainder%WDT_MAX_COUNT;

	time_left = wdt_pet_cylcle*WDT_MAX_COUNT + wdt_pet_remainder;
	if (time_left <= WDT_MAX_COUNT) {
		if (power_off_charging_mode)
			return;

		atomic_set(&wdt_pet_timer_enable, 0);
		qpnp_pon_wd_cfg_timer(time_left, 0);
		qpnp_pon_wd_enable(1, boot_stage_flags);
		return;
	}

	mod_timer(&wdt_pet_timer, jiffies + (WDT_MAX_COUNT - WDT_DELTA_UNIT)*HZ);
}

/*pmic wdt monitor timer setup*/
void pmic_wdt_monitor_timer_setup(void)
{
	atomic_set(&wdt_pet_timer_enable, 0);

	init_timer(&wdt_pet_timer);
	wdt_pet_timer.function = pmic_wdt_pet_timer_fn;
}

/*pmic wdt monitor enable*/
int pmic_wdt_monitor_enable(void)
{
	int rc = 0;

	rc = qpnp_pon_wd_enable(1, boot_stage_flags);

	if (atomic_read(&wdt_pet_timer_enable))
		pmic_wdt_pet_timer_start();

	return rc;
}

/*pmic wdt monitor disable*/
int pmic_wdt_monitor_disable(void)
{
	int rc = 0;

	/*auto set if boot_stage_flags = 0*/
	rc = qpnp_pon_wd_enable(0, B_NONE);

	wdt_pet_cylcle = 0;
	wdt_pet_remainder = 0;

	if (atomic_read(&wdt_pet_timer_enable))
		pmic_wdt_pet_timer_stop();

	return rc;
}

/*pmic wdt monitor pet*/
int pmic_wdt_monitor_pet(void)
{
	int rc = 0;

	rc = qpnp_pon_wd_pet();

	return rc;
}

/*pmic wdt monitor cfg*/
int pmic_wdt_monitor_cfg(long timeout)
{
	int rc = 0;

	if (timeout < 0)
		return -EINVAL;

	if (timeout <= WDT_MAX_COUNT) {
		rc = qpnp_pon_wd_cfg_timer(timeout, 0);
		atomic_set(&wdt_pet_timer_enable, 0);
		return rc;
	}

	wdt_pet_cylcle = timeout/WDT_MAX_COUNT;
	wdt_pet_remainder = timeout%WDT_MAX_COUNT;

	atomic_set(&wdt_pet_timer_enable, 1);
	rc = qpnp_pon_wd_cfg_timer(WDT_MAX_COUNT, 0);

	return rc;
}

/*pmic wdt monitor reboot*/
int pmic_wdt_monitor_reboot(void)
{
	int rc = 0;

	/*do immediately low-level reboot*/
	rc = qpnp_pon_wd_cfg_timer(1, 0);
	rc = qpnp_pon_wd_enable(1, boot_stage_flags);

	return rc;
}

/*pmic wdt force_offline*/
int pmic_wdt_monitor_force_offline(bool force_offline)
{
	int rc = 0;

	/*force_offline*/
	if (force_offline) {
		printk("wdt_monitor force_offline true from remote!\n");
		rc = qpnp_pon_wd_set_force_offline(1);
	} else {
		printk("wdt_monitor force_offline false from remote!\n");
		rc = qpnp_pon_wd_set_force_offline(0);
	}

	return rc;
}

/*pmic wdt monitor ops*/
struct wdt_monitor_ops qcom_pmic_wdt_ops = {
	.wdt_enable = pmic_wdt_monitor_enable,
	.wdt_disable = pmic_wdt_monitor_disable,
	.wdt_pet = pmic_wdt_monitor_pet,
	.wdt_cfg = pmic_wdt_monitor_cfg,
	.wdt_reboot = pmic_wdt_monitor_reboot,
	.wdt_force_offline = pmic_wdt_monitor_force_offline,
};

static bool __vivo_wdt_setup_done;
void vivo_wdt_monitor_setup(bool g_force_offline_init)
{
	if (__vivo_wdt_setup_done)
		return;

	wdt_force_offline = g_force_offline_init;
	printk("wdt_monitor force_offline_init[%d]\n", g_force_offline_init);

	pmic_wdt_monitor_timer_setup();
	vivo_wdt_monitor_ops_setup(&qcom_pmic_wdt_ops);

	__vivo_wdt_setup_done = true;
}
#endif	/*CONFIG_VIVO_WDT_MONITOR*/
/*===========================================================================
*
*			Module entry initialization
*
*===========================================================================*/

static int __init vivo_wdt_monitor_init(void)
{
	int ret;

	ret = kme_merge_group(&attr_group);

#ifdef CONFIG_VIVO_WDT_MONITOR
	if (ret) {
		vivo_wdt_monitor_setup(0);
		vivo_wdt_monitor_stop();
	}
#endif /*CONFIG_VIVO_WDT_MONITOR*/

	return ret;
}

static void __exit vivo_wdt_monitor_exit(void)
{
	kme_remove_group(&attr_group);
}

module_init(vivo_wdt_monitor_init);
module_exit(vivo_wdt_monitor_exit);
MODULE_LICENSE("GPL v2");

