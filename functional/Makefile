INCLUDES := -I../include
CFLAGS := $(CFLAGS) -g -O2 -Wall -D_GNU_SOURCE $(INCLUDES)
LDFLAGS := $(LDFLAGS) -lpthread -lrt

HEADERS := ../include/futextest.h
TARGETS := \
	futex_wait_timeout \
	futex_wait_wouldblock \
	futex_requeue_pi \
	futex_requeue_pi_signal_restart \
	futex_requeue_pi_mismatched_ops

.PHONY: all clean
all: $(TARGETS)

$(TARGETS): $(HEADERS)

clean:
	rm -f $(TARGETS)
