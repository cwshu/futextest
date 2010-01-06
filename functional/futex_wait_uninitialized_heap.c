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
 *      futex_wait_uninitialized_heap.c
 *
 * DESCRIPTION
 *      Wait on uninitialized heap. It shold be zero and FUTEX_WAIT should return
 *      immediately. This test is intent to test zero page handling in futex.
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
#include <sys/mman.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <linux/futex.h>
#include <libgen.h>

#include "logging.h"
#include "futextest.h"

int main(int argc, char **argv)
{
	long page_size;
	int res;
	void *buf;
	int ret = RET_PASS;
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

	page_size = sysconf(_SC_PAGESIZE);

	buf = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	if (buf == (void *)-1) {
		perror("mmap error.\n");
		exit(1);
	}

	printf("%s: Test the uninitialized futex value in FUTEX_WAIT\n",
	       basename(argv[0]));

	res = futex_wait(buf, 1, NULL, 0);
	if (res != 0 && errno != EWOULDBLOCK) {
		ret = RET_FAIL;
	}
	print_result(ret);

	return ret;
}
