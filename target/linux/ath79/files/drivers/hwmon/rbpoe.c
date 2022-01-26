// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Mikrotik POE driver
 *
 * Based on https://github.com/adron-s/mtpoe_ctrl
 */

#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/version.h>

#include "rbpoe.h"

DECLARE_CRC8_TABLE(rbpoe_crc_table);

#define MAX_PORTS 4

int rb_poe_get_port_idx(struct rb_poe_data *poe, int reg)
{
	if (poe->data->reverse)
		return (MAX_PORTS-reg);
	else
		return reg-1;
}
EXPORT_SYMBOL(rb_poe_get_port_idx);

int rb_poe_write_cmd(struct rb_poe_data *poe, u16 *resp, u8 cmd, u8 arg1, u8 arg2)
{
	int ret;
	u8 tx[4];
	u8 rx[6];
	u8 crc, retries;

	struct spi_transfer xfers[] = {
		{
			.tx_buf = tx,
			.len = 4,
			.delay.value = 10,
			.delay.unit = SPI_DELAY_UNIT_USECS,
		},
		{
			.rx_buf = rx,
			.len = 6,
		},
	};

	mutex_lock(&poe->lock);

	tx[0] = cmd;
	tx[1] = arg1;
	tx[2] = arg2;
	tx[3] = crc8(rbpoe_crc_table, tx, 3, 0);

	for (retries = 0; retries < MAX_RETRIES; retries++) {
		ret = spi_sync_transfer(poe->spi, xfers, ARRAY_SIZE(xfers));
		if (ret < 0) {
			dev_err(&poe->spi->dev, "SPI transfer error");
			goto out;
		}

		if (rx[1] != cmd) {
			ndelay(13);
			continue;
		}

		crc = crc8(rbpoe_crc_table, rx+1, 3, 0);

		if (rx[4] != crc && rx[5] != crc)
			continue;

		resp[0] = rx[2] << 8 | rx[3];
		goto out;
	}
out:
	mutex_unlock(&poe->lock);
	return ret;
}
EXPORT_SYMBOL(rb_poe_write_cmd);

static int rb_poe_read_version(struct rb_poe_data *data)
{
	int ret;
	u16 vers;

	ret = rb_poe_write_cmd(data, &vers, 0x41, 0, 0);
	if (ret < 0)
		return ret;
	return vers;
}

int rb_poe_read_voltage(struct rb_poe_data *data)
{
	int ret;
	u16 val;

	ret = rb_poe_write_cmd(data, &val, 0x42, 0, 0);
	if (ret < 0)
		return ret;
	return val * data->data->volt_lsb;
}
EXPORT_SYMBOL(rb_poe_read_voltage);

static int rb_poe_read_temperature(struct rb_poe_data *data)
{
	int ret;
	u16 val;

	ret = rb_poe_write_cmd(data, &val, 0x43, 0, 0);
	if (ret < 0)
		return ret;
	return (val * data->data->temp_lsb) - data->data->temp_offset;
}

/* sysfs attributes */
static ssize_t temp_input_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rb_poe_data *data = dev_get_drvdata(dev);
	int val;

	if (IS_ERR(data))
		return -ENODATA;

	val = rb_poe_read_temperature(data);
	if (val < 0)
		return -ENODATA;

	return sprintf(buf, "%d\n", val);
}

static ssize_t in_input_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct rb_poe_data *data = dev_get_drvdata(dev);
	int val;

	if (IS_ERR(data))
		return -ENODATA;

	val = rb_poe_read_voltage(data);
	if (val < 0)
		return -ENODATA;

	return sprintf(buf, "%d\n", val);
}

static SENSOR_DEVICE_ATTR_RO(temp1_input, temp_input, 0);
static SENSOR_DEVICE_ATTR_RO(in0_input, in_input, 4);

static struct attribute *rb_poe_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_in0_input.dev_attr.attr,
	NULL
};

ATTRIBUTE_GROUPS(rb_poe);

static int rb_poe_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct device *hwmon_dev;
	struct rb_poe_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->data = (struct rb_poe_model *)of_device_get_match_data(dev);
	data->spi = spi;

	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	crc8_populate_lsb(rbpoe_crc_table, 0x8C);
	dev_set_drvdata(dev, data);
	mutex_init(&data->lock);

	ret = rb_poe_read_version(data);
	if (ret < 0)
		return ret;

	dev_info(dev, "firmware: %d.%d", ret & 0xff, ret >> 8);

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret < 0) {
		dev_err(dev, "failed to populate DT children\n");
		return ret;
	}

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, "rbpoe", data, rb_poe_groups);

	if (IS_ERR(hwmon_dev))
		dev_dbg(dev, "unable to register hwmon device\n");

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct rb_poe_model v2_data = {
	.reverse = 1,
	.volt_lsb = 35,
	.temp_lsb = 1000,
	.temp_offset = 273000,
};

/*
 * ATSAMD20J15 temperature sensor factory calibration is done at 25C 667mV with Vddana=3.3V
 * Typical sensor slope 2.4mV/C. 667mV/2.4mV/C = 277.917C - 25C = 253.917C
 * Measured Vddana is 3.29V, reduce slope a bit and tune calibration vdd.
 * 680mV/2.375mV/C = 286.315C - 25C = 261.315C
 * 1000/2.375 = 421
 */
static const struct rb_poe_model v3_data = {
	.reverse = 0,
	.volt_lsb = 10,
	.temp_lsb = 421,
	.temp_offset = 261315,
};

static const struct of_device_id rb_poe_dt_match[] = {
	{
		.compatible = "mikrotik,poe-v2",
		.data = (void *)&v2_data,
	}, {
		.compatible = "mikrotik,poe-v3",
		.data = (void *)&v3_data,
	}, { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rb_poe_dt_match);

static struct spi_driver rb_poe_driver = {
	.probe = rb_poe_probe,
	.driver = {
		.name = "rb-poe-driver",
		.bus = &spi_bus_type,
		.of_match_table = of_match_ptr(rb_poe_dt_match),
	},
};

module_spi_driver(rb_poe_driver);

MODULE_AUTHOR("Oskari Lemmela <oskari@lemmela.net>");
MODULE_DESCRIPTION("Mikrotik POE driver");
MODULE_LICENSE("GPL");
