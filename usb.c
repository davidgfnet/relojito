/*
 * Copyright (C) 2022 David Guillen Fandos <david@davidgf.net>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencmsis/core_cm3.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/scb.h>

#include "commandset.h"

/* We need a special large control buffer for this device: */
uint8_t usbd_control_buffer[64];

usbd_device *usbd_dev;

extern unsigned main_color;
extern unsigned work_mode;

const struct usb_device_descriptor dev_descr = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0xdead,
	.idProduct = 0xc0fe,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

const struct usb_interface_descriptor custom_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = 0xFF, /* Custom driver */
	.bInterfaceSubClass = 0xFF,
	.bInterfaceProtocol = 0xFF,
	.iInterface = 0,
};

const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &custom_iface,
}};

const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0xC0,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static char serial_no[25] = {0};

static const char *usb_strings[] = {
	"davidgf.net (libopencm3)",
	"relojito control device",
	serial_no,
};

static void get_dev_unique_id(char *s) {
	volatile uint8_t *unique_id = (volatile uint8_t *)0x1FFFF7E8;
	int i;

	/* Fetch serial number from chip's unique ID */
	for(i = 0; i < 24; i+=2) {
		s[i] = ((*unique_id >> 4) & 0xF) + '0';
		s[i+1] = (*unique_id++ & 0xF) + '0';
	}
	for(i = 0; i < 24; i++)
		if(s[i] > '9')
			s[i] += 'A' - '9' - 1;
}

static uint8_t rdbuf[16];

static enum usbd_request_return_codes
	custom_control_request_in(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *dev, struct usb_setup_data *req)) {

	// Sending data out
	switch (req->bRequest) {
	case CMD_MODE:
		rdbuf[0] = work_mode;
		*buf = rdbuf;
		*len = 1;
		return USBD_REQ_HANDLED;
	case CMD_CLOCK: {
		// Return time
		uint32_t secs = rtc_get_counter_val();
		memcpy(rdbuf, &secs, sizeof(uint32_t));

		*buf = rdbuf;
		*len = sizeof(uint32_t);
		} return USBD_REQ_HANDLED;
	case CMD_READFLASH:
		*buf = (void*)(0x08000000 + 63*1024);
		*len = 1024;
		return USBD_REQ_HANDLED;
	}

	return USBD_REQ_NOTSUPP;
}

// Reboots the system into the bootloader, making sure
// it enters in DFU mode.
static void reboot_into_bootloader() {
	uint32_t * ptr = (uint32_t*)0x20000000 - 8;
	ptr[0] = 0xDEADBEEF;
	ptr[1] = 0xCC00FFEE;
}

static void reboot_dfu_complete(usbd_device *dev, struct usb_setup_data *req) {
	(void)req;
	(void)dev;

	// Reboot into DFU!
	reboot_into_bootloader();
	scb_reset_system();
}

volatile uint8_t usb_work_mode = 0xff;

static enum usbd_request_return_codes
	custom_control_request_out(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *dev, struct usb_setup_data *req)) {

	// Receive data from Host
	switch (req->bRequest) {
	case CMD_MODE:
		work_mode = (*buf)[0];
		break;
	case CMD_CLOCK: {
		// Update time!
			uint32_t secs = *(uint32_t*)(*buf);
			rtc_set_counter_val(secs);
		} break;
	case CMD_SET_COLOR:
		main_color = (*buf)[0] | ((*buf)[1] << 8) | ((*buf)[2] << 16);
		break;
	case CMD_RESET_RTC:
		rcc_backupdomain_reset();
		break;
	case CMD_GO_DFU:
		*complete = reboot_dfu_complete;
		break;
	};

	*len = 0;
	return USBD_REQ_HANDLED;
}

static void custom_set_config(usbd_device *dev, uint16_t wValue) {
	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE | USB_REQ_TYPE_IN,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT | USB_REQ_TYPE_DIRECTION,
				custom_control_request_in);

	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE | USB_REQ_TYPE_OUT,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT | USB_REQ_TYPE_DIRECTION,
				custom_control_request_out);
}

void reenumerate_usb() {
	// Force reenumeration
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	for (unsigned int i = 0; i < 800000; i++)
		asm volatile("nop");
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO12);
}

void start_usb() {
	get_dev_unique_id(serial_no);

	// Force USB connection
	reenumerate_usb();

	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config,
			     usb_strings, 3,
			     usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, custom_set_config);
}

void usbpoll() {
	usbd_poll(usbd_dev);
}

