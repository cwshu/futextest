// Copyright 2009 Google Inc.
// Author: walken@google.com (Michel Lespinasse)

#include "futextest.h"
#include "harness.h"

#include <stdio.h>


static inline void futex_wait_lock(futex_t *futex)
{
	int status = *futex;
	if (status == 0)
		status = futex_cmpxchg(futex, 0, 1);
	while (status != 0) {
		if (status == 1)
			status = futex_cmpxchg(futex, 1, 2);
		if (status != 0) {
			futex_wait(futex, 2, NULL, FUTEX_PRIVATE_FLAG);
			status = *futex;
		}
		if (status == 0)
			status = futex_cmpxchg(futex, 0, 2);
	}
}

static inline void futex_cmpxchg_unlock(futex_t *futex)
{
	int status = *futex;
	if (status == 1)
		status = futex_cmpxchg(futex, 1, 0);
	if (status == 2) {
		futex_cmpxchg(futex, 2, 0);
		futex_wake(futex, 1, FUTEX_PRIVATE_FLAG);
	}
}

static void * futex_wait_test(void * dummy)
{
	struct locktest_shared * shared = dummy;
	int i = shared->loops;
	barrier_sync(&shared->barrier_before);
	while (i--) {
		futex_wait_lock(&shared->futex);
		futex_cmpxchg_unlock(&shared->futex);
	}
	barrier_sync(&shared->barrier_after);
	return NULL;
}

int main (void)
{
	printf("FUTEX_WAIT test\n");
	locktest(futex_wait_test, 100000000);
	return 0;
}
