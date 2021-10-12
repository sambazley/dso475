#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

enum {
	USB_PACKET_INITIALIZE,
	USB_PACKET_LOG,
};

struct usb_packet_any {
	uint8_t length;
	uint8_t type;
};

struct usb_packet_initialize {
	uint8_t length;
	uint8_t type;
};

struct usb_packet_log {
	uint8_t length;
	uint8_t type;
	char payload [62];
};

union usb_packet_out {
	struct usb_packet_any any;
	struct usb_packet_initialize initialize;
};

#endif /* COMMON_H */
