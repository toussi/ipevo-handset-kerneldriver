#!/usr/bin/make -f


CFLAGS  += -DCONFIG_IPEVO=1
KDIR    ?= /lib/modules/`uname -r`/build
MODPATH ?= /lib/modules/`uname -r`/kernel/drivers/usb/input

modules:
	make -C $(KDIR) SUBDIRS=`pwd`/module CONFIG_IPEVO=m \
		$(DBG_CONFIG) CC="${CROSS_COMPILE}gcc" modules

install: modules
	install -d $(MODPATH)
	install -m 644 -c `pwd`/module/ipevo.ko $(MODPATH)
	/sbin/depmod -a

clean:
	find . \( -name '*.ko' -o -name '*.o' -o -name '.tmp_versions' -o -name '*~' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '*.tar.bz2' -o -name '*.rej' -o -name '*.orig' \)\
		-print | xargs rm -Rf

