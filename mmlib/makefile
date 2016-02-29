.EXPORT_ALL_VARIABLES:
MMLIB_HOME = $(shell pwd)

MMLIB_OBJ_DIR = $(MMLIB_HOME)/.obj

include $(MMLIB_HOME)/makefile.rule

MODULES_LIST = $(shell find . -type f -name makefile.mmlib|grep -v "\./makefile.mmlib"|sed "s/\/makefile.mmlib//g")
OBJS = $(shell find ${MMLIB_OBJ_DIR} -type f -name "*.o")

.PHONY: .SILENT MODULES

TARGETS = mmlib.a

all:mmlib.a

mmlib.a:MODULES
	$(AR) $@ $(OBJS)

MODULES:
	@mkdir -p $(MMLIB_OBJ_DIR); \
	for MODULE in $(MODULES_LIST); do \
		echo "Build $$MODULE"; \
		make -C $$MODULE -f makefile.mmlib all && echo "Build $$MODULE Done" && echo "" && echo ""; \
		if [ $$? -ne 0 ];then \
			echo "Build $$MODULE Failed"; \
			exit -1; \
		fi \
	done

clean:
	@for MODULE in $(MODULES_LIST); do \
		echo "Clean $$MODULE"; \
		make -C $$MODULE -f makefile.mmlib clean && echo "Clean $$MODULE Done" && echo "" && echo ""; \
		if [ $$? -ne 0 ];then \
			echo "Clean $$MODULE Failed"; \
			exit -1; \
		fi \
	done
	$(RM) $(MMLIB_OBJ_DIR)/*
	$(RM) $(TARGETS)

