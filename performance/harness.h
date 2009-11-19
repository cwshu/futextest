// Copyright 2009 Google Inc.
// Author: walken@google.com (Michel Lespinasse)

#include <limits.h>
#include <sys/times.h>
#include <stdio.h>
#include <pthread.h>


struct thread_barrier {
	futex_t threads;
	futex_t unblock;
};

struct locktest_shared {
	struct thread_barrier barrier_before;
	struct thread_barrier barrier_after;
	int loops;
	futex_t futex;
};

/* Called by main thread to initialize barrier */
static void barrier_init(struct thread_barrier *barrier, int threads)
{
	barrier->threads = threads;
	barrier->unblock = 0;
}

/* Called by worker threads to synchronize with main thread */
static void barrier_sync(struct thread_barrier *barrier)
{
	futex_dec(&barrier->threads);
	if (barrier->threads == 0)
		futex_wake(&barrier->threads, 1, FUTEX_PRIVATE_FLAG);
	while (barrier->unblock == 0)
		futex_wait(&barrier->unblock, 0, NULL, FUTEX_PRIVATE_FLAG);
}

/* Called by main thread to wait for all workers to reach sync point */
static void barrier_wait(struct thread_barrier *barrier)
{
	int threads;
	while ((threads = barrier->threads) > 0)
		futex_wait(&barrier->threads, threads, NULL,
			   FUTEX_PRIVATE_FLAG);
}

/* Called by main thread to unblock worker threads from their sync point */
static void barrier_unblock(struct thread_barrier *barrier)
{
	barrier->unblock = 1;
	futex_wake(&barrier->unblock, INT_MAX, FUTEX_PRIVATE_FLAG);
}


static void locktest(void * thread_function(void *), int iterations)
{
	int threads[] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 24, 32,
			  64, 128, 256, 512, 1024, 0 };
	int t;
	for (t = 0; threads[t]; t++) {
		int num_threads = threads[t];
		struct locktest_shared shared;
		pthread_t thread[num_threads];
		int i;
		clock_t before, after;
		struct tms tms_before, tms_after;
		int wall, user, system;
		double tick;

		barrier_init(&shared.barrier_before, num_threads);
		barrier_init(&shared.barrier_after, num_threads);
		shared.loops = iterations / num_threads;
		shared.futex = 0;

		for (i = 0; i < num_threads; i++)
			pthread_create(thread + i, NULL, thread_function,
				       &shared);
		barrier_wait(&shared.barrier_before);
		before = times(&tms_before);
		barrier_unblock(&shared.barrier_before);
		barrier_wait(&shared.barrier_after);
		after = times(&tms_after);
		wall = after - before;
		user = tms_after.tms_utime - tms_before.tms_utime;
		system = tms_after.tms_stime - tms_before.tms_stime;
		tick = 1.0 / sysconf(_SC_CLK_TCK);
		printf("%d threads: %.0f Kiter/s "
		       "(%.2fs user %.2fs system %.2fs wall %.2f cores)\n",
		       num_threads,
		       (num_threads * shared.loops) / (wall * tick * 1000),
		       user * tick, system * tick, wall * tick,
		       wall ? (user + system) * 1. / wall : 1.);
		barrier_unblock(&shared.barrier_after);
		for (i = 0; i < num_threads; i++)
			pthread_join(thread[i], NULL);
	}
}
