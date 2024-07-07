# ------------------------------------------------------

PLUGINS = midi.jam opl_isa.jam

# ------------------------------------------------------
.PHONY: all clean common $(PLUGINS)

all: $(PLUGINS)

$(PLUGINS) : %.jam :
	make -C $(@:.jam=)

clean:
	rm -f ../ext/jam/*.o
	@for dir in $(PLUGINS:.jam=); do \
	make -C $$dir clean; \
	done
