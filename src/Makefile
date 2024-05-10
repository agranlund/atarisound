CLEAN_FILES =	ext/stripx/stripx \
				ext/lgcc.o


CLEAN_ODIRS = 	ext \
				ext/jam \
				ext/stripx

BUILD_MDIRS	=	ext/libcmini \
				jamplugins


.PHONY: all clean

all:
	for dir in $(BUILD_MDIRS); do \
	make -C $$dir; \
	done

clean:
	@for file in $(CLEAN_FILES); do \
	rm -f $$file; \
	done
	@for dir in $(BUILD_MDIRS); do \
	make -C $$dir clean; \
	done
	@for dir in $(CLEAN_ODIRS); do \
	rm -f $$dir/*.o; \
	done

