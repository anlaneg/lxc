/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef __LXC_ARGUMENTS_H
#define __LXC_ARGUMENTS_H

#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>

#include <lxc/lxccontainer.h>

#include "compiler.h"

struct lxc_arguments;

typedef int (*lxc_arguments_parser_t)(struct lxc_arguments *, int, char *);
typedef int (*lxc_arguments_checker_t)(const struct lxc_arguments *);

struct lxc_arguments {
	const char *help;//命令行帮助信息
	//显示帮助信息时，此helpfn将被调用
	void (*helpfn)(const struct lxc_arguments *);
	const char *progname;//程序名
	const struct option *options;//参数选项情况
	lxc_arguments_parser_t parser;//定制的其它选项解析
	lxc_arguments_checker_t checker;//定制的参数检查

	const char *name;//容器名称
	char *log_file;//日志文件名称
	char *log_priority;//日志优先级
	int quiet;//不输出任何信息
	int daemonize;
	const char *rcfile;//rc文件名称
	const char *console;
	const char *console_log;
	const char *pidfile;
	const char **lxcpath;//指定的container特别path
	int lxcpath_cnt;//lxcpath数组长度
	/* set to 0 to accept only 1 lxcpath, -1 for unlimited */
	int lxcpath_additional;

	/* for lxc-start */
	//指出共享的名称
	const char *share_ns[32]; /* size must be greater than LXC_NS_MAX */

	/* for lxc-console */
	unsigned int ttynum;
	char escape;

	/* for lxc-wait */
	char *states;
	long timeout;

	/* for lxc-autostart */
	int shutdown;

	/* for lxc-stop */
	int hardstop;
	int nokill;
	int nolock;
	int nowait;
	int reboot;

	/* for lxc-destroy */
	int force;

	/* close fds from parent? */
	bool close_all_fds;

	/* lxc-create */
	char *bdevtype/*块设备类型*/, *configfile/*配置文件路径*/, *template/*模板名称或路径*/;
	char *fstype;
	uint64_t fssize;
	char *lvname, *vgname, *thinpool;
	char *rbdname, *rbdpool;
	char *zfsroot, *lowerdir, *dir;

	/* lxc-execute and lxc-unshare */
	uid_t uid;
	gid_t gid;

	/* auto-start */
	int all;
	int ignore_auto;
	int list;
	char *groups; /* also used by lxc-ls */

	/* lxc-snapshot and lxc-copy */
	enum task {
		CLONE,
		DESTROY,
		LIST,
		RESTORE,
		SNAP,
		RENAME,
	} task;
	int print_comments;
	char *commentfile;
	char *newname;
	char *newpath;
	char *snapname;
	int keepdata;
	int keepname;
	int keepmac;
	int allowrunning;

	/* lxc-ls */
	char *ls_fancy_format;
	char *ls_filter;
	unsigned int ls_nesting; /* maximum allowed nesting level */
	bool ls_active;
	bool ls_fancy;
	bool ls_frozen;
	bool ls_line;
	bool ls_running;
	bool ls_stopped;
	bool ls_defined;

	/* lxc-copy */
	bool tmpfs;

	/* lxc-unshare */
	int flags;
	int want_default_mounts;
	const char *want_hostname;
	bool setuid;

	/* remaining arguments */
	char *const *argv;//提供的其它参数
	int argc;

	/* private arguments */
	void *data;
};

#define LXC_COMMON_OPTIONS                                                     \
	    { "name",        required_argument, 0, 'n'         },              \
	    { "help",        no_argument,       0, 'h'         },              \
	    { "usage",       no_argument,       0, OPT_USAGE   },              \
	    { "version",     no_argument,       0, OPT_VERSION },              \
	    { "quiet",       no_argument,       0, 'q'         },              \
	    { "logfile",     required_argument, 0, 'o'         },              \
	    { "logpriority", required_argument, 0, 'l'         },              \
	    { "lxcpath",     required_argument, 0, 'P'         },              \
	    { "rcfile",      required_argument, 0, OPT_RCFILE  },              \
	    { 0,             0,                 0, 0           }

/* option keys for long only options */
#define OPT_USAGE 0x1000
#define OPT_VERSION OPT_USAGE - 1
#define OPT_RCFILE OPT_USAGE - 2
#define OPT_SHARE_NET OPT_USAGE - 3
#define OPT_SHARE_IPC OPT_USAGE - 4
#define OPT_SHARE_UTS OPT_USAGE - 5
#define OPT_SHARE_PID OPT_USAGE - 6

__hidden extern int lxc_arguments_parse(struct lxc_arguments *args, int argc, char *const argv[]);

__hidden extern int lxc_arguments_str_to_int(struct lxc_arguments *args, const char *str);

__hidden extern bool lxc_setup_shared_ns(struct lxc_arguments *args, struct lxc_container *c);

#define lxc_info(arg, fmt, args...)                                                \
	do {                                                                       \
		if (!(arg)->quiet) {                                               \
			fprintf(stdout, "%s: " fmt "\n", (arg)->progname, ##args); \
		}                                                                  \
	} while (0)

#define lxc_error(arg, fmt, args...)                                               \
	do {                                                                       \
		if (!(arg)->quiet) {                                               \
			fprintf(stderr, "%s: " fmt "\n", (arg)->progname, ##args); \
		}                                                                  \
	} while (0)

#define lxc_sys_error(arg, fmt, args...)                                                     \
	do {                                                                                 \
		if (!(arg)->quiet) {                                                         \
			fprintf(stderr, "%s: " fmt "\n", (arg)->progname, ##args); \
		}                                                                            \
	} while (0)

#endif /* __LXC_ARGUMENTS_H */
