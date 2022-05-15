/*
 * Driver for get who delete or rename file
 *
 * Copyright (C) 2020 vivo Co., Ltd.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/sizes.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <uapi/linux/android/binder.h>

#define MAX_FILEOBSER	10
#define MAX_FILETYPE	100

static DECLARE_KFIFO(kfdel_fifo, unsigned char, SZ_512K);
spinlock_t kfifo_lock;

struct kernel_filedel {
	struct dentry *filedel_debugfs_root;
	int filedelete_started;
} kdel;

char *filedel_dirs[MAX_FILEOBSER] = {
	"DCIM",
	"Pictures",
	"Movies",
	"whatsapp",
};

char *file_types[MAX_FILETYPE] = {
	".jpeg",
	".jpg",
	".gif",
	".png",
	".bmp",
	".wbmp",
	".webp",
	".heif",
	".heic",
	".dng",
	".cr2",
	".nef",
	".nrw",
	".arw",
	".rw2",
	".orf",
	".raf",
	".pef",
	".srw",
	".mp4",
	".m4v",
	".3gp",
	".mov",
	".wmv",
	".asf",
	".mkv",
	".avi",
	".webm",
	".divx",
	".flv",
	".rm",
	".tp",
	".ts",
	".m2ts",
	".mpg",
	".vob",
	".srt",
	".ass",
	".ssa",
	".mp3",
	".m4a",
	".wav",
	".amr",
	".awb",
	".wma",
	".ogg",
	".aac",
	".mka",
	".flac",
	".dts",
	".3gpa",
	".ac3",
	".qcp",
	".pcm",
	".ec3",
	".aiff",
	".ape",
	".dsd",
	".mid",
	".smf",
	".imy",
	".lrc",
};

static inline char *get_pathname(char *buf, int buflen, struct dentry *d)
{
	char *path;

	if (likely(!IS_ERR(d))) {
		path = dentry_path_raw(d, buf, buflen);
		if (unlikely(IS_ERR(path))) {
			path = NULL;
		}
	} else {
		path = NULL;
	}

	return path;
}

int is_end_with(const char *str1, char *str2)
{
	int len1, len2;

	if (str1 == NULL || str2 == NULL)
		return -EINVAL;

	len1 = strlen(str1);
	len2 = strlen(str2);

	if ((len1 < len2) ||  (len1 == 0 || len2 == 0))
		return -EINVAL;

	while (len2 >= 1) {
		if (str2[len2 - 1] != str1[len1 - 1])
			return -EINVAL;
		len2--;
		len1--;
	}

	return 0;
}

static bool path_in_fileobserver(char *path)
{
	int i, path_in = 0;

	if (!path)
		return false;

	for (i = 0; i < MAX_FILEOBSER; i++)
		if (filedel_dirs[i]) {
			if (strnstr(path, filedel_dirs[i], strlen(path))) {
				path_in = 1;
				break;
			}
		}

	if (!path_in)
		return false;

	/*someone access filedel_dirs */
	for (i = 0; i < MAX_FILETYPE; i++)
		if (file_types[i])
			if (!is_end_with(path, file_types[i])) {
				pr_info("fileobs %s need to be record\n", path);
				return true;
			}

	return false;
}

int filedelete_writefile(struct dentry *old_dir, struct dentry *new_dir)
{
	int ret = 0, buflen = 0;
	char *old_path = NULL;
	char *new_path = NULL;
	char old_pathbuf[SZ_128] = {0};
	char new_pathbuf[SZ_128] = {0};
	char buf[SZ_256] = {0};

	char *package_name = NULL;
	struct timespec ts;
	struct tm tm;
	uid_t uid;

	struct task_struct *caller = NULL;
	char *caller_name = NULL;

	if (!kdel.filedelete_started)
		return 0;

	if (!old_dir)
		return -EINVAL;

	old_path = get_pathname(old_pathbuf, SZ_128, old_dir);

	if (!path_in_fileobserver(old_path))
		return 0;

	uid = from_kuid_munged(current_user_ns(), current_uid());
	package_name = get_packagename(uid);

	if (package_name) {
		if (strnstr(package_name, "com.android.providers", strlen(package_name))) {
			caller = get_caller_task();
			if (caller) {
				uid = from_kuid_munged(current_user_ns(), task_uid(caller));
				caller_name = get_packagename(uid);
			}
		}

		if (caller_name) {
			snprintf(buf, SZ_256 - strlen(buf), "%s|", caller_name);
			kfree(caller_name);

		} else
			snprintf(buf, SZ_256 - strlen(buf), "%s|", package_name);
		kfree(package_name);

	} else {
		snprintf(buf, SZ_256 - strlen(buf), "%s|", current->comm);
	}

	if (!new_dir) {
		getnstimeofday(&ts);
		time_to_tm((ts.tv_sec - (sys_tz.tz_minuteswest * 60)), 0, &tm);
		snprintf(buf + strlen(buf), SZ_256 - strlen(buf), "%ld-%d-%d %d:%d:%d|unlink|%s\n",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
			 tm.tm_min, tm.tm_sec, old_path);
	} else {
		getnstimeofday(&ts);
		time_to_tm((ts.tv_sec - (sys_tz.tz_minuteswest * 60)), 0, &tm);

		new_path = get_pathname(new_pathbuf, SZ_128, new_dir);
		if (new_path) {
			snprintf(buf + strlen(buf), SZ_256 - strlen(buf),
				 "%ld-%d-%d %d:%d:%d|rename|%s|%s\n",
				 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				 tm.tm_hour, tm.tm_min, tm.tm_sec, old_path, new_path);
		}
	}

	buflen = strlen(buf);
	kfifo_in_spinlocked(&kfdel_fifo, buf, buflen, &kfifo_lock);

	return ret;
}
EXPORT_SYMBOL(filedelete_writefile);

