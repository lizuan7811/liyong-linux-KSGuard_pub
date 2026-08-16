KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 原始碼樹（含 Kbuild 用的 Makefile）
MDIR := $(PWD)/kernel

# 編譯產物最終要放的地方
BUILD_DIR := $(PWD)/build

all:
	$(MAKE) -C $(KDIR) M=$(MDIR) modules
	mkdir -p $(BUILD_DIR)
	@cd $(MDIR) && for f in $$(find . -type f \( \
		-name '*.o'        -o \
		-name '*.ko'       -o \
		-name '*.mod'      -o \
		-name '*.mod.c'    -o \
		-name '*.cmd'      -o \
		-name 'Module.symvers'          -o \
		-name 'modules.order'           -o \
		-name 'modules.builtin'         -o \
		-name 'modules.builtin.modinfo' \
	\) ); do \
		mkdir -p $(BUILD_DIR)/$$(dirname $$f); \
		mv $$f $(BUILD_DIR)/$$f; \
	done

clean:
	$(MAKE) -C $(KDIR) M=$(MDIR) clean
	rm -rf $(BUILD_DIR)

install:
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR)/../kernel modules_install
	depmod -a

.PHONY: all clean install