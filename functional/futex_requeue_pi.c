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

pthread_mutex_t mutex;
pthread_barrier_t wake_barrier;
pthread_barrier_t waiter_barrier;
int waiters_woken;
futex_t wait_q = FUTEX_INITIALIZER;

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
}

int create_pi_mutex(pthread_mutex_t *mutex)
{
	int ret;
	pthread_mutexattr_t mutexattr;

	if ((ret = pthread_mutexattr_init(&mutexattr)) != 0) {
		fprintf(stderr, "\t%s: pthread_mutexattr_init: %s\n",
			ERROR, strerror(ret));
		return -1;
	}
	if ((ret = pthread_mutexattr_setprotocol(&mutexattr, PTHREAD_PRIO_INHERIT)) != 0) {
		fprintf(stderr, "\t%s: pthread_mutexattr_setprotocol: %s\n",
			ERROR, strerror(ret));
		pthread_mutexattr_destroy(&mutexattr);
		return -1;
	}
	if ((ret = pthread_mutexattr_setpshared(&mutexattr, PSHARED)) != 0) {
		fprintf(stderr, "\t%s: pthread_mutexattr_setpshared(%d): %s\n",
			ERROR, PSHARED, strerror(ret));
		pthread_mutexattr_destroy(&mutexattr);
		return -1;
	}

	int pshared;
	pthread_mutexattr_getpshared(&mutexattr, &pshared);
	fprintf(stderr, "\tpshared set to %d\n", pshared);

	if ((ret = pthread_mutex_init(mutex, &mutexattr)) != 0) {
		printf("pthread_mutex_init: %s\n", strerror(ret));
		pthread_mutexattr_destroy(&mutexattr);
		return -1;
	}

	fprintf(stderr, "\tmutex.__data.__kind: %x\n", mutex->__data.__kind);

	return 0;
}

