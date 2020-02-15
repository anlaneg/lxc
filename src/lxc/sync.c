/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "start.h"
#include "sync.h"
#include "utils.h"

lxc_log_define(sync, lxc);

//自fd中读取sync,检查sequence是否与sync一致
static int __sync_wait(int fd, int sequence)
{
	int sync = -1;
	ssize_t ret;

	//尝试读取fd,如果读取失败，则返回-1
	ret = lxc_read_nointr(fd, &sync, sizeof(sync));
	if (ret < 0) {
		SYSERROR("Sync wait failure");
		return -1;
	}

	//文件达到结尾
	if (!ret)
		return 0;

	//其它告警情况（可能写了多次，但一次读取）
	if ((size_t)ret != sizeof(sync)) {
		ERROR("Unexpected sync size: %zu expected %zu", (size_t)ret, sizeof(sync));
		return -1;
	}

	if (sync == LXC_SYNC_ERROR) {
		ERROR("An error occurred in another process "
		      "(expected sequence number %d)", sequence);
		return -1;
	}

	//sync与sequence不相同时返回-1
	if (sync != sequence) {
		ERROR("Invalid sequence number %d. Expected sequence number %d",
		      sync, sequence);
		return -1;
	}
	return 0;
}

//如果写入sequence失败，则返回-1,否则返回0
static int __sync_wake(int fd, int sequence)
{
	int sync = sequence;

	if (lxc_write_nointr(fd, &sync, sizeof(sync)) < 0) {
		SYSERROR("Sync wake failure");
		return -1;
	}
	return 0;
}

//向fd中写入sequence,然后阻塞自fd中读取数据，检查读取的值是否为sequence+1
//如果读取sequeuce+1或者达到文件结尾，则返回0,否则返回-1
static int __sync_barrier(int fd, int sequence)
{
    //如果wake失败，则返回-1
	if (__sync_wake(fd, sequence))
		return -1;
	return __sync_wait(fd, sequence+1);
}

//通知父进程开始sequence阶段，等待父进程达到sequence阶段后开始下一步操作
int lxc_sync_barrier_parent(struct lxc_handler *handler, int sequence)
{
	return __sync_barrier(handler->sync_sock[0], sequence);
}

//通知子进程开始开始sequence阶段，等待子进程达到sequence阶段后开始下一步操作
int lxc_sync_barrier_child(struct lxc_handler *handler, int sequence)
{
	return __sync_barrier(handler->sync_sock[1], sequence);
}

int lxc_sync_wake_parent(struct lxc_handler *handler, int sequence)
{
	return __sync_wake(handler->sync_sock[0], sequence);
}

int lxc_sync_wait_parent(struct lxc_handler *handler, int sequence)
{
	return __sync_wait(handler->sync_sock[0], sequence);
}


int lxc_sync_wait_child(struct lxc_handler *handler, int sequence)
{
	return __sync_wait(handler->sync_sock[1], sequence);
}

int lxc_sync_wake_child(struct lxc_handler *handler, int sequence)
{
	return __sync_wake(handler->sync_sock[1], sequence);
}

//初始化一组同步socket
int lxc_sync_init(struct lxc_handler *handler)
{
	int ret;

	//创建同步socket
	ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, handler->sync_sock);
	if (ret) {
		SYSERROR("failed to create synchronization socketpair");
		return -1;
	}

	/* Be sure we don't inherit this after the exec */
	fcntl(handler->sync_sock[0], F_SETFD, FD_CLOEXEC);

	return 0;
}

void lxc_sync_fini_child(struct lxc_handler *handler)
{
	if (handler->sync_sock[0] != -1) {
		close(handler->sync_sock[0]);
		handler->sync_sock[0] = -1;
	}
}

//子进程不使用sync_socket[1]，故关闭sync_sock[1]
void lxc_sync_fini_parent(struct lxc_handler *handler)
{
	if (handler->sync_sock[1] != -1) {
		close(handler->sync_sock[1]);
		handler->sync_sock[1] = -1;
	}
}

void lxc_sync_fini(struct lxc_handler *handler)
{
	lxc_sync_fini_child(handler);
	lxc_sync_fini_parent(handler);
}
