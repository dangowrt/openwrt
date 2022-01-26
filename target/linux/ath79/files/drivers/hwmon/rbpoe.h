/* SPDX-License-Identifier: GPL-2.0-only
 *
 * POE driver for the MikroTik RouterBoard series
 */
#include <linux/spi/spi.h>

#define MAX_RETRIES 10

struct rb_poe_model {
	bool reverse;
	u8 volt_lsb;
	u16 temp_lsb;
	u32 temp_offset;
};

struct rb_poe_data {
	struct spi_device *spi;
	struct rb_poe_model *data;

	struct mutex lock;
};

int rb_poe_write_cmd(struct rb_poe_data *data, u16 *resp, u8 cmd, u8 arg1, u8 arg2);
int rb_poe_get_port_idx(struct rb_poe_data *data, int reg);
int rb_poe_read_voltage(struct rb_poe_data *data);
