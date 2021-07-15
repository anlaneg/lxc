/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <lxc/lxccontainer.h>
#include <lxc/version.h>

#include "arguments.h"
#include "compiler.h"
#include "config.h"
#include "initutils.h"
#include "namespace.h"

//利用a_optsions选项数组，构造其对应的短选项字符串
static int build_shortopts(const struct option *a_options, char *a_shortopts,
			   size_t a_size)
{
	size_t i = 0;
	const struct option *opt;

	if (!a_options || !a_shortopts || !a_size)
		return -1;

	//遍历所有选项，如果选项有ascii码的opt val,则将其加入到短选项buffer中
	for (opt = a_options; opt->name; opt++) {
		if (!isascii(opt->val))
			continue;

		//shortopts空间可存放，存放
		if (i < a_size)
			a_shortopts[i++] = opt->val;
		else
		    //空间不足
			goto is2big;

		//如果此选项有参数，则添加':'
		if (opt->has_arg == no_argument)
			continue;

		if (i < a_size)
			a_shortopts[i++] = ':';
		else
			goto is2big;

		//如果此选项参数是可选的，则添加'::'
		if (opt->has_arg == required_argument)
			continue;

		if (i < a_size)
			a_shortopts[i++] = ':';
		else
			goto is2big;
	}

	//给定字符串结尾
	if (i < a_size)
		a_shortopts[i] = '\0';
	else
		goto is2big;

	return 0;

is2big:
	errno = E2BIG;
	return -1;
}

//显示longopts的信息，并退出
__noreturn static void print_usage_exit(const struct option longopts[],
					  const struct lxc_arguments *a_args)

{
	int i;
	const struct option *opt;

	//显示程序名称
	fprintf(stderr, "Usage: %s ", a_args->progname);

	//遍历显示各选项
	for (opt = longopts, i = 1; opt->name; opt++, i++) {
		fprintf(stderr, "[");

		//输出短选项
		if (isprint(opt->val))
			fprintf(stderr, "-%c|", opt->val);

		//输出长选项
		fprintf(stderr, "--%s", opt->name);

		if ((opt->has_arg == required_argument) ||
		    (opt->has_arg == optional_argument)) {
			int j;
			char *uppername;

			uppername = strdup(opt->name);
			if (!uppername)
				exit(-ENOMEM);

			//将选项名称转发为全大写
			for (j = 0; uppername[j]; j++)
				uppername[j] = toupper(uppername[j]);

			//必选项及可选项的格式化输出
			if (opt->has_arg == required_argument)
				fprintf(stderr, "=%s", uppername);
			else	// optional_argument
				fprintf(stderr, "[=%s]", uppername);

			free(uppername);
		}

		fprintf(stderr, "] ");

		//每4个选项，输出一个回车换行（4这个数字可做为一个参数传入）
		if (!(i % 4))
			fprintf(stderr, "\n\t");
	}

	fprintf(stderr, "\n");
	exit(EXIT_SUCCESS);
}

//显示版本号并退出
__noreturn static void print_version_exit(void)
{
	printf("%s\n", lxc_get_version());
	exit(EXIT_SUCCESS);
}

//显示帮助信息
__noreturn static void print_help_exit(const struct lxc_arguments *args/*helpfn将被调用*/,
					 int code/*退出码*/)
{
	fprintf(stderr, "\
Usage: %s %s\
\n\
Common options :\n\
  -o, --logfile=FILE               Output log to FILE instead of stderr\n\
  -l, --logpriority=LEVEL          Set log priority to LEVEL\n\
  -q, --quiet                      Don't produce any output\n\
  -P, --lxcpath=PATH               Use specified container path\n\
  -?, --help                       Give this help list\n\
      --usage                      Give a short usage message\n\
      --version                    Print the version number\n\
\n\
Mandatory or optional arguments to long options are also mandatory or optional\n\
for any corresponding short options.\n\
\n\
See the %s man page for further information.\n\n",
	args->progname, args->help, args->progname);

	if (args->helpfn)
		args->helpfn(args);

	exit(code);
}

//lxcpath路径添加
static int lxc_arguments_lxcpath_add(struct lxc_arguments *args,
				     const char *lxcpath)
{
	if (args->lxcpath_additional != -1 &&
	    args->lxcpath_cnt > args->lxcpath_additional) {
		fprintf(stderr,
			"This command only accepts %d -P,--lxcpath arguments\n",
			args->lxcpath_additional + 1);
		exit(EXIT_FAILURE);
	}

	//申请空间，存放lxcpath
	args->lxcpath = realloc(
	    args->lxcpath, (args->lxcpath_cnt + 1) * sizeof(args->lxcpath[0]));
	if (args->lxcpath == NULL) {
		lxc_error(args, "no memory");
		return -ENOMEM;
	}

	args->lxcpath[args->lxcpath_cnt++] = lxcpath;
	return 0;
}

