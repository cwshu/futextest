SUBDIRS = functional performance stress

.PHONY: all clean
all:
	for DIR in $(SUBDIRS); do (cd $$DIR; ${MAKE} all); done

clean:
	for DIR in $(SUBDIRS); do (cd $$DIR; ${MAKE} clean); done
