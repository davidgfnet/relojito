/*
 * relojito-cli.cc
 *
 * Author: David Guillen Fandos (2023) <david@davidgf.net>
 *
 * CLI tool to comunicate with relojito over USB
 *
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <libusb-1.0/libusb.h>

#include "commandset.h"

#define VENDOR_ID         0xdead
#define PRODUCT_ID        0xc0fe
#define IFACE_NUMBER         0x0
#define TIMEOUT_MS          5000

static const int CTRL_REQ_TYPE_IN  = LIBUSB_ENDPOINT_IN  | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE;
static const int CTRL_REQ_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE;

void fatal_error(const char * errmsg) {
	fprintf(stderr, "ERROR! %s\n", errmsg);
	exit(1);
}

int main(int argc, char ** argv) {
	// Parse input
	// Usage is like ./xion set port-num value
	if (argc < 2) {
		fprintf(stderr, "Usage: %s command [args]\n\n", argv[0]);
		fprintf(stderr, "  Command list:\n");
		fprintf(stderr, "   * rebootdfu\n");
		fprintf(stderr, "   * readflash\n");
		fprintf(stderr, "   * readtime\n");
		fprintf(stderr, "   * reset-backup\n");
		fprintf(stderr, "   * writetime hour min sec\n");
		exit(1);
	}

	std::string cmd = argv[1];

	int result = libusb_init(NULL);
	if (result < 0)
		fatal_error("libusb_init failed!");

	struct libusb_device_handle *devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);

	if (!devh)
		fatal_error("libusb_open_device_with_vid_pid failed to find a matching device!");

	result = libusb_claim_interface(devh, IFACE_NUMBER);
	if (result < 0)
		fatal_error("libusb_claim_interface failed!");

	if (cmd == "rebootdfu") {
		// Do not check result since the reboot happens inmediately and it's very likely to fail
		uint8_t dummy;
		if (libusb_control_transfer(devh, CTRL_REQ_TYPE_OUT, CMD_GO_DFU, 0, IFACE_NUMBER, &dummy, 1, TIMEOUT_MS) < 0)
			fatal_error("Reboot into DFU command failed!");
	}
	else if (cmd == "readtime") {
		uint32_t tdat;
		libusb_control_transfer(devh, CTRL_REQ_TYPE_IN, CMD_CLOCK, 0, IFACE_NUMBER, (uint8_t*)&tdat, sizeof(uint32_t), TIMEOUT_MS);
		printf("Time is %d (%02d:%02d:%02d)\n", tdat, (tdat / 3600) % 24, (tdat / 60) % 60, tdat % 60);
	}
	else if (cmd == "readflash") {
		uint8_t data[1024];
		if (libusb_control_transfer(devh, CTRL_REQ_TYPE_IN, CMD_READFLASH, 0, IFACE_NUMBER, data, sizeof(data), TIMEOUT_MS) < 0)
			fatal_error("Could not read data!");
		fwrite(data, 1, sizeof(data), stdout);
	}
	else if (cmd == "reset-backup") {
		if (libusb_control_transfer(devh, CTRL_REQ_TYPE_OUT, CMD_RESET_RTC, 0, IFACE_NUMBER, 0, 0, TIMEOUT_MS) < 0)
			fatal_error("Backup domain reset failed!");
	}
	else if (cmd == "writetime") {
		unsigned char tdat[3];
		tdat[0] = atoi(argv[2]);
		tdat[1] = atoi(argv[3]);
		tdat[2] = atoi(argv[4]);

		libusb_control_transfer(devh, CTRL_REQ_TYPE_OUT, CMD_CLOCK, 0, IFACE_NUMBER, tdat, 3, TIMEOUT_MS);
	}

	libusb_release_interface(devh, 0);
	libusb_close(devh);
	libusb_exit(NULL);
	return 0;
}

