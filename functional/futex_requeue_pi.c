/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2006-2008
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
 *      futex_requeue_pi.c
 *
 * DESCRIPTION
 *      This test excercises the futex syscall op codes needed for requeuing
 *      priority inheritance aware POSIX condition variables and mutexes.
 *
 * AUTHORS
 *	Sripathi Kodi <sripathik@in.ibm.com>
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2008-Jan-13: Initial version by Sripathi Kodi <sripathik@in.ibm.com>
 *      2009-Nov-6: futex test adaptation by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "futextest.h"

#define PRIVATE 1
#ifdef PRIVATE
#define FUTEX_PRIVATE_FLAG 128
#define PSHARED PTHREAD_PROCESS_PRIVATE
#else
#define FUTEX_PRIVATE_FLAG 0
#define PSHARED PTHREAD_PROCESS_SHARED
#endif

#define THREAD_MAX 10
#define SIGNAL_PERIOD_US 100

pthread_barrier_t wake_barrier;
pthread_barrier_t waiter_barrier;
int waiters_woken;
futex_t f1 = FUTEX_INITIALIZER;
futex_t f2 = FUTEX_INITIALIZER;

/* Test option defaults */
static long timeout_ns = 0;
static int broadcast = 0;
static int owner = 0;
static int locked = 0;

typedef struct struct_waiter_arg {
	long id;
	struct timespec *timeout;
} waiter_arg_t;

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -b	Broadcast wakeup (all waiters)\n");
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
	printf("  -l	Lock the pi futex across requeue\n");
	printf("  -o	Use a third party pi futex owner during requeue\n");
	printf("  -t N	Timeout in nanoseconds (default: 100,000)\n");
	printf("  -v L	Verbosity level: %d=QUIET %d=CRITICAL %d=INFO\n",
	       VQUIET, VCRITICAL, VINFO);
}

int create_rt_thread(pthread_t *pth, void*(*func)(void*), void *arg, int policy, int prio)
{
	int ret;
	struct sched_param schedp;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	memset(&schedp, 0, sizeof(schedp));

	if ((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0) {
		error("pthread_attr_setinheritsched\n", ret);
		return -1;
	}

	if ((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0) {
		error("pthread_attr_setschedpolicy\n", ret);
		return -1;
	}

	schedp.sched_priority = prio;
	if ((ret = pthread_attr_setschedparam(&attr, &schedp)) != 0) {
		error("pthread_attr_setschedparam\n", ret);
		return -1;
	}

	if ((ret = pthread_create(pth, &attr, func, arg)) != 0) {
		error("pthread_create\n", ret);
		return -1;
	}
	return 0;
}


void *waiterfn(void *arg)
{
	waiter_arg_t *args = (waiter_arg_t *)arg;
	unsigned int old_val;
	int ret;

	info("Waiter %ld: running\n", args->id);
	/* Each thread sleeps for a different amount of time
	 * This is to avoid races, because we don't lock the
	 * external mutex here */
	usleep(1000 * (long)args->id);

	/* FIXME: need to hold the mutex prior to waiting right?... sort of... */

	/* cond_wait */
	old_val = f1;
	pthread_barrier_wait(&waiter_barrier);
	info("Calling futex_wait_requeue_pi: %p (%u) -> %p\n",
	     &f1, f1, &f2);
	ret = futex_wait_requeue_pi(&f1, old_val, &f2,
				    args->timeout, FUTEX_PRIVATE_FLAG);
	info("waiter %ld woke\n", args->id);
	if (ret < 0) {
		if (args->timeout && errno == ETIMEDOUT)
			ret = 0;
		else {
			ret = -errno;
			error("futex_wait_requeue_pi\n", errno);
		}
		futex_lock_pi(&f2, NULL, 0, FUTEX_PRIVATE_FLAG);
	}
	waiters_woken++;
	futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);

	info("Waiter %ld: exiting with %d\n", args->id, ret);
	return (void*)(long)ret;
}

void *broadcast_wakerfn(void *arg)
{
	unsigned int old_val;
	int nr_wake = 1;
	int nr_requeue = INT_MAX;
	long lock = (long)arg;
	int ret = 0;
	pthread_barrier_wait(&waiter_barrier);
	usleep(100000); /*icky*/
	info("Waker: Calling broadcast\n");

	if (lock) {
		info("Calling FUTEX_LOCK_PI on mutex=%x @ %p\n", 
		     f2, &f2);
		futex_lock_pi(&f2, NULL, 0, FUTEX_PRIVATE_FLAG);
	}
	/* cond_broadcast */
	old_val = f1;
	ret = futex_cmp_requeue_pi(&f1, old_val, &f2, nr_wake,
				   nr_requeue, FUTEX_PRIVATE_FLAG);
	if (ret < 0) {
		ret = -errno;
		error("FUTEX_CMP_REQUEUE_PI failed\n", errno);
	}

	if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
		error("broadcast_wakerfn\n", errno);

	if (lock)
		futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);

	info("Waker: exiting with %d\n", ret);
	return (void *)(long)ret;;
}

