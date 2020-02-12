/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef __LXC_RSYNC_H
#define __LXC_RSYNC_H

#include <stdio.h>

struct rsync_data {
	struct lxc_storage *orig;
	struct lxc_storage *new;
};

struct rsync_data_char {
	char *src;
	char *dest;
};

/* new helpers */
extern int lxc_rsync_exec_wrapper(void *data);
extern int lxc_storage_rsync_exec_wrapper(void *data);
extern int lxc_rsync_exec(const char *src, const char *dest);
extern int lxc_rsync(struct rsync_data *data);

#endif /* __LXC_RSYNC_H */
