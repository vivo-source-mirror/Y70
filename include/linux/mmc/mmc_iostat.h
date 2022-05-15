#ifndef _LINUX_MMC_IOSTAT_H
#define _LINUX_MMC_IOSTAT_H

#include <linux/mmc/core.h>

#define MMC_MAX_REQ_COUNT 4096
#define	MMC_STAT_SIZE_MAX 3

#define IOSTAT_ERR_UNKNOW -1
#define IOSTAT_ERR_NONE 0

typedef enum{
	CMD_READ,
	CMD_WRITE,
	CMD_TYPE_MAX,
} cmd_types;

typedef enum{
	DCMD_DISCARD,
	DCMD_FLUSH,
	DCMD_TYPE_MAX,
} dcmd_types;

typedef enum{
	CHUNK_4K,
	CHUNK_512K,
	CHUNK_OTHER,
	CHUNK_MAX,
} chunk_types;

typedef struct {
	unsigned long long	latency;
	unsigned long long	issue_time;
	unsigned long long	complete_time;
	sector_t 			lba;		/* sector cursor */
	unsigned int 		size;		/* total data len */
	int 			cmd_type;
} io_ele_t;

typedef struct {
	unsigned long long		lat_max;
	unsigned long long		lat_total;
	unsigned long long		lat_avg;
	unsigned long long		size_total;
	unsigned long long 		count;
} io_stat_info_t;

typedef struct {
	io_ele_t				recent_io[MMC_MAX_REQ_COUNT];
	unsigned int 			pos;
} io_recent_stat_t;

typedef struct {
	
	io_stat_info_t 			data_stat_info[MMC_STAT_SIZE_MAX][CMD_TYPE_MAX]; /* write/read I/O */
	io_stat_info_t 			other_stat_info[DCMD_TYPE_MAX]; /* discard/flush I/O */
	io_recent_stat_t		recent_stat;
	unsigned long long 		total_count;
	spinlock_t				lock;
} io_stat_t;

extern int mmc_iostat_add(struct mmc_card *card, struct mmc_request *mrq);

extern void mmc_iostat_init(struct mmc_card *card);

extern void mmc_iostat_deinit(struct mmc_card *card);

extern int mmc_io_stats_debugfs(struct mmc_card *card);


#endif
