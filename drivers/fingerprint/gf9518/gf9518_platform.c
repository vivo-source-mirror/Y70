/*
 * platform indepent driver interface
 *
 * Coypritht (c) 2017 Goodix
 */
#define pr_fmt(fmt)		"[FP_KERN]" KBUILD_MODNAME ": " fmt
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf9518_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

int gf9518_parse_dts(struct gf_dev *gf_dev)
{
	int rc = 0;

	gf_dev->vdd_use_gpio = of_property_read_bool(gf_dev->spi->dev.of_node, "goodix,vdd_use_gpio");
	gf_dev->vdd_use_pmic = of_property_read_bool(gf_dev->spi->dev.of_node, "goodix,vdd_use_pmic");
	gf_dev->vddio_use_gpio = of_property_read_bool(gf_dev->spi->dev.of_node, "goodix,vddio_use_gpio");
	gf_dev->vddio_use_pmic = of_property_read_bool(gf_dev->spi->dev.of_node, "goodix,vddio_use_pmic");

	pr_info("%s vdd_use_gpio %d\n", __func__, gf_dev->vdd_use_gpio);
	pr_info("%s vdd_use_pmic %d\n", __func__, gf_dev->vdd_use_pmic);
	pr_info("%s vddio_use_gpio %d\n", __func__, gf_dev->vddio_use_gpio);
	pr_info("%s vddio_use_pmic %d\n", __func__, gf_dev->vddio_use_pmic);

	// VDD
	if (gf_dev->vdd_use_gpio) {
		gf_dev->vdd_en_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_vdd_en", 0);
		if (!gpio_is_valid(gf_dev->vdd_en_gpio)) {
			pr_info("%s VDD_EN GPIO is invalid.\n", __func__);
			return -EIO;
		} else {
			pr_info("%s VDD_EN GPIO is %d.\n", __func__, gf_dev->vdd_en_gpio);
		}
	}

	if (gf_dev->vdd_use_pmic) {
		gf_dev->vdd = regulator_get(&gf_dev->spi->dev, "vdd");
		if (IS_ERR(gf_dev->vdd)) {
		pr_info("%s Unable to get vdd\n", __func__);
		return -EINVAL;
		} else {
			pr_info("%s Success to get vdd\n", __func__);
		}
	}

	// VDDIO
	if (gf_dev->vddio_use_gpio) {
		gf_dev->vddio_en_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_vddio_en", 0);
		if (!gpio_is_valid(gf_dev->vddio_en_gpio)) {
			pr_info("%s VDDIO_EN GPIO is invalid.\n", __func__);
			return -EIO;
		} else {
			pr_info("%s VDDIO_EN GPIO is %d.\n", __func__, gf_dev->vddio_en_gpio);
		}
	}

	if (gf_dev->vddio_use_pmic) {
		gf_dev->vddio = regulator_get(&gf_dev->spi->dev, "vddio");
		if (IS_ERR(gf_dev->vddio)) {
		pr_info("%s Unable to get vddio\n", __func__);
			return -EINVAL;
		} else {
			pr_info("%s Success to get vddio\n", __func__);
		}
	}

	/*get reset resource */
	gf_dev->reset_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_reset", 0);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_info("RESET GPIO is invalid.\n");
		return -EIO;
	}

	/*get irq resourece */
	gf_dev->irq_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_irq", 0);
	pr_info("%s irq_gpio:%d\n", __func__, gf_dev->irq_gpio);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_info("IRQ GPIO is invalid.\n");
		return -EIO;
	}
	return rc;
}

