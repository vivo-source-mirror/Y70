#include <linux/mmc/mmc_iostat.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/sort.h>

static unsigned char *chunk_types_str[CHUNK_MAX] = {
	"4K",
	"512K",
	"OTHR",
};

static unsigned char *cmd_type_str[CMD_TYPE_MAX] = {
	"r",
	"w",
};

static unsigned char *dcmd_type_str[DCMD_TYPE_MAX] = {
	"d",
	"f",
};

void mmc_iostat_init(struct mmc_card *card)
{
	card->io_stats = kzalloc(sizeof(io_stat_t), GFP_KERNEL);
	spin_lock_init(&card->io_stats->lock);
}

void mmc_iostat_deinit(struct mmc_card *card)
{
	kfree(card->io_stats);
}

int mmc_iostat_add(struct mmc_card *card, struct mmc_request *mrq)
{
	unsigned long long now = sched_clock();
	unsigned long long latency = 0;
	unsigned int 	pos = 0;
	chunk_types chunk_size = CHUNK_MAX;
	int type = -1;
	unsigned long flags;

	spin_lock_irqsave(&card->io_stats->lock, flags);
	latency = (now - mrq->cmdq_req->start_time)/1000;

	if (mrq->cmdq_req->cmdq_req_flags & DCMD) {
		if (mrq->cmd && mrq->cmd->opcode == MMC_SWITCH)
			type = DCMD_FLUSH;
		else if (mrq->cmd && mrq->cmd->opcode == MMC_ERASE)
			type = DCMD_DISCARD;
		else {
			spin_unlock_irqrestore(&card->io_stats->lock, flags);
			return IOSTAT_ERR_UNKNOW;
		}
		pos = card->io_stats->recent_stat.pos;
		if (latency > card->io_stats->other_stat_info[type].lat_max)
			card->io_stats->other_stat_info[type].lat_max = latency;
		card->io_stats->other_stat_info[type].count++;
		card->io_stats->other_stat_info[type].lat_total += latency;
		card->io_stats->other_stat_info[type].lat_avg = 
			card->io_stats->other_stat_info[type].lat_total/card->io_stats->other_stat_info[type].count;
		card->io_stats->recent_stat.recent_io[pos].lba = 0;
		card->io_stats->recent_stat.recent_io[pos].size = 0;
		goto out;
	}

	if (mrq->data) {
		type = (mrq->data->flags & MMC_DATA_READ) ? CMD_READ : CMD_WRITE;
		if ((mrq->req->__data_len)>>9 == 8)
			chunk_size = CHUNK_4K;
		else if ((mrq->req->__data_len)>>9 == 1024)
			chunk_size = CHUNK_512K;
		else
			chunk_size = CHUNK_OTHER;
		pos = card->io_stats->recent_stat.pos;
		if (latency > card->io_stats->data_stat_info[chunk_size][type].lat_max)
			card->io_stats->data_stat_info[chunk_size][type].lat_max = latency;
		card->io_stats->data_stat_info[chunk_size][type].size_total += (mrq->req->__data_len)>>9;
		card->io_stats->data_stat_info[chunk_size][type].count++;
		card->io_stats->data_stat_info[chunk_size][type].lat_total += latency;
		card->io_stats->data_stat_info[chunk_size][type].lat_avg
			= card->io_stats->data_stat_info[chunk_size][type].lat_total/card->io_stats->data_stat_info[chunk_size][type].count, 
		card->io_stats->recent_stat.recent_io[pos].lba = mrq->req->__sector;
		card->io_stats->recent_stat.recent_io[pos].size = (mrq->req->__data_len)>>9;
	} else {
		spin_unlock_irqrestore(&card->io_stats->lock, flags);
		return IOSTAT_ERR_UNKNOW;
	}

out:
	card->io_stats->recent_stat.recent_io[pos].latency = latency;
	card->io_stats->recent_stat.recent_io[pos].issue_time = mrq->cmdq_req->start_time;
	card->io_stats->recent_stat.recent_io[pos].complete_time = now;
	card->io_stats->recent_stat.recent_io[pos].cmd_type = type;
	card->io_stats->total_count++;
	if (card->io_stats->recent_stat.pos < MMC_MAX_REQ_COUNT - 1)
		card->io_stats->recent_stat.pos++;
	else
		card->io_stats->recent_stat.pos = 0;
	spin_unlock_irqrestore(&card->io_stats->lock, flags);

	return IOSTAT_ERR_NONE;
}