int create_rt_thread(pthread_t *pth, void*(*func)(void*), void *arg, int policy, int prio)
{
	int ret;
	struct sched_param schedp;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	memset(&schedp, 0, sizeof(schedp));

	if ((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setinheritsched: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	if ((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setschedpolicy: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	schedp.sched_priority = prio;
	if ((ret = pthread_attr_setschedparam(&attr, &schedp)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setschedparam: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	if ((ret = pthread_create(pth, &attr, func, arg)) != 0) {
		fprintf(stderr, "\t%s: pthread_create: %s\n",
			ERROR, strerror(ret));
		return -1;
	}
	return 0;
}


void *waiterfn(void *arg)
{
	waiter_arg_t *args = (waiter_arg_t *)arg;
	unsigned int old_val;
	int ret;

	fprintf(stderr, "\tWaiter %ld: running\n", args->id); fflush(stderr);
	/* Each thread sleeps for a different amount of time
	 * This is to avoid races, because we don't lock the
	 * external mutex here */
	usleep(1000 * (long)args->id);

	/* FIXME: need to hold the mutex prior to waiting right?... sort of... */

	/* cond_wait */
	old_val = wait_q;
	pthread_barrier_wait(&waiter_barrier);
	ret = futex_wait_requeue_pi(&wait_q, old_val, &(mutex.__data.__lock),
				    args->timeout, FUTEX_PRIVATE_FLAG);
	fprintf(stderr, "\twaiter %ld woke\n", args->id); fflush(stderr);
	if (ret < 0) {
		ret = -errno;
		fprintf(stderr, "\t%s: waiterfn: %s\n", ERROR, strerror(errno));
		pthread_mutex_lock(&mutex);
	}
	waiters_woken++;
	pthread_mutex_unlock(&mutex);

	fprintf(stderr, "\tWaiter %ld: exiting with %d\n", args->id, ret);
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
	fprintf(stderr, "\tWaker: Calling broadcast\n");

	if (lock) {
		fprintf(stderr, "\tCalling FUTEX_LOCK_PI on mutex=%x @ %p\n", 
			mutex.__data.__lock, &mutex.__data.__lock);
		pthread_mutex_lock(&mutex);
	}
	/* cond_broadcast */
	old_val = wait_q;
	ret = futex_cmp_requeue_pi(&wait_q, old_val, &(mutex.__data.__lock), nr_wake,
				   nr_requeue, FUTEX_PRIVATE_FLAG);
	if (ret < 0) {
		ret = -errno;
		fprintf(stderr, "\t%s: FUTEX_CMP_REQUEUE_PI failed: %s\n",
			ERROR, strerror(errno));
	}

	if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
		fprintf(stderr, "\t%s: broadcast_wakerfn: %s\n",
			ERROR, strerror(errno));

	if (lock)
		pthread_mutex_unlock(&mutex);

	fprintf(stderr, "\tWaker: exiting with %d\n", ret);
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
		fprintf(stderr, "\ttask_count: %d, waiters_woken: %d\n",
			task_count, waiters_woken);
		if (lock) {
			fprintf(stderr, "\tCalling FUTEX_LOCK_PI on mutex=%x @ %p\n", 
				mutex.__data.__lock, &mutex.__data.__lock);
			pthread_mutex_lock(&mutex);
		}
		fprintf(stderr, "\tWaker: Calling signal\n");
		/* cond_signal */
		old_val = wait_q;
		ret = futex_cmp_requeue_pi(&wait_q, old_val, &(mutex.__data.__lock),
					   nr_wake, nr_requeue, FUTEX_PRIVATE_FLAG);
		if (ret < 0)
			ret = -errno;
		fprintf(stderr, "\tfutex: %x\n", mutex.__data.__lock);
		if (lock) {
			fprintf(stderr, "\tCalling FUTEX_UNLOCK_PI on mutex=%x @ %p\n", 
				mutex.__data.__lock, &mutex.__data.__lock);
			pthread_mutex_unlock(&mutex);
		}
		fprintf(stderr, "\tfutex: %x\n", mutex.__data.__lock);
		if (ret < 0) {
			fprintf(stderr, "\t%s: FUTEX_CMP_REQUEUE_PI failed: %s\n",
				ERROR, strerror(errno));
			break;
		}
		
		if (!i) {
			fprintf(stderr, "\twaker waiting on wake_barrier\n");
			if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
				fprintf(stderr, "\t%s: signal_wakerfn: %s",
					ERROR, strerror(errno));
		}

		task_count += ret;
		usleep(SIGNAL_PERIOD_US);
		i++;
		if (i > 1000) {
			fprintf(stderr, "\ti>1000, giving up on pending waiters...\n");
			break;
		}
	}
	if (ret >= 0)
		ret = task_count;

	fprintf(stderr, "\tWaker: exiting with %d\n", ret);
	fprintf(stderr, "\tWaker: waiters_woken: %d\n", waiters_woken);
	return (void *)(long)ret;
}

void *third_party_blocker(void *arg)
{
	pthread_mutex_lock(&mutex);
	if (pthread_barrier_wait(&wake_barrier) == -EINVAL)
		fprintf(stderr, "\t%s: third_party_blocker: %s",
			ERROR, strerror(errno));
	pthread_mutex_unlock(&mutex);
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
		ret = clock_gettime(CLOCK_MONOTONIC, &ts);
		time_t secs = (ts.tv_nsec + timeout_ns) / 1000000000;
		ts.tv_nsec = (ts.tv_nsec + timeout_ns) % 1000000000;
		ts.tv_sec += secs;
		tsp = &ts;
	}

	if ((ret = pthread_barrier_init(&wake_barrier, NULL,
					1+third_party_owner))) {
		fprintf(stderr, "\t%s: pthread_barrier_init(wake_barrier) failed: %s\n",
			ERROR, strerror(errno));
		return ret;
	}
	if ((ret = pthread_barrier_init(&waiter_barrier, NULL,
					1+THREAD_MAX))) {
		fprintf(stderr, "\t%s: pthread_barrier_init(waiter_barrier) failed: %s\n",
			ERROR, strerror(errno));
		return ret;
	}

	if (broadcast)
		wakerfn = broadcast_wakerfn;

	if (third_party_owner) {
		if ((ret = create_rt_thread(&blocker, third_party_blocker, NULL,
					    SCHED_FIFO, 1))) {
			fprintf(stderr, "\t%s: Creating third party blocker thread failed: %s\n",
				ERROR, strerror(errno));
			goto out;
		}
	}

	waiters_woken = 0;
	for (i = 0; i < THREAD_MAX; i++) {
		args[i].id = i;
		args[i].timeout = tsp;
		fprintf(stderr, "\tStarting thread %ld\n", i); fflush(stderr);
		if ((ret = create_rt_thread(&waiter[i], waiterfn, (void *)&args[i],
					    SCHED_FIFO, 1))) {
			fprintf(stderr, "\t%s: Creating waiting thread failed: %s\n",
				ERROR, strerror(errno));
			goto out;
		}
	}
	if ((ret = create_rt_thread(&waker, wakerfn, (void *)lock,
				    SCHED_FIFO, 1))) {
		fprintf(stderr, "\t%s: Creating waker thread failed: %s\n",
			ERROR, strerror(errno));
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
		fprintf(stderr, "\t%s: pthread_barrier_destroy(wake_barrier) failed: %s\n",
			ERROR, strerror(errno));
	if ((ret = pthread_barrier_destroy(&waiter_barrier)))
		fprintf(stderr, "\t%s: pthread_barrier_destroy(waiter_barrier) failed: %s\n",
			ERROR, strerror(errno));
	return ret;
}

int main(int argc, char *argv[])
{
	int c, ret;

	while ((c = getopt(argc, argv, "bchlot:")) != -1) {
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
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	printf("%s: Test requeue functionality\n", basename(argv[0]));
	printf("\tArguments: broadcast=%d locked=%d owner=%d timeout=%ldns\n",
	       broadcast, locked, owner, timeout_ns);

	if ((ret = create_pi_mutex(&mutex)) != 0) {
		printf("Creating pi mutex failed\n");
		exit(1);
	}

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
