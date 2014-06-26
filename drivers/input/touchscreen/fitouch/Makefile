export CONFIG_FITOUCH = m

MODULE_NAME := fitouch 
	
obj-$(CONFIG_FITOUCH) += fitouch.o 

all: modules

modules: 
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd)  modules 

strip:
	$(CROSS_COMPILE)strip $(MODULE_NAME).ko --strip-unneeded

.PHONY: modules clean

clean: 
	rm -fr *.mod.c *.mod *.o .*.cmd *.ko *~ modules.order .tmp_versions Module.symvers

