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

#include "hpgl.h"
#include "usb.h"
#include <stdlib.h>
#include <string.h>
#include <stm32f0xx.h>
#include "uart.h"

static const char *header = "\
<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\
<svg width=\"700\" height=\"578\" viewBox=\"-10 -10 700 578\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n";

static const char *graticule = "\
<defs>\n\
<pattern id=\"grid\" width=\"50\" height=\"60\" y=\"64\" patternUnits=\"userSpaceOnUse\">\n\
<path d=\"M 50 0 L 0 0 0 60\" fill=\"none\" stroke=\"black\" stroke-width=\"0.2\"/>\n\
</pattern>\n\
<pattern id=\"xaxis\" width=\"10\" height=\"8\" patternUnits=\"userSpaceOnUse\">\n\
<path d=\"M 0 0 L 0 8\" fill=\"none\" stroke=\"black\" stroke-width=\"0.2\"/>\n\
</pattern>\n\
<pattern id=\"yaxis\" width=\"8\" height=\"12\" y=\"64\" patternUnits=\"userSpaceOnUse\">\n\
<path d=\"M 0 0 L 8 0\" fill=\"none\" stroke=\"black\" stroke-width=\"0.2\"/>\n\
</pattern>\n\
</defs>\n\
<rect width=\"500\" height=\"480\" y=\"64\" fill=\"none\" stroke=\"black\" stroke-width=\"0.2\" />\n\
<rect width=\"500\" height=\"480\" y=\"64\" fill=\"url(#grid)\" />\n\
<rect y=\"300\" width=\"500\" height=\"8\" fill=\"url(#xaxis)\" />\n\
<rect x=\"246\" y=\"64\" width=\"8\" height=\"480\" fill=\"url(#yaxis)\" />\n";

static char buf [128], *buf_ptr = buf;

static int pen = 0;
static int pen_down = 0;
static int line_type = 0;
static int x = 0;
static int y = 256;
static int xy_state = 0;

static void sp()
{
	if (buf_ptr - buf <= 1 || (buf_ptr[-1] != ',' && buf_ptr[-1] != ';')) {
		return;
	}

	int new_pen = *buf - '0';

	if (pen == 0 && new_pen != 0) {
		usb_log_str(header);
		usb_log_str(graticule);
	} else if (pen != 0 && new_pen == 0) {
		usb_log_str("</svg>\n");
	}

	pen = new_pen;

	buf_ptr = buf;
}

static void color()
{
	switch (pen) {
	case 2:
		usb_log_str("green");
		break;
	case 3:
		usb_log_str("blue");
		break;
	default:
		usb_log_str("black");
	}
}


static void line_start()
{
	usb_log_str("<polyline stroke=\"");

	color();

	usb_log_str("\" ");

	switch (line_type) {
	case 2:
		usb_log_str("stroke-dasharray=\"10 10\" ");
		break;
	}

	usb_log_str("points=\"");
}

static void line_end()
{
	usb_log_str("\" stroke-width=\"1\" fill=\"none\" />\n");
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b) (MIN(MAX(x, a), b))
#define Y(y) CLAMP((560 - (y) * 2), 0, 560)

static void pr_start()
{
	if (!pen_down) {
		return;
	}

	line_start();

	buf_ptr = buf;
}

static void pr()
{
	if (buf_ptr - buf <= 1 || (buf_ptr[-1] != ',' && buf_ptr[-1] != ';')) {
		return;
	}

	buf_ptr[-1] = 0;
	buf_ptr = buf;

	int n = atoi(buf);

	xy_state ^= 1;

	if (xy_state == 1) {
		x += n;
		return;
	}

	y += n;
	usb_log_int(x);
	usb_log_str(",");
	usb_log_int(Y(y));
	usb_log_str(" ");
}

static void pr_end()
{
	if (!pen_down) {
		return;
	}

	line_end();
}

static void pa()
{
	if (!pen_down) {
		return;
	}

	if (buf_ptr - buf <= 1 || (buf_ptr[-1] != ',' && buf_ptr[-1] != ';')) {
		return;
	}

	buf_ptr[-1] = 0;
	buf_ptr = buf;

	int n = atoi(buf);

	xy_state ^= 1;

	if (xy_state == 1) {
		x = n;
		return;
	}

	y = n;

	usb_log_str("<circle cx=\"");
	usb_log_int(x);
	usb_log_str("\" cy=\"");
	usb_log_int(Y(y));
	usb_log_str("\" r=\"1\" fill=\"black\" />\n");
}

