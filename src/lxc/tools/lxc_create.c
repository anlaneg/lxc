/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <lxc/lxccontainer.h>

#include "arguments.h"
#include "config.h"
#include "log.h"
#include "storage_utils.h"
#include "utils.h"

lxc_log_define(lxc_create, lxc);

static int my_parser(struct lxc_arguments *args, int c, char *arg);
static void create_helpfn(const struct lxc_arguments *args);
static bool validate_bdev_args(struct lxc_arguments *args);

static const struct option my_longopts[] = {
	{"bdev", required_argument, 0, 'B'},
	{"config", required_argument, 0, 'f'},
	{"template", required_argument, 0, 't'},
	{"lvname", required_argument, 0, '0'},
	{"vgname", required_argument, 0, '1'},
	{"thinpool", required_argument, 0, '2'},
	{"fstype", required_argument, 0, '3'},
	{"fssize", required_argument, 0, '4'},
	{"zfsroot", required_argument, 0, '5'},
	{"dir", required_argument, 0, '6'},
	{"rbdname", required_argument, 0, '7'},
	{"rbdpool", required_argument, 0, '8'},
	LXC_COMMON_OPTIONS
};

/*lxc-create对应的命令行参数信息*/
static struct lxc_arguments my_args = {
	.progname     = "lxc-create",
	.helpfn       = create_helpfn,
	.help         = "\
--name=NAME --template=TEMPLATE [OPTION...] [-- template-options]\n\
\n\
lxc-create creates a container\n\
\n\
Options :\n\
  -n, --name=NAME               NAME of the container\n\
  -f, --config=CONFIG           Initial configuration file\n\
  -t, --template=TEMPLATE       Template to use to setup container\n\
  -B, --bdev=BDEV               Backing store type to use\n\
      --dir=DIR                 Place rootfs directory under DIR\n\
\n\
  BDEV options for LVM (with -B/--bdev lvm):\n\
      --lvname=LVNAME           Use LVM lv name LVNAME\n\
                                (Default: container name)\n\
      --vgname=VG               Use LVM vg called VG\n\
                                (Default: lxc)\n\
      --thinpool=TP             Use LVM thin pool called TP\n\
                                (Default: lxc)\n\
\n\
  BDEV options for Ceph RBD (with -B/--bdev rbd) :\n\
      --rbdname=RBDNAME         Use Ceph RBD name RBDNAME\n\
                                (Default: container name)\n\
      --rbdpool=POOL            Use Ceph RBD pool name POOL\n\
                                (Default: lxc)\n\
\n\
  BDEV option for ZFS (with -B/--bdev zfs) :\n\
      --zfsroot=PATH            Create zfs under given zfsroot\n\
                                (Default: tank/lxc)\n\
\n\
  BDEV options for LVM or Loop (with -B/--bdev lvm/loop) :\n\
      --fstype=TYPE             Create fstype TYPE\n\
                                (Default: ext4)\n\
      --fssize=SIZE[U]          Create filesystem of\n\
                                size SIZE * unit U (bBkKmMgGtT)\n\
                                (Default: 1G, default unit: M)\n\
  -- template-options\n\
         This will pass template-options to the template as arguments.\n\
         To see the list of options supported by the template,\n\
         you can run lxc-create -t TEMPLATE -h.\n",
	.options      = my_longopts,
	.parser       = my_parser,
	.checker      = NULL,
	.log_priority = "ERROR",
	.log_file     = "none",
};

//定制的选项解析函数
static int my_parser(struct lxc_arguments *args, int c, char *arg)
{
	switch (c) {
	case 'B':
	    //块设备类型
		args->bdevtype = arg;
		break;
	case 'f':
	    //配置文件名称
		args->configfile = arg;
		break;
	case 't':
	    //模块路径或名称
		args->template = arg;
		break;
	case '0':
		args->lvname = arg;
		break;
	case '1':
		args->vgname = arg;
		break;
	case '2':
		args->thinpool = arg;
		break;
	case '3':
		args->fstype = arg;
		break;
	case '4':
		args->fssize = get_fssize(arg);
		break;
	case '5':
		args->zfsroot = arg;
		break;
	case '6':
		args->dir = arg;
		break;
	case '7':
		args->rbdname = arg;
		break;
	case '8':
		args->rbdpool = arg;
		break;
	}
	return 0;
}