static ssize_t filedelete_start_write(struct file *file,
		const char __user *data, size_t count, loff_t *ppos)
{
	char buffer[SZ_256] = {0}, name[SZ_256] = {0};
	char *p_buffer = buffer, *token = NULL;
	unsigned int dir = 0;
	unsigned int filetype = 0;
	ssize_t ret = 0;
	int typename = 0;

	ret = simple_write_to_buffer(buffer, sizeof(buffer), ppos, data, count);
	if (ret <= 0)
		return ret;

	if (kdel.filedelete_started) {
		pr_info("fileobs fileobser_started\n");
		return count;
	}

	token = strsep(&p_buffer, " ");
	if (!token)
		return -EINVAL;

	if (sscanf(token, "%d", &kdel.filedelete_started) == 1) {
		pr_info("kfo.fileobser_started\n");
	}

	do {
		token = strsep(&p_buffer, " ");
		if (!token)
			break;

		if (sscanf(token, "%s", name) == 1)
			if (!strncmp(name, "filetype", 8))
				typename = 1;
		if (typename) {
			if (filetype < MAX_FILETYPE)
				file_types[filetype++] = kstrdup(name, GFP_KERNEL);
		} else {
			if (dir < MAX_FILEOBSER)
				filedel_dirs[dir++] = kstrdup(name, GFP_KERNEL);
		}
	} while (token);

	return count;
}

static ssize_t filedelete_start_read(struct file *file,
		char __user *data, size_t count, loff_t *ppos)
{
	char buf[SZ_128] = {0,};
	int buf_curseg = 0;
	int i;

	buf_curseg = snprintf(buf, SZ_128 - buf_curseg, "filedelete started is %d\n", kdel.filedelete_started);
	buf_curseg += snprintf(buf + buf_curseg, SZ_128 - buf_curseg, "Observed Patch:\n");

	for (i = 0; i < MAX_FILEOBSER; i++)
		if (filedel_dirs[i])
			buf_curseg += snprintf(buf + buf_curseg, SZ_128 - buf_curseg, "%s\n", filedel_dirs[i]);

	return simple_read_from_buffer(data, count, ppos, buf, buf_curseg);
}

static ssize_t filedelete_log_read(struct file *file,
		char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	size_t fifo_len = 0;
	unsigned int copied;

	if (!kdel.filedelete_started)
		return -EINVAL;
	fifo_len = kfifo_len(&kfdel_fifo);

	if (kfifo_to_user(&kfdel_fifo, buf, min(fifo_len, count),
			  &copied) != 0) {
		pr_info("fileobs read kfobs faild\n");
		ret = -EIO;
	} else
		ret = (ssize_t)copied;

	return ret;
}

static const struct file_operations filedelete_start_fops = {
	.read = filedelete_start_read,
	.write = filedelete_start_write,
};

static const struct file_operations filedelete_log_fops = {
	.read = filedelete_log_read,
};

static int filedel_init_debugfs(void)
{
	struct dentry *filedel_start = NULL;
	struct dentry *filedel_log = NULL;

	kdel.filedel_debugfs_root = debugfs_create_dir("filedelete", NULL);
	if (!kdel.filedel_debugfs_root) {
		pr_info("create fileobs dir failed\n");
		return -ENOMEM;
	}

	filedel_start = debugfs_create_file("filedelete_start", 0644,
			kdel.filedel_debugfs_root, NULL, &filedelete_start_fops);
	if (!filedel_start) {
		pr_info("create delete_start failed\n");
		if (kdel.filedel_debugfs_root)
			debugfs_remove(kdel.filedel_debugfs_root);
		return -ENOMEM;
	}

	filedel_log = debugfs_create_file("filedelete_log", 0444,
			kdel.filedel_debugfs_root, NULL, &filedelete_log_fops);
	if (!filedel_start) {
		pr_info("create obser_stop failed\n");
		if (filedel_start)
			debugfs_remove(filedel_start);
		if (kdel.filedel_debugfs_root)
			debugfs_remove(kdel.filedel_debugfs_root);

		return -ENOMEM;
	}

	return 0;

}

static int __init filedelete_record_init(void)
{
	int ret;

	ret = filedel_init_debugfs();
	if (ret)
		return ret;

	INIT_KFIFO(kfdel_fifo);
	spin_lock_init(&kfifo_lock);

	kdel.filedelete_started = 0;

	return 0;
}

static void __exit filedelete_record_exit(void)
{
	debugfs_remove_recursive(kdel.filedel_debugfs_root);
}

module_init(filedelete_record_init);
module_exit(filedelete_record_exit);
MODULE_AUTHOR("xirui.zhang@vivo.com>");
