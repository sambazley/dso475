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

#include "common.h"
#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static libusb_device_handle *dso = 0;
static int libusb_inited = 0;
static int iface_claimed = 0;

static int receive_data(libusb_device_handle *dev, uint8_t **dest, size_t *len)
{
	int r;

	*dest = 0;
	*len = 0;

	while (1) {
		unsigned char buf [8];
		int rlen;
		r = libusb_bulk_transfer(dev, 0x82, buf, sizeof(buf), &rlen, 0);
		if (r < 0) {
			fprintf(stderr, "Failed to receive data %d\n", r);
			goto end;
		}

		if (rlen == 0) {
			continue;
		}

		*len += rlen;
		*dest = realloc(*dest, *len);
		strncpy((char *) *dest + *len - rlen, (char *) buf, rlen);

		uint8_t packet_length = ((uint8_t *) *dest)[0];

		if (packet_length == 0) {
			continue;
		} else if (*len > packet_length) {
			r = -E2BIG;
			break;
		} else if (*len >= packet_length) {
			r = 0;
			break;
		}
	}

end:
	return r;
}

static void close_dev()
{
	if (iface_claimed) {
		libusb_release_interface(dso, 0);
		iface_claimed = 0;
	}

	if (dso) {
		libusb_close(dso);
	}

	if (libusb_inited) {
		libusb_exit(NULL);
		libusb_inited = 0;
	}
}

static int open_dev()
{
	int r;
	ssize_t dev_cnt;
	libusb_device **devs;

	if (libusb_inited) {
		fprintf(stderr, "libusb already initialised\n");
		return 1;
	}

	r = libusb_init(NULL);

	if (r < 0) {
		fprintf(stderr, "Failed to initialize libusb\n");
		return r;
	}
	libusb_inited = 1;

	r = dev_cnt = libusb_get_device_list(NULL, &devs);
	if (r < 0) {
		fprintf(stderr, "Failed to get device list\n");
		return r;
	}

	for (int i = 0; i < dev_cnt; i++) {
		struct libusb_device_descriptor desc;
		libusb_device *dev = devs[i];

		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "Cannot get device descriptor\n");
			break;
		}

		if (desc.idVendor == 0x9876 && desc.idProduct == 0x4567) {
			r = libusb_open(dev, &dso);
			if (r < 0) {
				fprintf(stderr, "Failed to open device\n");
			}
			break;
		}
	}

	libusb_free_device_list(devs, 1);

	if (r < 0) {
		return r;
	}

	if (!dso) {
		fprintf(stderr, "No device found\n");
		r = 1;
		goto exit;
	}

	r = libusb_claim_interface(dso, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to claim interface\n");
		goto exit;
	}
	iface_claimed = 1;

	return 0;
exit:
	close_dev();
	return r;
}

static int read_dso()
{
	int r = 0;

	if ((r = open_dev())) {
		return r;
	}

	while (1) {
		uint8_t *rbuf;
		size_t len;
		r = receive_data(dso, &rbuf, &len);

		if (r < 0) {
			if (rbuf) {
				free(rbuf);
			}
			break;
		}

		for (size_t i = 2; i < len; i++) {
			printf("%c", rbuf[i]);
		}

		fflush(stdout);

		if (len >= 7 && strncmp((char *) rbuf + len - 7, "</svg>\n", 7) == 0) {
			free(rbuf);
			break;
		}

		free(rbuf);
	}

	close_dev();

	return r;
}

int main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	return read_dso();
}
