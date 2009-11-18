/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2009
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
 *      futex_requeue_pi_mismatched_ops.c
 *
 * DESCRIPTION
 *      1. Block a thread using FUTEX_WAIT
 *      2. Attempt to use FUTEX_CMP_REQUEUE_PI on the futex from 1.
 *      3. The kernel must detect the mismatch and return -EINVAL.
 *
 * AUTHOR
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2009-Nov-9: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "futextest.h"
#include "logging.h"

futex_t f1 = FUTEX_INITIALIZER;
futex_t f2 = FUTEX_INITIALIZER;
int child_ret = 0;

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
	printf("  -v L	Verbosity level: %d=QUIET %d=CRITICAL %d=INFO\n",
	       VQUIET, VCRITICAL, VINFO);
}

void *blocking_child(void *arg)
{
	child_ret = futex_wait(&f1, f1, NULL, FUTEX_PRIVATE_FLAG);
	if (child_ret < 0) {
		child_ret = -errno;
		error("futex_wait\n", errno);
	}
	return (void *)&child_ret;
}

int main(int argc, char *argv[])
{
	pthread_t child;
	int ret = 0;
	int c;

	while ((c = getopt(argc, argv, "chv:")) != -1) {
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

	printf("%s: Detect mismatched requeue_pi operations\n",
	       basename(argv[0]));

	ret = pthread_create(&child, NULL, blocking_child, NULL);
	if (ret) {
		error("pthread_create\n", errno);
		goto out;
	}
	/* Allow the child to block in the kernel. */
	sleep(1);
	
	/*
	 * The kernel should detect the waiter did not setup the
	 * q->requeue_pi_key and return -EINVAL. If it does not,
	 * it likely gave the lock to the child, which is now hung
	 * in the kernel.
	 */
	ret = futex_cmp_requeue_pi(&f1, f1, &f2, 1, 0, FUTEX_PRIVATE_FLAG);
	if (ret < 0) {
		ret = -errno;
		if (ret == -EINVAL) {
			/* 
			 * The kernel correctly detected the mismatched
			 * requeue_pi target and aborted. Wake the child with
			 * FUTEX_WAKE.
			 */
			ret = futex_wake(&f1, 1, FUTEX_PRIVATE_FLAG);
			if (ret == 1)
				ret = 0;
			else if (ret < 0)
				error("futex_wake\n", errno);
			else {
				error("futex_wake did not wake the child\n", 0);
				ret = -1;
			}
		} else {
			error("futex_cmp_requeue_pi\n", errno);
		}
	} else if (ret > 0) {
		fail("futex_cmp_requeue_pi failed to detect the mismatch\n");
	} else {
		error("futex_cmp_requeue_pi found no waiters\n", 0);
		ret = -1;
	}

	pthread_join(child, NULL);

	if (!ret)
		ret = child_ret;


 out:
	/* If the kernel crashes, we shouldn't return at all. */
	printf("Result: %s\n", ret == 0 ? PASS : FAIL);
	return ret;
}