int gf9518_request_resource(struct gf_dev *gf_dev, irq_handler_t thread_fn)
{
	int rc = 0;
	pr_info("%s: enter\n", __func__);

	if (!gf_dev->resource_requested) {
		//VDD
		if (gf_dev->vdd_use_gpio) {
			rc = gpio_request(gf_dev->vdd_en_gpio, "goodix_vdd_en");
			if (rc) {
				pr_info("%s Failed to request VDD GPIO. rc = %d\n", __func__, rc);
				goto error_gpio_vdd;
			}
		}
		// VDDIO
		if (gf_dev->vddio_use_gpio) {
			rc = gpio_request(gf_dev->vddio_en_gpio, "goodix_vddio_en");
			if (rc) {
				pr_info("%s Failed to request VDDIO GPIO. rc = %d\n", __func__, rc);
				goto error_gpio_vddio;
			}
		}
		/*get reset resource */
		rc = gpio_request(gf_dev->reset_gpio, "goodix_reset");
		if (rc) {
			pr_info("Failed to request RESET GPIO. rc = %d\n", rc);
			goto error_gpio_reset;
		}
		gpio_direction_output(gf_dev->reset_gpio, 0);

		/*get irq resourece */
		rc = gpio_request(gf_dev->irq_gpio, "goodix_irq");
		if (rc) {
			pr_info("Failed to request IRQ GPIO. rc = %d\n", rc);
			goto error_gpio_irq;
		}
		gpio_direction_input(gf_dev->irq_gpio);

		/*register irq*/
		gf_dev->irq = gf9518_irq_num(gf_dev);

		rc = request_threaded_irq(gf_dev->irq, NULL, thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "gf", gf_dev);

		if (rc) {
			pr_err("failed to request IRQ:%d\n", gf_dev->irq);
			goto error_irq;
		}
		enable_irq_wake(gf_dev->irq);
		gf_dev->irq_enabled = 1;

		gf_dev->resource_requested = true;
	} else {
		 pr_info("%s resource is requested already.", __func__);
	}

	/*power on and enable irq*/
	rc = gf9518_power_on(gf_dev);
	if (rc) {
		pr_err("gf9518 power on fail.\n");
		goto error_power;
	}

	gf9518_hw_reset(gf_dev, 3);
	gf_dev->device_available = 1;

	pr_info("%s: exit\n", __func__);
	return 0;

error_power:
	gf_disable_irq(gf_dev);
error_irq:
	free_irq(gf_dev->irq, gf_dev);

	if (gpio_is_valid(gf_dev->irq_gpio)) {
		gpio_free(gf_dev->irq_gpio);
	}
error_gpio_irq:
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
	}
error_gpio_reset:
	if (gf_dev->vddio_use_gpio) {
		if (gpio_is_valid(gf_dev->vddio_en_gpio)) {
			gpio_free(gf_dev->vddio_en_gpio);
		}
	}
error_gpio_vddio:
	if (gf_dev->vdd_use_gpio) {
		if (gpio_is_valid(gf_dev->vdd_en_gpio)) {
			gpio_free(gf_dev->vdd_en_gpio);
		}
	}
error_gpio_vdd:
	pr_err("%s requset source failed.\n", __func__);
	return rc;
}

void gf9518_release_resource(struct gf_dev *gf_dev)
{
	pr_info("%s: enter\n", __func__);
	if (gf_dev->resource_requested) {
		gf_disable_irq(gf_dev);
		if (gf_dev->irq)
			free_irq(gf_dev->irq, gf_dev);
		gf9518_power_off(gf_dev);

		if (gpio_is_valid(gf_dev->irq_gpio)) {
			gpio_free(gf_dev->irq_gpio);
			pr_info("remove irq_gpio success\n");
		}

		if (gpio_is_valid(gf_dev->reset_gpio)) {
			gpio_free(gf_dev->reset_gpio);
			pr_info("remove reset_gpio success\n");
		}

		if (gf_dev->vdd_use_gpio) {
			if (gpio_is_valid(gf_dev->vdd_en_gpio)) {
				gpio_free(gf_dev->vdd_en_gpio);
				pr_info("remove vdd_en_gpio success\n");
			}
		}

		if (gf_dev->vddio_use_gpio) {
			if (gpio_is_valid(gf_dev->vddio_en_gpio)) {
				gpio_free(gf_dev->vddio_en_gpio);
				pr_info("remove vddio_en_gpio success\n");
			}
		}
		gf_dev->resource_requested = false;
	} else {
		pr_warn("resource not request, can not release resource.\n");
	}
	pr_info("%s: exit\n", __func__);
}


