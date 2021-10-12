OBJS=vec.o boot.o main.o uart.o uart.o usb.o hpgl.o
DEPS=$(OBJS:.o=.d)

USB_DIR=libstm32usb

CC = arm-none-eabi-gcc
LD = arm-none-eabi-ld
AS = arm-none-eabi-as
OBJCOPY = arm-none-eabi-objcopy

CFLAGS += -std=c99 -mcpu=cortex-m0 -mthumb -Wall -Wextra -Wpedantic
LDFLAGS = -Tstm32.ld -mcpu=cortex-m0 -mthumb -Wl,--gc-sections -lc -lnosys

CFLAGS += -DSTM32F042x6

CFLAGS += -I/opt/STM32/STM32F0
CFLAGS += -I/usr/arm-none-eabi/include/

CFLAGS += -I..

CFLAGS += -I$(USB_DIR)

ifeq ($(DEBUG),1)
CFLAGS += -Og -ggdb -DDEBUG
else
CFLAGS += -O2
endif

all: $(DEPS)

-include $(DEPS)

all: stm32

stm32.bin: stm32
	$(OBJCOPY) $^ $@ -O binary

stm32: $(OBJS) $(USB_DIR)/libusb.a
	$(CC) $^ -o $@ $(LDFLAGS)

$(USB_DIR)/libusb.a:
	make -C $(USB_DIR) TARGET=STM32F042x6 DEBUG=1

flash: stm32.bin
ifeq ($(DEBUG),1)
	openocd -f flash.cfg
else
	st-flash --reset write $^ 0x08000000
endif

%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

%.d: %.s
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

clean:
	rm -f $(OBJS) $(DEPS) stm32 stm32.bin
	make -C libstm32usb clean
