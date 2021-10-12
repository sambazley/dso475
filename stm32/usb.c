/* Copyright (C) 2021 Sam Bazley
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "usb.h"
#include "uart.h"
#include "common.h"
#include <usblib.h>
#include <stm32f0xx.h>
#include <string.h>

static struct usb_endpoint endpoints [] = {
	{64, 64, USB_EP_CONTROL, DIR_BIDIR},
	{8, 0, USB_EP_BULK, DIR_OUT},
	{0, 8, USB_EP_BULK, DIR_IN},
};

static const uint8_t device_descriptor [] = {
	18,         // bLength (constant)
	1,          // bDescriptorType (constant)
	0x10, 0x01, // bcdUSB - USB 1.1
	0,          // bDeviceClass
	0,          // bDeviceSubClass
	0,          // bDeviceProtocol
	64,         // bMaxPacketSize
	0x76, 0x98, // idVendor
	0x67, 0x45, // idProduct
	0x01, 0x01, // bcdDevice - device version number
	1,          // iManufacturer
	2,          // iProduct
	3,          // iSerialNumber
	1,          // bNumConfigurations
};

#define MSB(x) ((x) >> 8)
#define LSB(x) ((x) & 0xFF)

#define MIN(a, b) ((a < b) ? (a) : (b))

#define CFG_SIZE (9 + 9 + 7 + 7)

static const uint8_t config1_descriptor [] = {
	9,                            // bLength
	2,                            // bDescriptorType
	LSB(CFG_SIZE), MSB(CFG_SIZE), // wTotalLength
	1,                            // bNumInterfaces
	1,                            // bConfigurationValue
	0,                            // iConfiguration
	0x80,                         // bmAttributes
	25,                           // bMaxPower

	// interface 0
	9,                            // bLength
	4,                            // bDescriptorType
	0,                            // bInterfaceNumber
	0,                            // bAlternateSetting
	2,                            // bNumendpoints
	0xff,                         // bInterfaceClass
	0x00,                         // bInterfaceSubClass
	0,                            // bInterfaceProtocol
	0,                            // iInterface

	// OUT endpoint
	7,                            // bLength
	0x05,                         // bDescriptorType
	0x01,                         // bendpointAddress
	0x02,                         // bmAttributes (bulk)
	8, 0,                         // wMaxPacketSize (8)
	0,                            // bInterval

	// IN endpoint
	7,                            // bLength
	0x05,                         // bDescriptorType
	0x82,                         // bendpointAddress
	0x02,                         // bmAttributes (bulk)
	8, 0,                         // wMaxPacketSize (8)
	10,                           // bInterval
};

static const uint8_t lang_str [] = {
	6, 3,
	0x09, 0x04, 0x00, 0x00
};

static const uint8_t vendor_str [] = {
	22, 3,
	'S', 0,
	'a', 0,
	'm', 0,
	' ', 0,
	'B', 0,
	'a', 0,
	'z', 0,
	'l', 0,
	'e', 0,
	'y', 0
};

static const uint8_t product_str [] = {
	14, 3,
	'D', 0,
	'S', 0,
	'O', 0,
	'4', 0,
	'7', 0,
	'5', 0
};

static const uint8_t serial_no_str [] = {
	4, 3, '1', 0
};

enum {
	DESC_DEVICE = 1,
	DESC_CONFIG,
	DESC_STRING
};

#define DESCRIPTOR(type, index, wIndex, addr) \
{(type << 8) | index, wIndex, addr, sizeof(addr)}

static struct usb_descriptor descriptors [] = {
	DESCRIPTOR(DESC_DEVICE, 0, 0x0000, device_descriptor),
	DESCRIPTOR(DESC_CONFIG, 0, 0x0000, config1_descriptor),
	DESCRIPTOR(DESC_STRING, 0, 0x0000, lang_str),
	DESCRIPTOR(DESC_STRING, 1, 0x0409, vendor_str),
	DESCRIPTOR(DESC_STRING, 2, 0x0409, product_str),
	DESCRIPTOR(DESC_STRING, 3, 0x0409, serial_no_str),
};

static void on_control_out_interface0(struct usb_interface *iface,
		struct usb_setup_packet *sp)
{
	(void) iface;

	switch (sp->bRequest) {
	default:
		uart_send_str("== UNHANDLED INTERFACE 0 REQUEST ");
		uart_send_int(sp->bRequest);
		uart_send_str(" ==\n");
		usb_ack(0);
	}
}

enum {
	REQ_SET_INTERFACE = 11
};

static void on_control_out_interface1(struct usb_interface *iface,
		struct usb_setup_packet *sp)
{
	switch (sp->bRequest) {
	case REQ_SET_INTERFACE:
		uart_send_str("SET_IFACE ");
		uart_send_int(sp->wValue);
		uart_send_str("\n");
		iface->alternate = sp->wValue;
		volatile uint16_t *epr = USB_EP(1);

		if (iface->alternate) {
			if (*epr & USB_EP_DTOG_TX) {
				*epr = (*epr & USB_EPREG_MASK) | USB_EP_DTOG_TX;
			}

			if (*epr & USB_EP_DTOG_RX) {
				*epr = (*epr & USB_EPREG_MASK) | USB_EP_DTOG_RX;
			}

			usb_ep_set_rx_status(1, USB_EP_RX_VALID);
			usb_ep_set_tx_status(1, USB_EP_TX_VALID);
		} else {
			usb_ep_set_rx_status(1, USB_EP_RX_DIS);
			usb_ep_set_tx_status(1, USB_EP_TX_DIS);
		}

		usb_ack(0);

		break;
	default:
		uart_send_str("== UNHANDLED INTERFACE 1 REQUEST ");
		uart_send_int(sp->bRequest);
		uart_send_str(" ==\n");
		usb_ack(0);
	}
}

static struct usb_interface interfaces [] = {
	{0, 0, on_control_out_interface0},
	{1, 0, on_control_out_interface1},
};

size_t strnlen(const char *s, size_t maxlen)
{
	for (size_t i = 0; i < maxlen; i++) {
		if (s[i] == 0) {
			return i;
		}
	}

	return maxlen;
}

static volatile struct usb_packet_log log_packet;
static volatile int send_complete = 1;

static char buffer [62] = {0};
static char *buf_ptr = buffer;

static int _usb_log_str(const char *str)
{
	int buf_len = buf_ptr - buffer;
	int len = strnlen((char *) str, sizeof(buffer) - buf_len);

	strncpy(buf_ptr, str, len);

	buf_ptr += len;
	buf_len = buf_ptr - buffer;

	__disable_irq();

	if (!send_complete) {
		return len;
	}

	send_complete = 0;

	log_packet.type = USB_PACKET_LOG;
	log_packet.length = 2 + buf_len;
	strncpy((char *) log_packet.payload, (char *) buffer, buf_len);
	usb_send_data(2, (uint8_t *) &log_packet, log_packet.length, 0);

	memset(buffer, 0, sizeof(buffer));
	buf_ptr = buffer;

	__enable_irq();

	return len;
}

void usb_log_str(const char *str)
{
	GPIOA->ODR &= ~1;

	while (*str) {
		str += _usb_log_str(str);
	}
}

void usb_log_int(uint32_t n)
{
	static char str [sizeof(log_packet.payload)];
	char *ptr = str;
	int sending = 0;

	memset(str, 0, sizeof(str));

	for (int i = 32; i >= 0; i--) {
		uint32_t x = n;

		for (int j = 0; j < i; j++) {
			x /= 10;
		}

		x %= 10;

		if (x) {
			sending = 1;
		}

		if (sending) {
			*ptr++ = '0' + x;
		}
	}

	if (!sending) {
		*ptr++ = '0';
	}

	*ptr++ = 0;

	usb_log_str(str);
}

static void on_correct_transfer(uint8_t ep, uint8_t *data, uint8_t len)
{
	(void) data;
	(void) len;

	if (ep == 0x02) {
		GPIOA->ODR |= 1;
		send_complete = 1;
	}
}

static struct usb_configuration conf = {
	.endpoints = endpoints,
	.endpoint_count = sizeof(endpoints) / sizeof(struct usb_endpoint),
	.interfaces = interfaces,
	.interface_count = sizeof(interfaces) / sizeof(struct usb_interface),
	.descriptors = descriptors,
	.descriptor_count = sizeof(descriptors) / sizeof(struct usb_descriptor),
	.on_correct_transfer = on_correct_transfer,
	.log_str = uart_send_str,
	.log_int = uart_send_int,
};

void usb_impl_init()
{
	usb_init(&conf);
}
