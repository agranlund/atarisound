
# ------------------------------------------------------

PLUGINS = midi.jam

# ------------------------------------------------------

.PHONY: all clean distclean $(PLUGINS)

all: $(PLUGINS)

$(PLUGINS) : %.jam :
	make -C $(@:.jam=)

clean:
	@for dir in $(PLUGINS:.jam=); do \
	make -C $$dir clean; \
	done

distclean: clean common_clean

include $(ROOTDIR)Makefile.common