static void pd_start()
{
	pen_down = 1;

	buf_ptr = buf;
}

static void pd()
{
	if (!pen_down) {
		return;
	}

	if (buf_ptr - buf <= 1 || (buf_ptr[-1] != ',' && buf_ptr[-1] != ';')) {
		return;
	}

	buf_ptr[-1] = 0;
	buf_ptr = buf;

	int n = atoi(buf);

	xy_state ^= 1;

	static int old_x, old_y;

	if (xy_state) {
		old_x = x;
		x = n;
	} else {
		old_y = y;
		y = n;

		line_start();
		usb_log_int(old_x);
		usb_log_str(",");
		usb_log_int(Y(old_y));
		usb_log_str(" ");
		usb_log_int(x);
		usb_log_str(",");
		usb_log_int(Y(y));
		line_end();
	}
}

static void pu()
{
	if (buf_ptr - buf <= 1 || (buf_ptr[-1] != ',' && buf_ptr[-1] != ';')) {
		return;
	}

	buf_ptr[-1] = 0;
	buf_ptr = buf;

	int n = atoi(buf);

	xy_state ^= 1;

	if (xy_state) {
		x = n;
	} else {
		y = n;
	}

	pen_down = 0;
}

static void lb()
{
	if (buf_ptr[-1] == 0x03) {
		return;
	}

	char str [] = {buf_ptr[-1], 0};
	usb_log_str(str);

	buf_ptr = buf;
}

static void lb_start()
{
	usb_log_str("<text x=\"");
	usb_log_int(x + 3);
	usb_log_str("\" y=\"");
	usb_log_int(Y(y) + 5);
	usb_log_str("\" font-family=\"mono\" font-size=\"14\" fill=\"");
	color();
	usb_log_str("\">");
}

static void lb_end()
{
	usb_log_str("</text>\n");
}

static void lt()
{
	if (buf_ptr - buf <= 1 || buf_ptr[-1] != ';') {
		line_type = 0;
		return;
	}

	line_type = atoi(buf);
}

static struct cmd {
	const char *str;
	void (*func)();
	void (*start)();
	void (*end)();
} cmds [] = {
	{"SP", sp, 0, 0},
	{"PR", pr, pr_start, pr_end},
	{"PA", pa, 0, 0},
	{"PD", pd, pd_start, 0},
	{"PU", pu, 0, 0},
	{"LB", lb, lb_start, lb_end},
	{"LT", lt, 0, 0}
};

static struct cmd *cmd = 0;

static volatile int overflow = 0;

static volatile char uart_buf [128];
static volatile uint16_t uart_buf_len = 0;

void hpgl_received(char c)
{
	if (uart_buf_len >= sizeof(uart_buf)) {
		overflow = 1;
		return;
	}

	uart_buf[uart_buf_len++] = c;
}

void hpgl_loop()
{
	while (1) {
		char c;

		if (overflow) {
			uart_send_str("overflow");
			break;
		}

		NVIC_DisableIRQ(USART2_IRQn);
		if (uart_buf_len) {
			c = uart_buf[0];
			for (int i = 1; i < uart_buf_len; i++) {
				uart_buf[i - 1] = uart_buf[i];
			}
			uart_buf_len--;
			NVIC_EnableIRQ(USART2_IRQn);
		} else {
			NVIC_EnableIRQ(USART2_IRQn);
			__WFI();
			continue;
		}

		*buf_ptr++ = c;

		if (c == ';' || c == 0x03) {
			if (cmd) {
				cmd->func();

				if (cmd->end) {
					cmd->end();
				}
			}

			xy_state = 0;
			cmd = 0;
			buf_ptr = buf;
			continue;
		}

		if (buf_ptr == buf + sizeof(buf)) {
			uart_send_str("buf overflow\n");
			return;
		}

		if (cmd) {
			cmd->func();
		}

		for (size_t i = 0; i < sizeof(cmds) / sizeof(*cmds); i++) {
			if ((unsigned) (buf_ptr - buf) == strlen(cmds[i].str) &&
					strncmp(buf, cmds[i].str, strlen(cmds[i].str)) == 0) {
				buf_ptr = buf;
				cmd = &cmds[i];

				if (cmd->start) {
					cmd->start();
				}

				break;
			}
		}
	}

	while (1) __NOP();
}
