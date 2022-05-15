/*
 *  linux/drivers/mmc/core/mmc_vsm.h
 *
 *  Copyright (C) 2017 Ruyi Liu, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MMC_MMC_VSM_H
#define _MMC_MMC_VSM_H

#include <linux/scatterlist.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#define MMC_VSM_MODE_SET	0x10
#define MMC_NORMAL_MODE_SET	0x00

#define MMC_VSM_WRITE 	0x1
#define MMC_VSM_READ 	0x0

#define RESULT_OK		0
#define RESULT_FAIL		1
#define RESULT_UNSUP_HOST	2
#define RESULT_UNSUP_CARD	3

#define CUSTOM_CMD_60		60

#define VSM_SAMSUNG_ARG		0xC7810000
#define VSM_HYNIX_ARG_1		0x534D4900
#define VSM_HYNIX_ARG_2		0x48525054

#define NAND_INFO_STR_LEN	1536

int mmc_nand_info_get(struct mmc_card *card, char *buf, u8 *nand_info);
int mmc_test_simple_transfer(struct mmc_card *card,
	struct scatterlist *sg, unsigned sg_len, unsigned dev_addr,
	unsigned blocks, unsigned blksz, int write);

#define mmc_vsm_vivo_support(c)  ((c)->vsm_mask & 0x00000001)
#define mmc_vsm_card_support(c)	(((c)->ext_csd.supported_modes & 0x00000002))

#endif

