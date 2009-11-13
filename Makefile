SUBDIRS := functional performance stress

.PHONY: all clean
all:
	for DIR in $(SUBDIRS); do $(MAKE) -C $$DIR $@ ; done

clean:
	for DIR in $(SUBDIRS); do $(MAKE) -C $$DIR $@ ; done
