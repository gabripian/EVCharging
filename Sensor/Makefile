CONTIKI_PROJECT = Sensor
all: $(CONTIKI_PROJECT)
# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

# Include CoAP resources
MODULES_REL += ../Util			#position of Util folder respect to the Makefile

MODULES_REL+=./resources
MODULES_REL+=/home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn

TARGET_LIBFILES += -lm

INC+= /home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn

CONTIKI= ../../..


ifeq ($(TARGET), cooja)
CFLAGS += -DCOOJA
endif
CFLAGS+=-Wno-error=unused-variable
CFLAGS+=-Wno-error=unused-function
CFLAGS+=-Wno-error=unused-but-set-variable
CFLAGS+= -Wl,-u,_printf_float
# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include