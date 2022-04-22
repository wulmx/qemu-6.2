// SPDX-License-Identifier: GPL-2.0+
/*
 * Atmel PIO4 pinctrl driver
 *
 * Copyright (C) 2016 Atmel Corporation
 *               Wenyou.Yang <wenyou.yang@atmel.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/pinctrl.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <mach/atmel_pio4.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Warning:
 * In order to not introduce confusion between Atmel PIO groups and pinctrl
 * framework groups, Atmel PIO groups will be called banks.
 */

struct atmel_pio4_plat {
	struct atmel_pio4_port *reg_base;
	unsigned int slew_rate_support;
};

static const struct pinconf_param conf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1 },
	{ "drive-open-drain", PIN_CONFIG_DRIVE_OPEN_DRAIN, 0 },
	{ "input-schmitt-disable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0 },
	{ "input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1 },
	{ "input-debounce", PIN_CONFIG_INPUT_DEBOUNCE, 0 },
	{ "atmel,drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
	{ "slew-rate", PIN_CONFIG_SLEW_RATE, 0},
};

static u32 atmel_pinctrl_get_pinconf(struct udevice *config,
				     struct atmel_pio4_plat *plat)
{
	const struct pinconf_param *params;
	u32 param, arg, conf = 0;
	u32 i;
	u32 val;

	for (i = 0; i < ARRAY_SIZE(conf_params); i++) {
		params = &conf_params[i];
		if (!dev_read_prop(config, params->property, NULL))
			continue;

		param = params->param;
		arg = params->default_value;

		/* Keep slew rate enabled by default. */
		if (plat->slew_rate_support)
			conf |= ATMEL_PIO_SR;

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			conf &= (~ATMEL_PIO_PUEN_MASK);
			conf &= (~ATMEL_PIO_PDEN_MASK);
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			conf |= ATMEL_PIO_PUEN_MASK;
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			conf |= ATMEL_PIO_PDEN_MASK;
			break;
		case PIN_CONFIG_DRIVE_OPEN_DRAIN:
			if (arg == 0)
				conf &= (~ATMEL_PIO_OPD_MASK);
			else
				conf |= ATMEL_PIO_OPD_MASK;
			break;
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			if (arg == 0)
				conf |= ATMEL_PIO_SCHMITT_MASK;
			else
				conf &= (~ATMEL_PIO_SCHMITT_MASK);
			break;
		case PIN_CONFIG_INPUT_DEBOUNCE:
			if (arg == 0) {
				conf &= (~ATMEL_PIO_IFEN_MASK);
				conf &= (~ATMEL_PIO_IFSCEN_MASK);
			} else {
				conf |= ATMEL_PIO_IFEN_MASK;
				conf |= ATMEL_PIO_IFSCEN_MASK;
			}
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			dev_read_u32(config, params->property, &val);
			conf &= (~ATMEL_PIO_DRVSTR_MASK);
			conf |= (val << ATMEL_PIO_DRVSTR_OFFSET)
				& ATMEL_PIO_DRVSTR_MASK;
			break;
		case PIN_CONFIG_SLEW_RATE:
			if (!plat->slew_rate_support)
				break;

			dev_read_u32(config, params->property, &val);
			/* And disable it if requested. */
			if (val == 0)
				conf &= ~ATMEL_PIO_SR;
			break;
		default:
			printf("%s: Unsupported configuration parameter: %u\n",
			       __func__, param);
			break;
		}
	}

	return conf;
}

static inline struct atmel_pio4_port *atmel_pio4_bank_base(struct udevice *dev,
							   u32 bank)
{
	struct atmel_pio4_plat *plat = dev_get_plat(dev);
	struct atmel_pio4_port *bank_base =
			(struct atmel_pio4_port *)((u32)plat->reg_base +
			ATMEL_PIO_BANK_OFFSET * bank);

	return bank_base;
}

#define MAX_PINMUX_ENTRIES	40

static int atmel_pinctrl_set_state(struct udevice *dev, struct udevice *config)
{
	struct atmel_pio4_plat *plat = dev_get_plat(dev);
	struct atmel_pio4_port *bank_base;
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(config);
	u32 offset, func, bank, line;
	u32 cells[MAX_PINMUX_ENTRIES];
	u32 i, conf;
	int count;

	conf = atmel_pinctrl_get_pinconf(config, plat);

	count = fdtdec_get_int_array_count(blob, node, "pinmux",
					   cells, ARRAY_SIZE(cells));
	if (count < 0) {
		printf("%s: bad pinmux array %d\n", __func__, count);
		return -EINVAL;
	}

	if (count > MAX_PINMUX_ENTRIES) {
		printf("%s: unsupported pinmux array count %d\n",
		       __func__, count);
		return -EINVAL;
	}

	for (i = 0 ; i < count; i++) {
		offset = ATMEL_GET_PIN_NO(cells[i]);
		func = ATMEL_GET_PIN_FUNC(cells[i]);

		bank = ATMEL_PIO_BANK(offset);
		line = ATMEL_PIO_LINE(offset);

		bank_base = atmel_pio4_bank_base(dev, bank);

		writel(BIT(line), &bank_base->mskr);
		conf &= (~ATMEL_PIO_CFGR_FUNC_MASK);
		conf |= (func & ATMEL_PIO_CFGR_FUNC_MASK);
		writel(conf, &bank_base->cfgr);
	}

	return 0;
}

const struct pinctrl_ops atmel_pinctrl_ops  = {
	.set_state = atmel_pinctrl_set_state,
};

static int atmel_pinctrl_probe(struct udevice *dev)
{
	struct atmel_pio4_plat *plat = dev_get_plat(dev);
	ulong priv = dev_get_driver_data(dev);
	fdt_addr_t addr_base;

	dev = dev_get_parent(dev);
	addr_base = dev_read_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->reg_base = (struct atmel_pio4_port *)addr_base;
	plat->slew_rate_support = priv;

	return 0;
}

static const struct udevice_id atmel_pinctrl_match[] = {
	{ .compatible = "atmel,sama5d2-pinctrl" },
	{ .compatible = "microchip,sama7g5-pinctrl",
	  .data = (ulong)1, },
	{}
};

U_BOOT_DRIVER(atmel_pinctrl) = {
	.name = "pinctrl_atmel_pio4",
	.id = UCLASS_PINCTRL,
	.of_match = atmel_pinctrl_match,
	.probe = atmel_pinctrl_probe,
	.plat_auto	= sizeof(struct atmel_pio4_plat),
	.ops = &atmel_pinctrl_ops,
};