void *signal_wakerfn(void *arg)
{
	long lock = (long)arg;
	unsigned int old_val;
	int nr_requeue = 0;
	int task_count = 0;
	int nr_wake = 1;
	int ret = 0;
	int i = 0;

	pthread_barrier_wait(&waiter_barrier);
	while (task_count < THREAD_MAX && waiters_woken < THREAD_MAX) {
		info("task_count: %d, waiters_woken: %d\n",
		     task_count, waiters_woken);
		if (lock) {
			info("Calling FUTEX_LOCK_PI on mutex=%x @ %p\n", 
			     f2, &f2);
			futex_lock_pi(&f2, NULL, 0, FUTEX_PRIVATE_FLAG);
		}
		info("Waker: Calling signal\n");
		/* cond_signal */
		old_val = f1;
		ret = futex_cmp_requeue_pi(&f1, old_val, &f2,
					   nr_wake, nr_requeue, FUTEX_PRIVATE_FLAG);
		if (ret < 0)
			ret = -errno;
		info("futex: %x\n", f2);
		if (lock) {
			info("Calling FUTEX_UNLOCK_PI on mutex=%x @ %p\n", 
			     f2, &f2);
			futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);
		}
		info("futex: %x\n", f2);
		if (ret < 0) {
			error("FUTEX_CMP_REQUEUE_PI failed\n", errno);
			break;
		}
		
		if (!i) {
			info("waker waiting on wake_barrier\n");
			if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
				error("signal_wakerfn", errno);
		}

		task_count += ret;
		usleep(SIGNAL_PERIOD_US);
		i++;
		if (i > 1000) {
			info("i>1000, giving up on pending waiters...\n");
			break;
		}
	}
	if (ret >= 0)
		ret = task_count;

	info("Waker: exiting with %d\n", ret);
	info("Waker: waiters_woken: %d\n", waiters_woken);
	return (void *)(long)ret;
}

void *third_party_blocker(void *arg)
{
	futex_lock_pi(&f2, NULL, 0, FUTEX_PRIVATE_FLAG);
	if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
		error("third_party_blocker\n", errno);
	futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);
	return NULL;
}

int unit_test(int broadcast, long lock, int third_party_owner, long timeout_ns)
{
	void *(*wakerfn)(void *) = signal_wakerfn;
	pthread_t waiter[THREAD_MAX], waker, blocker;
	waiter_arg_t args[THREAD_MAX];
	struct timespec ts, *tsp = NULL;
	int ret;
	long i;

	if (timeout_ns) {
		info("timeout_ns = %ld\n", timeout_ns);
		ret = clock_gettime(CLOCK_MONOTONIC, &ts);
		time_t secs = (ts.tv_nsec + timeout_ns) / 1000000000;
		ts.tv_nsec = ((int64_t)ts.tv_nsec + timeout_ns) % 1000000000;
		ts.tv_sec += secs;
		info("ts.tv_sec  = %ld\n", ts.tv_sec);
		info("ts.tv_nsec = %ld\n", ts.tv_nsec);
		tsp = &ts;
	}

	if ((ret = pthread_barrier_init(&wake_barrier, NULL,
					1+third_party_owner))) {
		error("pthread_barrier_init(wake_barrier) failed\n", errno);
		return ret;
	}
	if ((ret = pthread_barrier_init(&waiter_barrier, NULL,
					1+THREAD_MAX))) {
		error("pthread_barrier_init(waiter_barrier) failed\n", errno);
		return ret;
	}

	if (broadcast)
		wakerfn = broadcast_wakerfn;

	if (third_party_owner) {
		if ((ret = create_rt_thread(&blocker, third_party_blocker, NULL,
					    SCHED_FIFO, 1))) {
			error("Creating third party blocker thread failed\n",
			      errno);
			goto out;
		}
	}

	waiters_woken = 0;
	for (i = 0; i < THREAD_MAX; i++) {
		args[i].id = i;
		args[i].timeout = tsp;
		info("Starting thread %ld\n", i);
		if ((ret = create_rt_thread(&waiter[i], waiterfn, (void *)&args[i],
					    SCHED_FIFO, 1))) {
			error("Creating waiting thread failed\n", errno);
			goto out;
		}
	}
	if ((ret = create_rt_thread(&waker, wakerfn, (void *)lock,
				    SCHED_FIFO, 1))) {
		error("Creating waker thread failed\n", errno);
		goto out;
	}

	/* Wait for threads to finish */
	for (i=0; i<THREAD_MAX; i++) {
		pthread_join(waiter[i], NULL);
	}
	if (third_party_owner)
		pthread_join(blocker, NULL);
	pthread_join(waker, NULL);

out:
	if ((ret = pthread_barrier_destroy(&wake_barrier)))
		error("pthread_barrier_destroy(wake_barrier) failed\n", errno);
	if ((ret = pthread_barrier_destroy(&waiter_barrier)))
		error("pthread_barrier_destroy(waiter_barrier) failed\n",
		      errno);
	return ret;
}

int main(int argc, char *argv[])
{
	int c, ret;

	while ((c = getopt(argc, argv, "bchlot:v:")) != -1) {
		switch(c) {
		case 'b':
			broadcast = 1;
			break;
		case 'c':
			futextest_use_color(1);
			break;
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		case 'l':
			locked = 1;
			break;
		case 'o':
			owner = 1;
			break;
		case 't':
			timeout_ns = atoi(optarg);
			break;
		case 'v':
			futextest_verbosity(atoi(optarg));
			break;
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	printf("%s: Test requeue functionality\n", basename(argv[0]));
	printf("\tArguments: broadcast=%d locked=%d owner=%d timeout=%ldns\n",
	       broadcast, locked, owner, timeout_ns);

	/*
	 * FIXME: unit_test is obsolete now that we parse options and the
	 * various style of runs are done by run.sh - simplify the code and move
	 * unit_test into main()
	 */
	ret = unit_test(broadcast, locked, owner, timeout_ns);

	/* FIXME: need to distinguish between FAIL and ERROR */
	printf("Result: %s\n", ret ? ERROR : PASS);
	return ret;
}