//显示lxc-create命令对应的独特帮助信息
static void create_helpfn(const struct lxc_arguments *args)
{
	char *argv[3], *path;
	pid_t pid;

	//如果模板为空，则直接返回
	if (!args->template)
		return;

	pid = fork();
	if (pid) {
	    //父进程等待子进程退出
		(void)wait_for_pid(pid);
		return;
	}

	//子进程开始处理

	//取模板文件的绝对路径
	path = get_template_path(args->template);

	argv[0] = path;
	argv[1] = "-h";
	argv[2] = NULL;

	//执行此模板文件的帮助信息
	execv(path, argv);
	ERROR("Error executing %s -h", path);
	_exit(EXIT_FAILURE);
}

//块设备参数校验
static bool validate_bdev_args(struct lxc_arguments *args)
{
	if (strncmp(args->bdevtype, "best", strlen(args->bdevtype)) != 0) {
		if (args->fstype || args->fssize)
			if (strncmp(args->bdevtype, "lvm", strlen(args->bdevtype)) != 0 &&
			    strncmp(args->bdevtype, "loop", strlen(args->bdevtype)) != 0 &&
			    strncmp(args->bdevtype, "rbd", strlen(args->bdevtype)) != 0) {
				ERROR("Filesystem type and size are only valid with block devices");
				return false;
			}

		if (strncmp(args->bdevtype, "lvm", strlen(args->bdevtype)) != 0)
			if (args->lvname || args->vgname || args->thinpool) {
				ERROR("--lvname, --vgname and --thinpool are only valid with -B lvm");
				return false;
			}

		if (strncmp(args->bdevtype, "rbd", strlen(args->bdevtype)) != 0)
			if (args->rbdname || args->rbdpool) {
				ERROR("--rbdname and --rbdpool are only valid with -B rbd");
				return false;
			}

		if (strncmp(args->bdevtype, "zfs", strlen(args->bdevtype)) != 0)
			if (args->zfsroot) {
				ERROR("zfsroot is only valid with -B zfs");
				return false;
			}
	}

	return true;
}

