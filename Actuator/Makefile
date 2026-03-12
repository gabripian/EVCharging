CONTIKI_PROJECT = Actuator
all: $(CONTIKI_PROJECT)
# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

# Include CoAP resources
MODULES_REL += ../Util						#position of Util folder respect to the Makefile

MODULES_REL+=./resources

CONTIKI= ../../..

# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

ifeq ($(TARGET), cooja)
CFLAGS += -DCOOJA
endif
CFLAGS+=-Wno-error=unused-variable
CFLAGS+=-Wno-error=unused-function
CFLAGS+= -Wl,-u,_printf_float

CFLAGS += -DCOAP_OBSERVE_CLIENT=1
include $(CONTIKI)/Makefile.include