/******************************************************************************
 *
 *   Copyright © Darren Hart, 2014
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
 * NAME
 *      futex_wait_thp.c
 *
 * DESCRIPTION
 *      Excercise futexes mapped from transparent huge pages.
 *
 * AUTHOR
 *      Darren Hart <dvhart@infradead.org>
 *
 *****************************************************************************/

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <libgen.h>
#include <unistd.h>
#include "futextest.h"
#include "logging.h"


/* FIXME: determine THP size at runtime? */
#define HUGE_PAGE_SIZE (8 * 1024 * 1024)
#define TIMEOUT_NS 100

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
	printf("  -v L	Verbosity level: %d=QUIET %d=CRITICAL %d=INFO\n",
	       VQUIET, VCRITICAL, VINFO);
}

int main(int argc, char *argv[])
{
	long huge_page_size = HUGE_PAGE_SIZE;
	int ret = RET_PASS;
	struct timespec to;
	long page_size;
	void *thp_buf;
	futex_t *f1;
	int res;
	char c;

	/* initialize timeout */
	to.tv_sec = 0;
	to.tv_nsec = TIMEOUT_NS;

	while ((c = getopt(argc, argv, "ch:v:")) != -1) {
		switch(c) {
		case 'c':
			log_color(1);
			break;
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		case 'v':
			log_verbosity(atoi(optarg));
			break;
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	page_size = sysconf(_SC_PAGESIZE);

	printf("%s: Excercise futexes mapped from transparent huge pages\n", basename(argv[0]));
	printf("    huge page size: %ld\n", huge_page_size);
	printf("    page size: %ld\n", page_size);
	printf("    iterations: %ld\n", huge_page_size / page_size);

	thp_buf = memalign(huge_page_size, huge_page_size);
	/* FIXME: Verify a hugepage was created */

	f1 = (futex_t *)thp_buf;

	while ((void *)f1 < thp_buf + huge_page_size) {
		info("Calling futex_wait on thp futex: %p\n", f1);
		res = futex_wait(f1, *f1, &to, FUTEX_PRIVATE_FLAG);
		if (!res || errno != ETIMEDOUT) {
			fail("futex_wait returned %d\n", ret < 0 ? errno : ret);
			ret = RET_FAIL;
			break;
		}
		f1 = (futex_t *)((void *)f1 + page_size);
	}

	print_result(ret);
	return ret;
}
