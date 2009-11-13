INCLUDES := -I../include
CFLAGS := $(CFLAGS) -g -O2 -Wall -D_GNU_SOURCE $(INCLUDES)
LDFLAGS := $(LDFLAGS) -lpthread -lrt

HEADERS := ../include/futextest.h
TARGETS :=

.PHONY: all clean
all: $(TARGETS)

$(TARGETS): $(HEADERS)

clean:
	rm -f $(TARGETS)
