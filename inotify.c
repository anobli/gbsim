/*
 * Greybus Simulator
 *
 * Copyright 2014 Google Inc.
 * Copyright 2014 Linaro Ltd.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <linux/types.h>

#include "gbsim.h"

#define MAX_NAME 256
#define INOTIFY_EVENT_SIZE  ( sizeof(struct inotify_event) )
#define INOTIFY_EVENT_BUF   ( INOTIFY_EVENT_SIZE + MAX_NAME + 1 )

static pthread_t inotify_pthread;
int notify_fd = -ENXIO;
static char root[256];

static struct greybus_manifest_header *get_manifest_blob(char *mnfs)
{
	struct greybus_manifest_header *mh;
	int mnf_fd;
	ssize_t n;
	__le16 file_size;
	uint16_t size;

	if ((mnf_fd = open(mnfs, O_RDONLY)) < 0) {
		gbsim_error("failed to open manifest blob %s\n", mnfs);
		return NULL;
	}

	/* First just get the size */
	if ((n = read(mnf_fd, &file_size, 2)) != 2) {
		gbsim_error("failed to read manifest size, read %zd\n", n);
		goto out;
	}
	size = le16toh(file_size);

	/* Size has to cover at least itself */
	if (size < 2) {
		gbsim_error("bad manifest size %hu\n", size);
		goto out;
	}

	/* Allocate a big enough buffer */
	if (!(mh = malloc(size))) {
		gbsim_error("failed to allocate manifest buffer\n");
		goto out;
	}

	/* Now go back and read the whole thing */
	if (lseek(mnf_fd, 0, SEEK_SET)) {
		gbsim_error("failed to seek to front of manifest\n");
		goto out_free;
	}
	if (read(mnf_fd, mh, size) != size) {
		gbsim_error("failed to read manifest\n");
		goto out_free;
	}
	close(mnf_fd);

	return mh;
out_free:
	free(mh);
out:
	close(mnf_fd);

	return NULL;
}

static void *inotify_thread(void *param)
{
	char buffer[16 * INOTIFY_EVENT_BUF];
	ssize_t length;
	int i;

	(void) param;
	do {
		size_t size;

		length = read(notify_fd, buffer, sizeof(buffer));
		if (length < 0) {
			gbsim_error("inotify read: %s\n", strerror(errno));
			return NULL;
		}
		for (i = 0; i < length; i += size) {
			struct inotify_event *event = (struct inotify_event *)&buffer[i];
			size = sizeof(*event);
			if (length - i < size) {
				gbsim_error("inotify: partial event: %zd < %zu\n",
					length - i, size);
				return NULL;
			}

			if (!event->len)
				continue;

			size += event->len;
			if (i + size > length) {
				gbsim_error("inotify: short event: %zd < %zu\n",
					length - i, size);
				return NULL;
			}

			if (event->mask & IN_CLOSE_WRITE) {
				char mnfs[256];
				struct greybus_manifest_header *mh;
				strcpy(mnfs, root);
				strcat(mnfs, "/");
				strcat(mnfs, event->name);
				mh = get_manifest_blob(mnfs);
				if (mh) {
					interface.manifest = mh;
					interface.manifest_size = le16toh(mh->size);
					manifest_parse(mh, le16toh(mh->size));
				}
			}
		}
	} while (length >= 0);

	return NULL;
}

int inotify_start(char *base_dir)
{
	int ret;
	struct stat root_stat;
	int notify_wd;

	/* Our inotify directory */
	strcpy(root, base_dir);
	strcat(root, "/");
	strcat(root, "hotplug-module");

	ret = stat(root, &root_stat);
	if (ret < 0 || !S_ISDIR(root_stat.st_mode) || access(root, R_OK|W_OK) < 0) {
		gbsim_error("invalid base directory %s\n", root);
		exit(EXIT_FAILURE);
	}

	if ((notify_fd = inotify_init()) < 0)
		perror("inotify init failed");

	if ((notify_wd = inotify_add_watch(notify_fd, root, IN_CLOSE_WRITE|IN_DELETE)) < 0)
		perror("inotify add watch failed");

	ret = pthread_create(&inotify_pthread, NULL, inotify_thread, NULL);
	if (ret < 0) {
		perror("can't create inotify thread");
		exit(EXIT_FAILURE);
	}

	return 0;
}