static int mmc_io_stat_show(struct seq_file *s, void *data)
{
	struct mmc_card *card = s->private;
	int i, j;
	unsigned long flags;
	unsigned long long start = sched_clock();

	seq_printf(s, "%-10s %16s %16s %16s %16s\n",
		"Item", "IO Count", "Size(MB)", "Latency Avg(us)", "Latency Max(us)");
	spin_lock_irqsave(&card->io_stats->lock, flags);
	for (i = 0; i < CHUNK_MAX; i++)
		for (j = 0; j < CMD_TYPE_MAX; j++) {
		seq_printf(s, "[%4s][%s] %16u %16llu %16llu %16llu\n", 
		chunk_types_str[i], cmd_type_str[j], 
		card->io_stats->data_stat_info[i][j].count, 
		card->io_stats->data_stat_info[i][j].size_total >> 11,
		card->io_stats->data_stat_info[i][j].lat_total/card->io_stats->data_stat_info[i][j].count, 
		card->io_stats->data_stat_info[i][j].lat_max);
	}

	for (i = 0; i < DCMD_TYPE_MAX; i++) {
		seq_printf(s, "[%7s] %16u %16llu %16llu %16llu\n", 
		dcmd_type_str[i], 
		card->io_stats->other_stat_info[i].count, 
		card->io_stats->other_stat_info[i].size_total,
		card->io_stats->other_stat_info[i].lat_avg, 
		card->io_stats->other_stat_info[i].lat_max);
	}
	spin_unlock_irqrestore(&card->io_stats->lock, flags);
	seq_printf(s, "time cost: %lld\n", sched_clock() - start);

	return IOSTAT_ERR_NONE;
}

static int mmc_io_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_io_stat_show, inode->i_private);
}

static const struct file_operations mmc_dbg_io_stat_fops = {
	.open		= mmc_io_stat_open,
	.read		= seq_read,
	.release	= single_release,
	.llseek		= seq_lseek,
};

#define CMP_NEG -1
#define CMP_POS 1
static int cmd_cmp_func(const void *a, const void *b)
{
	const io_ele_t *pa = a;
	const io_ele_t *pb = b;

	if (pa->latency < pb->latency)
		return CMP_POS;
	else if (pa->latency > pb->latency)
		return CMP_NEG;
	else
		return 0;
}

static void swap_cmd(void *a, void *b, int size)
{
	io_ele_t *l = a, *r = b;
	io_ele_t tmp;

	tmp = *l;
	*l = *r;
	*r = tmp;
}