//负责创建container
int main(int argc, char *argv[])
{
	struct lxc_container *c;
	struct bdev_specs spec;
	struct lxc_log log;
	int flags = 0;

	//lxc-create参数解析
	if (lxc_arguments_parse(&my_args, argc, argv))
		exit(EXIT_FAILURE);

	log.name = my_args.name;
	log.file = my_args.log_file;
	log.level = my_args.log_priority;
	log.prefix = my_args.progname;
	log.quiet = my_args.quiet;
	log.lxcpath = my_args.lxcpath[0];

	if (lxc_log_init(&log))
		exit(EXIT_FAILURE);

	if (!my_args.template) {
	    /*必须指定模块名称*/
		ERROR("A template must be specified");
		ERROR("Use \"none\" if you really want a container without a rootfs");
		exit(EXIT_FAILURE);
	}

	if (my_args.dir && my_args.dir[0] != '/') {
		ERROR("--dir should use absolute path");
		exit(EXIT_FAILURE);
	}

	//如果指定模板为'none',则认为未指定模板
	if (strncmp(my_args.template, "none", strlen(my_args.template)) == 0)
		my_args.template = NULL;

	/*如未指定块设备类型，则使用_unset*/
	if (!my_args.bdevtype)
		my_args.bdevtype = "_unset";

	if (!validate_bdev_args(&my_args))
		exit(EXIT_FAILURE);

	if (strncmp(my_args.bdevtype, "none", strlen(my_args.bdevtype)) == 0)
		my_args.bdevtype = "dir";

	/* Final check whether the user gave use a valid bdev type. */
	if (strncmp(my_args.bdevtype, "best", strlen(my_args.bdevtype)) != 0 &&
	    strncmp(my_args.bdevtype, "_unset", strlen(my_args.bdevtype)) != 0 &&
	    !is_valid_storage_type(my_args.bdevtype)) {
		ERROR("%s is not a valid backing storage type", my_args.bdevtype);
		exit(EXIT_FAILURE);
	}

	/*如未指定lxcpath,则使用配置文件中的lxc.lxcpath*/
	if (!my_args.lxcpath[0])
		my_args.lxcpath[0] = lxc_get_global_config_item("lxc.lxcpath");

	//确保lxcpath存在
	if (mkdir_p(my_args.lxcpath[0], 0755))
		exit(EXIT_FAILURE);

	//非root用户，检查对lxcpath的访问权限
	if (geteuid())
		if (access(my_args.lxcpath[0], O_RDONLY) < 0) {
			ERROR("You lack access to %s", my_args.lxcpath[0]);
			exit(EXIT_FAILURE);
		}

	//构造container对象
	c = lxc_container_new(my_args.name, my_args.lxcpath[0]);
	if (!c) {
		ERROR("Failed to create lxc container");
		exit(EXIT_FAILURE);
	}

	//检查container是否已被定义，如已定义则报错
	if (c->is_defined(c)) {
		lxc_container_put(c);
		ERROR("Container already exists");
		exit(EXIT_FAILURE);
	}

	//指定了配置文件，则为容器加载配置文件
	if (my_args.configfile)
		c->load_config(c, my_args.configfile);
	else
	    /*未指定，则加载默认配置文件*/
		c->load_config(c, lxc_get_global_config_item("lxc.default_config"));

	memset(&spec, 0, sizeof(spec));

	if (my_args.fstype)
		spec.fstype = my_args.fstype;

	if (my_args.fssize)
		spec.fssize = my_args.fssize;

	if ((strncmp(my_args.bdevtype, "zfs", strlen(my_args.bdevtype)) == 0) ||
	    (strncmp(my_args.bdevtype, "best", strlen(my_args.bdevtype)) == 0))
		if (my_args.zfsroot)
			spec.zfs.zfsroot = my_args.zfsroot;

	if ((strncmp(my_args.bdevtype, "lvm", strlen(my_args.bdevtype)) == 0) ||
	    (strncmp(my_args.bdevtype, "best", strlen(my_args.bdevtype)) == 0)) {
		if (my_args.lvname)
			spec.lvm.lv = my_args.lvname;

		if (my_args.vgname)
			spec.lvm.vg = my_args.vgname;

		if (my_args.thinpool)
			spec.lvm.thinpool = my_args.thinpool;
	}

	if ((strncmp(my_args.bdevtype, "rbd", strlen(my_args.bdevtype)) == 0) ||
	    (strncmp(my_args.bdevtype, "best", strlen(my_args.bdevtype)) == 0)) {
		if (my_args.rbdname)
			spec.rbd.rbdname = my_args.rbdname;

		if (my_args.rbdpool)
			spec.rbd.rbdpool = my_args.rbdpool;
	}

	if (my_args.dir)
		spec.dir = my_args.dir;

	if (strncmp(my_args.bdevtype, "_unset", strlen(my_args.bdevtype)) == 0)
		my_args.bdevtype = NULL;

	if (my_args.quiet)
		flags = LXC_CREATE_QUIET;

	//创建container
	if (!c->create(c, my_args.template, my_args.bdevtype, &spec, flags, &argv[optind])) {
		ERROR("Failed to create container %s", c->name);
		lxc_container_put(c);
		exit(EXIT_FAILURE);
	}

	lxc_container_put(c);
	exit(EXIT_SUCCESS);
}