//lxc参数解析
extern int lxc_arguments_parse(struct lxc_arguments *args, int argc,
			       char *const argv[])
{
	int ret = 0;
	bool logfile = false;
	char shortopts[256];

	//利用长选项，构造短选项
	ret = build_shortopts(args->options, shortopts, sizeof(shortopts));
	if (ret < 0) {
		lxc_error(args, "build_shortopts() failed : %s",
			  strerror(errno));
		return ret;
	}

	//解析所有参数
	for (;;) {
		int c;
		int index = 0;

		c = getopt_long(argc, argv, shortopts, args->options, &index);
		if (c == -1)
			break;

		switch (c) {
		case 'n':
		    /*用户配置的容器名称*/
			args->name = optarg;
			break;
		case 'o':
			args->log_file = optarg;
			logfile = true;
			break;
		case 'l':
			args->log_priority = optarg;
			if (!logfile &&
			    args->log_file &&
			    strcmp(args->log_file, "none") == 0)
			    args->log_file = NULL;
			break;
		case 'q':
			args->quiet = 1;
			break;
		case OPT_RCFILE:
			args->rcfile = optarg;
			break;
		case 'P':
		    //收集指定的特别container path
			remove_trailing_slashes(optarg);
			ret = lxc_arguments_lxcpath_add(args, optarg);
			if (ret < 0)
				return ret;
			break;
		case OPT_USAGE:
		    //显示用法信息并退出
			print_usage_exit(args->options, args);
		case OPT_VERSION:
		    //显示版本信息并退出
			print_version_exit();
		case '?':
			print_help_exit(args, 1);
		case 'h':
			print_help_exit(args, 0);
		default:
		    /*其它参数解析*/
			if (args->parser) {
				ret = args->parser(args, c, optarg);
				if (ret)
					goto error;
			}
		}
	}

	/*
	 * Reclaim the remaining command arguments
	 */
	args->argv = &argv[optind];
	args->argc = argc - optind;

	/* If no lxcpaths were given, use default */
	if (!args->lxcpath_cnt) {
	    /*用户没有指出lxcpath,添加默认path*/
		ret = lxc_arguments_lxcpath_add(
		    args, lxc_get_global_config_item("lxc.lxcpath"));
		if (ret < 0)
			return ret;
	}

	/* Check the command options */
	if (!args->name && strncmp(args->progname, "lxc-autostart", strlen(args->progname)) != 0
	                && strncmp(args->progname, "lxc-unshare", strlen(args->progname)) != 0) {
	    //未指定名称，针对lxc-autostart,lxc-unshare使用argv待解析的第一个参数做为name
		if (args->argv) {
			args->name = argv[optind];
			optind++;
			args->argv = &argv[optind];
			args->argc = argc - optind;
		}

		if (!args->name) {
		    //未给定名称，报错
			lxc_error(args, "No container name specified");
			return -1;
		}
	}

	//执行参数检查
	if (args->checker)
		ret = args->checker(args);

error:
    //解析出错检查
	if (ret)
		lxc_error(args, "could not parse command line");

	return ret;
}

int lxc_arguments_str_to_int(struct lxc_arguments *args, const char *str)
{
	long val;
	char *endptr;

	errno = 0;
	val = strtol(str, &endptr, 10);
	if (errno) {
		lxc_error(args, "invalid statefd '%s' : %s", str,
			  strerror(errno));
		return -1;
	}

	if (*endptr) {
		lxc_error(args, "invalid digit for statefd '%s'", str);
		return -1;
	}

	return (int)val;
}

//将share_ns转换为配置
bool lxc_setup_shared_ns(struct lxc_arguments *args, struct lxc_container *c)
{
	int i;

	for (i = 0; i < LXC_NS_MAX; i++) {
		const char *key, *value;

		value = args->share_ns[i];
		if (!value)
			continue;//跳过未配置共享的namespace

		if (i == LXC_NS_NET)
			key = "lxc.namespace.share.net";
		else if (i == LXC_NS_IPC)
			key = "lxc.namespace.share.ipc";
		else if (i == LXC_NS_UTS)
			key = "lxc.namespace.share.uts";
		else if (i == LXC_NS_PID)
			key = "lxc.namespace.share.pid";
		else
			continue;

		//设置key的配置项
		if (!c->set_config_item(c, key, value)) {
			lxc_error(args, "Failed to set \"%s = %s\"", key, value);
			return false;
		}
	}

	return true;
}