static int mmc_io_stat_recent_show(struct seq_file *s, void *data)
{
	struct mmc_card *card = s->private;
	io_ele_t *cmd = NULL;
	int i = 0;
	unsigned long long lat_total = 0, size_total = 0;
	unsigned long long lat_avg = 0, first_time = 0, last_time = 0;
	unsigned long long write_count = 0, read_count = 0;
	unsigned long long flush_count = 0, discard_count = 0;
	unsigned long long write_size = 0, read_size = 0;
	unsigned long flags;
	unsigned long long start = sched_clock();

	cmd = kmalloc(MMC_MAX_REQ_COUNT * sizeof(io_ele_t), GFP_KERNEL);
	if (!cmd) {
		seq_printf(s, "kmalloc failed\n");
		return IOSTAT_ERR_NONE;
	}
	spin_lock_irqsave(&card->io_stats->lock, flags);
	memcpy(cmd, card->io_stats->recent_stat.recent_io, MMC_MAX_REQ_COUNT * sizeof(io_ele_t));
	spin_unlock_irqrestore(&card->io_stats->lock, flags);
	for (i = 0; i < MMC_MAX_REQ_COUNT; i++) {
			lat_total += cmd[i].latency;
			size_total += cmd[i].size;
			if (cmd[i].cmd_type == CMD_READ && cmd[i].size != 0) {
				read_count++;
				read_size += cmd[i].size;
				
			}

			if (cmd[i].cmd_type == CMD_WRITE && cmd[i].size != 0) {
				write_count++;
				write_size += cmd[i].size;
			}

			if (cmd[i].cmd_type == DCMD_DISCARD && cmd[i].size == 0) {
				discard_count++;
			}

			if (cmd[i].cmd_type == DCMD_FLUSH && cmd[i].size == 0) {
				flush_count++;
			}
	}
	lat_avg = lat_total/MMC_MAX_REQ_COUNT;

	if (card->io_stats->recent_stat.pos != 0)
		last_time = cmd[card->io_stats->recent_stat.pos - 1].issue_time;
	else
		last_time = cmd[MMC_MAX_REQ_COUNT - 1].issue_time;

	if (card->io_stats->recent_stat.pos != MMC_MAX_REQ_COUNT - 1)
		first_time = cmd[card->io_stats->recent_stat.pos].issue_time;
	else
		first_time = cmd[0].issue_time;

	sort(cmd, MMC_MAX_REQ_COUNT, sizeof(io_ele_t), cmd_cmp_func, swap_cmd);

	seq_printf(s, "first time stamp(ns): %llu\n", first_time);
	seq_printf(s, "last time stamp(ns): %llu\n", last_time);
	seq_printf(s, "average latency(us): %llu\n", lat_avg);
	seq_printf(s, "recent size(MB): %llu\n", size_total >> 11);
	seq_printf(s, "recent write size(MB): %llu\n", write_size >> 11);
	seq_printf(s, "recent read size(MB): %llu\n", read_size >> 11);
	seq_printf(s, "recent write count: %llu\n", write_count);
	seq_printf(s, "recent read count: %llu\n", read_count);
	seq_printf(s, "%-4s %16s %16s %16s\n",
		"type", "lba", "Size(sector)", "Latency(us)");
	for (i = 0; i < 30; i++)
		seq_printf(s, "[%2s] %16llu %16u %16llu\n", 
			cmd[i].size ? cmd_type_str[cmd[i].cmd_type] : dcmd_type_str[cmd[i].cmd_type],
			(unsigned long long)cmd[i].lba,
			cmd[i].size,
			cmd[i].latency);
	seq_printf(s, "time cost: %lld\n", sched_clock() - start);

	kfree(cmd);
	return IOSTAT_ERR_NONE;
}

static int mmc_io_stat_recent_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_io_stat_recent_show, inode->i_private);
}

static const struct file_operations mmc_dbg_io_stat_recent_fops = {
	.open		= mmc_io_stat_recent_open,
	.read		= seq_read,
	.release	= single_release,
	.llseek		= seq_lseek,
};

int mmc_io_stats_debugfs(struct mmc_card *card)
{
	if (!debugfs_create_file("iostat", S_IRUSR, card->debugfs_root, card,
				&mmc_dbg_io_stat_fops))
		goto err;

	if (!debugfs_create_file("iostat_recent", S_IRUSR, card->debugfs_root, card,
				&mmc_dbg_io_stat_recent_fops))
		goto err;

	return IOSTAT_ERR_NONE;
err:
	dev_err(&card->dev,
			"Can't create the file. Perhaps debugfs is disabled.\n");
	return -ENODEV;
}
EXPORT_SYMBOL(mmc_io_stats_debugfs);