int gf9518_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;
	pr_info("%s, enter.\n", __func__);

	if (!gf_dev->resource_requested) {
		pr_info("%s, resource_requested: %d.\n", __func__, gf_dev->resource_requested);
		return 0;
	}

	// VDD ON
	if (gf_dev->vdd_use_gpio) {
		rc = gpio_direction_output(gf_dev->vdd_en_gpio, 1);
		if (rc) {
		pr_info("gf9518 vdd power on fail.\n");
		return -EIO;
		}
	}

	if (gf_dev->vdd_use_pmic) {
		if (regulator_is_enabled(gf_dev->vdd)) {
			pr_info("%s, vdd state:on,don't set repeatedly!\n", __func__);
			return rc;
		}

		rc = regulator_set_load(gf_dev->vdd, 600000);
		if (rc < 0) {
			pr_err("%s: vdd regulator_set_load(uA_load=%d) failed. rc=%d\n",
			__func__, 1000, rc);
		}

		if (regulator_count_voltages(gf_dev->vdd) > 0) {
			rc = regulator_set_voltage(gf_dev->vdd, 3000000, 3000000);
			if (rc) {
				pr_info(KERN_ERR "gf9518:Unable to set voltage on vdd");
			}
		}
		rc = regulator_enable(gf_dev->vdd);
	}

	// VDDIO ON
	if (gf_dev->vddio_use_gpio) {
		rc = gpio_direction_output(gf_dev->vddio_en_gpio, 1);
		if (rc) {
			pr_info("gf9518 vddio power on fail.\n");
			return -EIO;
		}
	}

	if (gf_dev->vddio_use_pmic) {
		if (regulator_is_enabled(gf_dev->vddio)) {
			pr_info("%s, vddio state:on,don't set repeatedly!\n", __func__);
			return rc;
		}

		rc = regulator_set_load(gf_dev->vddio, 600000);
		if (rc < 0) {
			pr_err("%s: vddio regulator_set_load(uA_load=%d) failed. rc=%d\n",
			__func__, 1000, rc);
		}

		if (regulator_count_voltages(gf_dev->vddio) > 0) {
			rc = regulator_set_voltage(gf_dev->vddio, 1800000, 1800000);
			if (rc) {
				pr_info(KERN_ERR "gf9518:Unable to set voltage on vddio");
			}
		}
		rc = regulator_enable(gf_dev->vddio);
	}

	pr_info("%s, exit.\n", __func__);

	return rc;
}

int gf9518_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;
	pr_info("%s, enter.\n", __func__);

	if (!gf_dev->resource_requested) {
		pr_info("%s, resource_requested: %d.\n", __func__, gf_dev->resource_requested);
		return 0;
	}

	rc = gpio_direction_output(gf_dev->reset_gpio, 0);
	if (rc) {
		pr_info("gf9518 set reset gpio output fail.\n");
		return -EIO;
	}

	// VDD OFF
	if (gf_dev->vdd_use_gpio) {
		rc = gpio_direction_output(gf_dev->vdd_en_gpio, 0);
		if (rc) {
		pr_info("gf9518 vdd power off fail.\n");
		return -EIO;
		}
	}

	if (gf_dev->vdd_use_pmic) {
		rc = regulator_set_load(gf_dev->vdd, 0);
		if (rc < 0) {
			pr_err("%s: vdd regulator_set_load(uA_load=%d) failed. rc=%d\n",
				__func__, 0, rc);
		}
		if (regulator_is_enabled(gf_dev->vdd)) {
			regulator_disable(gf_dev->vdd);
		}
		pr_info(KERN_ERR "gf9518: disable  vdd %d\n", rc);
	}

	// VDDIO OFF
	if (gf_dev->vddio_use_gpio) {
		rc = gpio_direction_output(gf_dev->vddio_en_gpio, 0);
		if (rc) {
		pr_info("gf9518 vddio power off fail.\n");
		return -EIO;
		}
	}

	if (gf_dev->vddio_use_pmic) {
	rc = regulator_set_load(gf_dev->vddio, 0);
		if (rc < 0) {
			pr_err("%s: vddio regulator_set_load(uA_load=%d) failed. rc=%d\n",
				__func__, 0, rc);
		}
		if (regulator_is_enabled(gf_dev->vddio)) {
			regulator_disable(gf_dev->vddio);
		}
		pr_info(KERN_ERR "gf9518: disable  vddio %d\n", rc);
	}

	pr_info("%s, exit.\n", __func__);

	return rc;
}

int gf9518_hw_get_power_state(struct gf_dev *gf_dev)
{

	int retval = 0;

	retval = gpio_get_value(gf_dev->vdd_en_gpio);
	pr_info("%s, retval=%d\n", __func__, retval);

	return retval;
}

int gf9518_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EFAULT;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);
	mdelay(10);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	return 0;
}

int gf9518_irq_num(struct gf_dev *gf_dev)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EFAULT;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}

