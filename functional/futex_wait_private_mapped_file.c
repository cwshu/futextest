/******************************************************************************
 *
 * Copyright FUJITSU LIMITED 2010
 * Copyright KOSAKI Motohiro <kosaki.motohiro@jp.fujitsu.com>
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NAME
 *      futex_wait_private_mapped_file.c
 *
 * DESCRIPTION
 *	Internally, Futex has two handling mode, anon and file. The private file
 *	mapping is special. At first it behave as file, but after write anything
 *	it behave as anon. This test is intent to test such case.
 *
 * AUTHOR
 *      KOSAKI Motohiro <kosaki.motohiro@jp.fujitsu.com>
 *
 * HISTORY
 *      2010-Jan-6: Initial version by KOSAKI Motohiro <kosaki.motohiro@jp.fujitsu.com>
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <libgen.h>

#include "logging.h"
#include "futextest.h"

#define PAGE_SZ 4096

char pad[PAGE_SZ] = {1};
futex_t val = 1;
char pad2[PAGE_SZ] = {1};

void* thr_futex_wait(void *arg)
{
	int ret;

	info("futex wait\n");
	ret = futex_wait(&val, 1, NULL, 0);
	if (ret != 0 && errno != EWOULDBLOCK) {
		perror("futex error.\n");
		print_result(RET_ERROR);
		exit(RET_ERROR);
	}
	info("futex_wait: ret = %d, errno = %d\n", ret, errno);

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t thr;
	int ret = RET_PASS;
	int res;
	int c;

	while ((c = getopt(argc, argv, "c")) != -1) {
		switch(c) {
		case 'c':
			log_color(1);
			break;
		default:
			exit(1);
		}
	}

	printf("%s: Test the futex value of private file mappings in FUTEX_WAIT\n",
	       basename(argv[0]));

	ret = pthread_create(&thr, NULL, thr_futex_wait, NULL);
	if (ret < 0) {
		fprintf(stderr, "pthread_create error\n");
		goto error;
	}

	info("wait a while");
	sleep(3);
	val = 2;
	res = futex_wake(&val, 1, 0);
	info("futex_wake %d", res);

	info("join");
	pthread_join(thr, NULL);

	print_result(ret);
	return ret;

error:
	ret = RET_ERROR;
	print_result(ret);
	return ret;
}
