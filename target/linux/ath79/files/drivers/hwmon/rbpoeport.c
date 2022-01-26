// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Mikrotik POE port driver
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "rbpoe.h"

#define PORT_DISABLED 0x0
#define PORT_FORCE    0x1
#define PORT_ENABLED  0x2

struct poeport_data {
	struct rb_poe_data *poe;
	struct device *dev;

	u8 state;
	u8 force;
	u32 reg;
};

static int read_state(struct poeport_data *port)
{
	int ret;
	u16 status;

	ret = rb_poe_write_cmd(port->poe, &status, 0x45, 0, 0);
	if (ret < 0)
		return ret;
	return status >> rb_poe_get_port_idx(port->poe, port->reg)*4 & 0xF;
}

static int write_state(struct poeport_data *port, u8 state, u8 force)
{
	int ret;
	u16 status;

	if (state)
		if (force)
			state = PORT_FORCE;
		else
			state = PORT_ENABLED;
	else
		state = PORT_DISABLED;

	ret = rb_poe_write_cmd(port->poe, &status, 0x44, port->reg, state);
	if (ret < 0) {
		usleep_range(100, 150);
		ret = rb_poe_write_cmd(port->poe, &status, 0x44, port->reg, state);
	}
	return ret;
}

static int read_current(struct poeport_data *port)
{
	int ret;
	u16 curr;

	ret = rb_poe_write_cmd(port->poe, &curr, 0x58+port->reg, 0, 0);
	if (ret < 0)
		return ret;
	return curr;
}

/* sysfs attributes */
static ssize_t in_input_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int state, curr;

	if (IS_ERR(data))
		return PTR_ERR(data);

	state = read_state(data);
	if (state < 0 || state == PORT_DISABLED)
		return -ENODATA;

	curr = read_current(data);
	if (curr < 0 || (state == PORT_ENABLED && curr >> 15))
		return -ENODATA;

	return sprintf(buf, "%d\n", rb_poe_read_voltage(data->poe));
}

static ssize_t force_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", data->force);
}

static ssize_t force_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int ret;
	int state;

	ret = kstrtoint(buf, 0, &state);
	if (ret)
		return ret;

	data->force = state;
	ret = write_state(data, data->state, state);
	if (ret)
		return ret;
	return count;
}

static ssize_t port_state_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int curr;

	if (IS_ERR(data))
		return PTR_ERR(data);

	if (!data->state)
		return sprintf(buf, "disabled\n");

	curr = read_current(data);
	if (curr == 0x800A)
		return sprintf(buf, "short circuit\n");
	if (!data->force && curr >> 15)
		return sprintf(buf, "searching\n");
	if (curr < 3)
		return sprintf(buf, "no load\n");

	return sprintf(buf, "delivering\n");

}

static ssize_t in_enable_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", data->state);
}

static ssize_t in_enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int ret;
	int state;

	ret = kstrtoint(buf, 0, &state);
	if (ret)
		return ret;

	data->state = state;
	ret = write_state(data, state, data->force);
	if (ret)
		return ret;

	return count;
}

static ssize_t curr_input_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int curr;

	if (IS_ERR(data))
		return PTR_ERR(data);

	curr = read_current(data);
	if (curr < 0 || curr >> 15)
		return -ENODATA;

	return sprintf(buf, "%d\n", curr);
}

static ssize_t power_input_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct poeport_data *data = dev_get_drvdata(dev);
	int volt, curr;

	if (IS_ERR(data))
		return PTR_ERR(data);

	curr = read_current(data);
	if (curr < 0 || curr >> 15)
		return -ENODATA;

	volt = rb_poe_read_voltage(data->poe);
	if (volt < 0)
		return -ENODATA;

	return sprintf(buf, "%d\n", curr*volt);
}

static SENSOR_DEVICE_ATTR_RO(curr1_input, curr_input, 0);
static SENSOR_DEVICE_ATTR_RO(power1_input, power_input, 0);
static SENSOR_DEVICE_ATTR_RW(in1_enable, in_enable, 0);
static SENSOR_DEVICE_ATTR_RO(in1_input, in_input, 0);
static SENSOR_DEVICE_ATTR_RW(force_enable, force_enable, 0);
static SENSOR_DEVICE_ATTR_RO(port_state, port_state, 0);

static struct attribute *rb_poeport_attrs[] = {
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_power1_input.dev_attr.attr,
	&sensor_dev_attr_in1_enable.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_force_enable.dev_attr.attr,
	&sensor_dev_attr_port_state.dev_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(rb_poeport);

static int rb_poeport_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *hwmon_dev;
	struct poeport_data *data;
	const char *label;
	int ret, val;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	ret = of_property_read_u32(dev->of_node, "reg", &val);
	if (ret < 0) {
		dev_err(dev, "missing 'reg' property (%d)\n", ret);
		return ret;
	}

	data->reg = val;

	if (of_property_read_string(dev->of_node, "label", &label) < 0)
		label = "poeport";

	if (!dev->parent) {
		dev_err(dev, "no ctrl device\n");
		return -ENODEV;
	}

	data->poe = dev_get_drvdata(dev->parent);

	switch (read_state(data)) {
	case PORT_FORCE:
		data->force = 1;
		data->state = 1;
		break;
	case PORT_ENABLED:
		data->force = 0;
		data->state = 1;
		break;
	case PORT_DISABLED:
		data->state = 0;
		data->force = 0;
	}

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, label, data, rb_poeport_groups);

	if (IS_ERR(hwmon_dev))
		dev_dbg(dev, "unable to register hwmon device\n");

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct of_device_id rb_poeport_of_match[] = {
	{ .compatible = "mikrotik,poeport" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rb_poeport_of_match);

static struct platform_driver rb_poeport_driver = {
	.probe = rb_poeport_probe,
	.driver = {
		.name = "rb-poeport-driver",
		.of_match_table = of_match_ptr(rb_poeport_of_match),
	},
};

module_platform_driver(rb_poeport_driver);

MODULE_AUTHOR("Oskari Lemmela <oskari@lemmela.net>");
MODULE_DESCRIPTION("Mikrotik poeport driver");
MODULE_LICENSE("GPL");
