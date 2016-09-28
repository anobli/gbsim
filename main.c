/*
 * Greybus Simulator
 *
 * Copyright 2014 Google Inc.
 * Copyright 2014 Linaro Ltd.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include "gbsim.h"
#include "socket.h"

int bbb_backend = 0;
int i2c_adapter = 0;
int uart_portno = 0;
int uart_count = 0;
char *mnfb = NULL;
int verbose = 1;

static struct sigaction sigact;

struct gbsim_interface interface;
extern AvahiSimplePoll *simple_poll;
static void cleanup(void)
{
	printf("cleaning up\n");
	sigemptyset(&sigact.sa_mask);

	avahi_simple_poll_quit(simple_poll);
	uart_cleanup();
	sockets_close();
	svc_exit();
}

static void signal_handler(int sig)
{
	if (sig == SIGINT || sig == SIGHUP || sig == SIGTERM)
		cleanup();
}

static void signals_init(void)
{
	sigact.sa_handler = signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, (struct sigaction *)NULL);
	sigaction(SIGHUP, &sigact, (struct sigaction *)NULL);
	sigaction(SIGTERM, &sigact, (struct sigaction *)NULL);
}

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

int main(int argc, char *argv[])
{
	int o;
	struct greybus_manifest_header *mh;

	while ((o = getopt(argc, argv, ":bm:i:u:U:v")) != -1) {
		switch (o) {
		case 'b':
			bbb_backend = 1;
			printf("bbb_backend %d\n", bbb_backend);
			break;
		case 'm':
			mnfb = optarg;
			printf("manifest %s\n", mnfb);
			break;
		case 'i':
			i2c_adapter = atoi(optarg);
			printf("i2c_adapter %d\n", i2c_adapter);
			break;
		case 'u':
			uart_portno = atoi(optarg);
			printf("uart_portno %d\n", uart_portno);
			break;
		case 'U':
			uart_count = atoi(optarg);
			printf("uart_count %d\n", uart_count);
			break;
		case 'v':
			verbose = 1;
			printf("verbose %d\n", verbose);
			break;
		case ':':
			if (optopt == 'i')
				gbsim_error("i2c_adapter required\n");
			else if (optopt == 'h')
				gbsim_error("hotplug_basedir required\n");
			else if (optopt == 'u')
				gbsim_error("uart_portno required\n");
			else if (optopt == 'U')
				gbsim_error("uart_count required\n");
			else
				gbsim_error("-%c requires an argument\n",
					optopt);
			return 1;
		case '?':
			if (isprint(optopt))
				gbsim_error("unknown option -%c'\n", optopt);
			else
				gbsim_error("unknown option character 0x%02x\n",
					optopt);
			return 1;
		default:
			abort();
		}
	}

	if (!mnfb) {
		gbsim_error("manifest not specified, aborting\n");
		return 1;
	}

	signals_init();

	TAILQ_INIT(&interface.connections);

	socket_init();

	/* Protocol handlers */
	gpio_init();
	i2c_init();
	uart_init();
	sdio_init();
	loopback_init();

	mh = get_manifest_blob(mnfb);
	if (!mh) {
		gbsim_error("Failed to load manifest, aborting\n");
		return 1;
	}
	manifest_parse(mh, le16toh(mh->size));

	socket_loop();

	avahi_main();

	return 0;
}

