/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>

#include "config.h"
#include "file_utils.h"
#include "log.h"
#include "macro.h"
#include "parse.h"
#include "syscall_wrappers.h"
#include "utils.h"

lxc_log_define(parse, lxc);

void *lxc_strmmap(void *addr, size_t length, int prot, int flags, int fd,
		  off_t offset)
{
	void *tmp = NULL, *overlap = NULL;

	/* We establish an anonymous mapping that is one byte larger than the
	 * underlying file. The pages handed to us are zero filled. */
	tmp = mmap(addr, length + 1, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (tmp == MAP_FAILED)
		return tmp;

	/* Now we establish a fixed-address mapping starting at the address we
	 * received from our anonymous mapping and replace all bytes excluding
	 * the additional \0-byte with the file. This allows us to use normal
	 * string-handling functions. */
	overlap = mmap(tmp, length, prot, MAP_FIXED | flags, fd, offset);
	if (overlap == MAP_FAILED)
		munmap(tmp, length + 1);

	return overlap;
}

int lxc_strmunmap(void *addr, size_t length)
{
	return munmap(addr, length + 1);
}

int lxc_file_for_each_line_mmap(const char *file, lxc_file_cb callback/*遍历回调函数*/, void *data)
{
	int saved_errno;
	ssize_t ret = -1, bytes_sent;
	char *line;
	int fd = -1, memfd = -1;
	char *buf = NULL;

	//创建memfd,创建一个在内存中的文件.lxc_config_file
	memfd = memfd_create(".lxc_config_file", MFD_CLOEXEC);
	if (memfd < 0) {
	    //创建memfd失败，改为创建临时文件
		char template[] = P_tmpdir "/.lxc_config_file_XXXXXX";

		if (errno != ENOSYS) {
			SYSERROR("Failed to create memory file");
			goto on_error;
		}

		TRACE("Failed to create in-memory file. Falling back to "
		      "temporary file");
		memfd = lxc_make_tmpfile(template, true);
		if (memfd < 0) {
			SYSERROR("Failed to create temporary file \"%s\"", template);
			goto on_error;
		}
	}

	//打开配置文件
	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		SYSERROR("Failed to open file \"%s\"", file);
		goto on_error;
	}

	/* sendfile() handles up to 2GB. No config file should be that big. */
	//将fd的内容复制到memfd中
	bytes_sent = lxc_sendfile_nointr(memfd, fd, NULL, LXC_SENDFILE_MAX);
	if (bytes_sent < 0) {
		SYSERROR("Failed to sendfile \"%s\"", file);
		goto on_error;
	}

	ret = lxc_write_nointr(memfd, "\0", 1);
	if (ret < 0) {
		SYSERROR("Failed to append zero byte");
		goto on_error;
	}
	bytes_sent++;

	//将memfd偏移移动到0位置
	ret = lseek(memfd, 0, SEEK_SET);
	if (ret < 0) {
		SYSERROR("Failed to lseek");
		goto on_error;
	}

	//将memfd映射到内存，mmap系统调用下去后仅替换ops即可
	ret = -1;
	buf = mmap(NULL, bytes_sent, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_POPULATE, memfd, 0);
	if (buf == MAP_FAILED) {
		buf = NULL;
		SYSERROR("Failed to mmap");
		goto on_error;
	}

	//buf按'\r\n\0'进行分行，针对每行数据执行callback
	ret = 0;
	lxc_iterate_parts(line, buf, "\r\n\0") {
		ret = callback(line, data);
		if (ret) {
			/* Callback rv > 0 means stop here callback rv < 0 means
			 * error.
			 */
			if (ret < 0)
				ERROR("Failed to parse config file \"%s\" at "
				      "line \"%s\"", file, line);
			break;
		}
	}

on_error:
	saved_errno = errno;
	if (fd >= 0)
		close(fd);
	if (memfd >= 0)
		close(memfd);
	if (buf && munmap(buf, bytes_sent)) {
		SYSERROR("Failed to unmap");
		if (ret == 0)
			ret = -1;
	}
	errno = saved_errno;

	return ret;
}

//打开file,针对file中的每一行，调用callback
int lxc_file_for_each_line(const char *file, lxc_file_cb callback, void *data)
{
	__do_fclose FILE *f = NULL;
	__do_free char *line = NULL;
	int err = 0;
	size_t len = 0;

	f = fopen(file, "re");
	if (!f) {
		SYSERROR("Failed to open \"%s\"", file);
		return -1;
	}

	//读取f中的每一行，针对每一行，执行callback
	while (getline(&line, &len, f) != -1) {
		err = callback(line, data);
		if (err) {
			/* Callback rv > 0 means stop here callback rv < 0 means
			 * error.
			 */
			if (err < 0)
				ERROR("Failed to parse config: \"%s\"", line);
			break;
		}
	}

	return err;
}
