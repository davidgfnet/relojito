/*
 * Copyright (C) 2023 David Guillen Fandos <david@davidgf.net>
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
#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencmsis/core_cm3.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/iwdg.h>


#include "render.h"
#include "rules.h"
#include "confmgr.h"
#include "constants.h"

#define MODE_NORMAL       0  // Displays the clock, default state
#define MODE_DEMO_PLAY    1  // Plays some rule for the user to preview it
#define MODE_TEST         2  // Plays some "testing" pattern to check LEDs
#define MODE_DEVDEMO      3  // Some cool patterns to test the device LEDs in all their glory

unsigned work_mode = MODE_NORMAL;
unsigned main_color = 0x005060; //0xa0a0a0;  // def 0x005060

volatile char spi_dma_done = 1, usarttx_dma_done = 1;

t_rule_state demo_action = {0};
char usart_rxb[1024], usart_txb[1024];
volatile int usart_rxlen = 0, usart_txlen = 0, usarttx_dma_size = 0;

#define LED_PORT_NAME     GPIOB
#define LED_PORT_PIN      GPIO5

#define BLE_UART_PORT     GPIOA
#define BLE_UART_PIN_TX   GPIO_USART2_TX   // PA2
#define BLE_UART_PIN_RX   GPIO_USART2_RX   // PA3
#define BLE_UART_DEV      USART2

#define LED_ROWS            12
#define LED_COLS            12
#define LED_CNT      (LED_ROWS * LED_COLS)
#define MS_UPDATE_FREQ      10    // (10ms, refresh rate is 100Hz)

// For speed, use a lookup table. Each byte is expanded into 3 bytes.
// for val in range(256):
//  code = int("".join("011" if val & (1 << i) else "001" for i in range(8)), 2)
//  print("{0x%02x, 0x%02x, 0x%02x}," % (code & 0xff, (code >> 8) & 0xff, (code >> 16) & 0xff))
const uint8_t lookup_tbl[256][3] = {
	{0x49, 0x92, 0x24},{0x49, 0x92, 0x64},{0x49, 0x92, 0x2c},{0x49, 0x92, 0x6c},
	{0x49, 0x92, 0x25},{0x49, 0x92, 0x65},{0x49, 0x92, 0x2d},{0x49, 0x92, 0x6d},
	{0x49, 0xb2, 0x24},{0x49, 0xb2, 0x64},{0x49, 0xb2, 0x2c},{0x49, 0xb2, 0x6c},
	{0x49, 0xb2, 0x25},{0x49, 0xb2, 0x65},{0x49, 0xb2, 0x2d},{0x49, 0xb2, 0x6d},
	{0x49, 0x96, 0x24},{0x49, 0x96, 0x64},{0x49, 0x96, 0x2c},{0x49, 0x96, 0x6c},
	{0x49, 0x96, 0x25},{0x49, 0x96, 0x65},{0x49, 0x96, 0x2d},{0x49, 0x96, 0x6d},
	{0x49, 0xb6, 0x24},{0x49, 0xb6, 0x64},{0x49, 0xb6, 0x2c},{0x49, 0xb6, 0x6c},
	{0x49, 0xb6, 0x25},{0x49, 0xb6, 0x65},{0x49, 0xb6, 0x2d},{0x49, 0xb6, 0x6d},
	{0xc9, 0x92, 0x24},{0xc9, 0x92, 0x64},{0xc9, 0x92, 0x2c},{0xc9, 0x92, 0x6c},
	{0xc9, 0x92, 0x25},{0xc9, 0x92, 0x65},{0xc9, 0x92, 0x2d},{0xc9, 0x92, 0x6d},
	{0xc9, 0xb2, 0x24},{0xc9, 0xb2, 0x64},{0xc9, 0xb2, 0x2c},{0xc9, 0xb2, 0x6c},
	{0xc9, 0xb2, 0x25},{0xc9, 0xb2, 0x65},{0xc9, 0xb2, 0x2d},{0xc9, 0xb2, 0x6d},
	{0xc9, 0x96, 0x24},{0xc9, 0x96, 0x64},{0xc9, 0x96, 0x2c},{0xc9, 0x96, 0x6c},
	{0xc9, 0x96, 0x25},{0xc9, 0x96, 0x65},{0xc9, 0x96, 0x2d},{0xc9, 0x96, 0x6d},
	{0xc9, 0xb6, 0x24},{0xc9, 0xb6, 0x64},{0xc9, 0xb6, 0x2c},{0xc9, 0xb6, 0x6c},
	{0xc9, 0xb6, 0x25},{0xc9, 0xb6, 0x65},{0xc9, 0xb6, 0x2d},{0xc9, 0xb6, 0x6d},
	{0x59, 0x92, 0x24},{0x59, 0x92, 0x64},{0x59, 0x92, 0x2c},{0x59, 0x92, 0x6c},
	{0x59, 0x92, 0x25},{0x59, 0x92, 0x65},{0x59, 0x92, 0x2d},{0x59, 0x92, 0x6d},
	{0x59, 0xb2, 0x24},{0x59, 0xb2, 0x64},{0x59, 0xb2, 0x2c},{0x59, 0xb2, 0x6c},
	{0x59, 0xb2, 0x25},{0x59, 0xb2, 0x65},{0x59, 0xb2, 0x2d},{0x59, 0xb2, 0x6d},
	{0x59, 0x96, 0x24},{0x59, 0x96, 0x64},{0x59, 0x96, 0x2c},{0x59, 0x96, 0x6c},
	{0x59, 0x96, 0x25},{0x59, 0x96, 0x65},{0x59, 0x96, 0x2d},{0x59, 0x96, 0x6d},
	{0x59, 0xb6, 0x24},{0x59, 0xb6, 0x64},{0x59, 0xb6, 0x2c},{0x59, 0xb6, 0x6c},
	{0x59, 0xb6, 0x25},{0x59, 0xb6, 0x65},{0x59, 0xb6, 0x2d},{0x59, 0xb6, 0x6d},
	{0xd9, 0x92, 0x24},{0xd9, 0x92, 0x64},{0xd9, 0x92, 0x2c},{0xd9, 0x92, 0x6c},
	{0xd9, 0x92, 0x25},{0xd9, 0x92, 0x65},{0xd9, 0x92, 0x2d},{0xd9, 0x92, 0x6d},
	{0xd9, 0xb2, 0x24},{0xd9, 0xb2, 0x64},{0xd9, 0xb2, 0x2c},{0xd9, 0xb2, 0x6c},
	{0xd9, 0xb2, 0x25},{0xd9, 0xb2, 0x65},{0xd9, 0xb2, 0x2d},{0xd9, 0xb2, 0x6d},
	{0xd9, 0x96, 0x24},{0xd9, 0x96, 0x64},{0xd9, 0x96, 0x2c},{0xd9, 0x96, 0x6c},
	{0xd9, 0x96, 0x25},{0xd9, 0x96, 0x65},{0xd9, 0x96, 0x2d},{0xd9, 0x96, 0x6d},
	{0xd9, 0xb6, 0x24},{0xd9, 0xb6, 0x64},{0xd9, 0xb6, 0x2c},{0xd9, 0xb6, 0x6c},
	{0xd9, 0xb6, 0x25},{0xd9, 0xb6, 0x65},{0xd9, 0xb6, 0x2d},{0xd9, 0xb6, 0x6d},
	{0x4b, 0x92, 0x24},{0x4b, 0x92, 0x64},{0x4b, 0x92, 0x2c},{0x4b, 0x92, 0x6c},
	{0x4b, 0x92, 0x25},{0x4b, 0x92, 0x65},{0x4b, 0x92, 0x2d},{0x4b, 0x92, 0x6d},
	{0x4b, 0xb2, 0x24},{0x4b, 0xb2, 0x64},{0x4b, 0xb2, 0x2c},{0x4b, 0xb2, 0x6c},
	{0x4b, 0xb2, 0x25},{0x4b, 0xb2, 0x65},{0x4b, 0xb2, 0x2d},{0x4b, 0xb2, 0x6d},
	{0x4b, 0x96, 0x24},{0x4b, 0x96, 0x64},{0x4b, 0x96, 0x2c},{0x4b, 0x96, 0x6c},
	{0x4b, 0x96, 0x25},{0x4b, 0x96, 0x65},{0x4b, 0x96, 0x2d},{0x4b, 0x96, 0x6d},
	{0x4b, 0xb6, 0x24},{0x4b, 0xb6, 0x64},{0x4b, 0xb6, 0x2c},{0x4b, 0xb6, 0x6c},
	{0x4b, 0xb6, 0x25},{0x4b, 0xb6, 0x65},{0x4b, 0xb6, 0x2d},{0x4b, 0xb6, 0x6d},
	{0xcb, 0x92, 0x24},{0xcb, 0x92, 0x64},{0xcb, 0x92, 0x2c},{0xcb, 0x92, 0x6c},
	{0xcb, 0x92, 0x25},{0xcb, 0x92, 0x65},{0xcb, 0x92, 0x2d},{0xcb, 0x92, 0x6d},
	{0xcb, 0xb2, 0x24},{0xcb, 0xb2, 0x64},{0xcb, 0xb2, 0x2c},{0xcb, 0xb2, 0x6c},
	{0xcb, 0xb2, 0x25},{0xcb, 0xb2, 0x65},{0xcb, 0xb2, 0x2d},{0xcb, 0xb2, 0x6d},
	{0xcb, 0x96, 0x24},{0xcb, 0x96, 0x64},{0xcb, 0x96, 0x2c},{0xcb, 0x96, 0x6c},
	{0xcb, 0x96, 0x25},{0xcb, 0x96, 0x65},{0xcb, 0x96, 0x2d},{0xcb, 0x96, 0x6d},
	{0xcb, 0xb6, 0x24},{0xcb, 0xb6, 0x64},{0xcb, 0xb6, 0x2c},{0xcb, 0xb6, 0x6c},
	{0xcb, 0xb6, 0x25},{0xcb, 0xb6, 0x65},{0xcb, 0xb6, 0x2d},{0xcb, 0xb6, 0x6d},
	{0x5b, 0x92, 0x24},{0x5b, 0x92, 0x64},{0x5b, 0x92, 0x2c},{0x5b, 0x92, 0x6c},
	{0x5b, 0x92, 0x25},{0x5b, 0x92, 0x65},{0x5b, 0x92, 0x2d},{0x5b, 0x92, 0x6d},
	{0x5b, 0xb2, 0x24},{0x5b, 0xb2, 0x64},{0x5b, 0xb2, 0x2c},{0x5b, 0xb2, 0x6c},
	{0x5b, 0xb2, 0x25},{0x5b, 0xb2, 0x65},{0x5b, 0xb2, 0x2d},{0x5b, 0xb2, 0x6d},
	{0x5b, 0x96, 0x24},{0x5b, 0x96, 0x64},{0x5b, 0x96, 0x2c},{0x5b, 0x96, 0x6c},
	{0x5b, 0x96, 0x25},{0x5b, 0x96, 0x65},{0x5b, 0x96, 0x2d},{0x5b, 0x96, 0x6d},
	{0x5b, 0xb6, 0x24},{0x5b, 0xb6, 0x64},{0x5b, 0xb6, 0x2c},{0x5b, 0xb6, 0x6c},
	{0x5b, 0xb6, 0x25},{0x5b, 0xb6, 0x65},{0x5b, 0xb6, 0x2d},{0x5b, 0xb6, 0x6d},
	{0xdb, 0x92, 0x24},{0xdb, 0x92, 0x64},{0xdb, 0x92, 0x2c},{0xdb, 0x92, 0x6c},
	{0xdb, 0x92, 0x25},{0xdb, 0x92, 0x65},{0xdb, 0x92, 0x2d},{0xdb, 0x92, 0x6d},
	{0xdb, 0xb2, 0x24},{0xdb, 0xb2, 0x64},{0xdb, 0xb2, 0x2c},{0xdb, 0xb2, 0x6c},
	{0xdb, 0xb2, 0x25},{0xdb, 0xb2, 0x65},{0xdb, 0xb2, 0x2d},{0xdb, 0xb2, 0x6d},
	{0xdb, 0x96, 0x24},{0xdb, 0x96, 0x64},{0xdb, 0x96, 0x2c},{0xdb, 0x96, 0x6c},
	{0xdb, 0x96, 0x25},{0xdb, 0x96, 0x65},{0xdb, 0x96, 0x2d},{0xdb, 0x96, 0x6d},
	{0xdb, 0xb6, 0x24},{0xdb, 0xb6, 0x64},{0xdb, 0xb6, 0x2c},{0xdb, 0xb6, 0x6c},
	{0xdb, 0xb6, 0x25},{0xdb, 0xb6, 0x65},{0xdb, 0xb6, 0x2d},{0xdb, 0xb6, 0x6d},
};

// Format is GRB888
uint8_t spibuf[LED_CNT * 24 * 3 / 8];
void push_buffer_via_spi(const uint32_t *buffer, unsigned bsize) {
	// We use 3 SPI bits per encoded bit
	// 0 is encoded as 100, 1 is encoded as 110
	unsigned off = 0;
	while (bsize--) {
		unsigned int val = *buffer++;
		for (unsigned i = 0; i < 3; i++) {
			uint8_t valb = val >> ((2-i) * 8);
			spibuf[off++] = lookup_tbl[valb][0];
			spibuf[off++] = lookup_tbl[valb][1];
			spibuf[off++] = lookup_tbl[valb][2];
		}
	}

	// Now program the DMA to transfer all the bits. We send the LSB first.
	spi_dma_done = 0;
	dma_channel_reset(DMA1, DMA_CHANNEL3);
	dma_set_priority(DMA1, DMA_CHANNEL3, DMA_CCR_PL_HIGH);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t)&SPI1_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)spibuf);
	dma_set_number_of_data(DMA1, DMA_CHANNEL3, sizeof(spibuf));
	dma_set_read_from_memory(DMA1, DMA_CHANNEL3);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_8BIT);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);

	dma_enable_channel(DMA1, DMA_CHANNEL3);
	spi_enable_tx_dma(SPI1);
}

void dma1_channel3_isr(void) {
	// Clear flag before returning
	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL3, DMA_TCIF);

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
	spi_disable_tx_dma(SPI1);
	dma_disable_channel(DMA1, DMA_CHANNEL3);

	spi_dma_done = 1;
}

void dma1_channel7_isr(void) {
	// Clear flag before returning
	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL7, DMA_TCIF);

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL7);
	usart_disable_tx_dma(BLE_UART_DEV);
	dma_disable_channel(DMA1, DMA_CHANNEL7);

	// Proceed to empty the transmitted bytes from the buffer.
	// This is a bit slow but most of the time we won't have more stuff in the buffer
	// since communication is not pipelined.
	usart_txlen -= usarttx_dma_size;
	if (usart_txlen)
		memmove(usart_txb, &usart_txb[usarttx_dma_size], usart_txlen);

	usarttx_dma_done = 1;
}

static uint8_t unhex(char hc) {
	if (hc >= '0' && hc <= '9')
		return hc - '0';
	if (hc >= 'a' && hc <= 'f')
		return hc - 'a' + 10;
	return hc - 'A' + 10;
}

static char tohex(unsigned n) {
	static const char * const hexc = "0123456789abcdef";
	return hexc[n];
}

int push_usart_response(uint8_t *data, unsigned len) {
	// Prevent buffer overflows
	if (usart_txlen + 13 + len*2 > sizeof(usart_txb))
		return 0;

	// Push header, size, checksum and the actual payload
	memcpy(&usart_txb[usart_txlen], "HZR_", 4);
	usart_txb[usart_txlen + 4] = '@' + (len >> 5);
	usart_txb[usart_txlen + 5] = '@' + (len & 0x1F);

	uint8_t chksum = 0xde;
	for (unsigned i = 0; i < len; i++)
		chksum ^= data[i];

	uint16_t fchksum = chksum | ((~chksum) << 8);

	usart_txb[usart_txlen + 6] = tohex((fchksum >> 12) & 0xF);
	usart_txb[usart_txlen + 7] = tohex((fchksum >>  8) & 0xF);
	usart_txb[usart_txlen + 8] = tohex((fchksum >>  4) & 0xF);
	usart_txb[usart_txlen + 9] = tohex((fchksum >>  0) & 0xF);
	usart_txlen += 10;

	for (unsigned i = 0; i < len; i++) {
		usart_txb[usart_txlen++] = tohex(data[i] >> 4);
		usart_txb[usart_txlen++] = tohex(data[i] & 15);
	}
	usart_txb[usart_txlen++] = '\n';  // Serial terminals are very stupid :P
	return 1;
}


static void process_payload(uint8_t *data, unsigned len) {
	switch (data[0]) {
	case 0:
		// Read clock value
		{
			uint32_t clkval = rtc_get_counter_val();
			uint8_t buffer[5] = { 0, clkval >> 0, clkval >> 8, clkval >> 16, clkval >> 24 }; 
			push_usart_response(buffer, sizeof(buffer));
		}
		break;
	case 1:
		// Read config bundle
		{
			const t_ruleset * rs = current_ruleset();
			uint8_t buffer[sizeof(t_ruleset) + 1] = { 1 };
			memcpy(&buffer[1], rs, sizeof(t_ruleset));
			push_usart_response(buffer, sizeof(buffer));
		}
		break;
	case 2:
		// Get current mode
		{
			uint8_t resp[2] = {2, work_mode};
			push_usart_response(resp, sizeof(resp));
		}
		break;
	case 4:
		// Set clock value
		if (len == 1+4)
			rtc_set_counter_val(*(uint32_t*)&data[1]);
		break;
	case 5:
		// Set config bundle
		if (len == 1+sizeof(t_ruleset)) {
			char resp[] = "\005FLASHOK";
			int errcode = flash_new_cfg((t_ruleset*)&data[1]);
			if (errcode) {
				resp[6] = 'E'; resp[7] = '0' + errcode;
			}
			push_usart_response((uint8_t*)resp, 8);
		}
		break;
	case 6:
		// Push demo bundle, this is just a t_rule_state that will be "replayed"
		if (len == 1+sizeof(t_rule_state)) {
			// demo_action
			memcpy(&demo_action, &data[1], sizeof(t_rule_state));
		}
		break;
	case 7:
		// Change mode (with timeout)
		if (len >= 2)
			work_mode = data[1];
		break;
	};
}

void usart2_isr(void) {
	// Check if we received a character
	if (USART_SR(BLE_UART_DEV) & USART_SR_RXNE) {
		// Read the byte into the usart buffer
		char data = usart_recv(BLE_UART_DEV);
		// Ignore end line characters on purpose.
		if (data != '\n' && usart_rxlen < sizeof(usart_rxb))
			usart_rxb[usart_rxlen++] = data;
	}
}

static void usart_poll() {
	// Program DMA UART transmissions if any data is queued
	if (usart_txlen && usarttx_dma_done) {
		usarttx_dma_done = 0;
		usarttx_dma_size = usart_txlen;

		// Prepare DMA to transmit UART data
		dma_channel_reset(DMA1, DMA_CHANNEL7);
		dma_set_peripheral_address(DMA1, DMA_CHANNEL7, (uint32_t)&USART2_DR);
		dma_set_memory_address(DMA1, DMA_CHANNEL7, (uint32_t)usart_txb);
		dma_set_number_of_data(DMA1, DMA_CHANNEL7, usarttx_dma_size);
		dma_set_read_from_memory(DMA1, DMA_CHANNEL7);
		dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL7);
		dma_set_peripheral_size(DMA1, DMA_CHANNEL7, DMA_CCR_PSIZE_8BIT);
		dma_set_memory_size(DMA1, DMA_CHANNEL7, DMA_CCR_MSIZE_8BIT);
		dma_set_priority(DMA1, DMA_CHANNEL7, DMA_CCR_PL_VERY_HIGH);
		dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL7);
		dma_enable_channel(DMA1, DMA_CHANNEL7);
		usart_enable_tx_dma(BLE_UART_DEV);
	}
}

// Packet format looks as follows:
// HDR LEN Checksum Data (Hex encoded) Newline
// Returns 1 if some amount of data was consumed
static int process_usart() {
	int recvlen = usart_rxlen;

	// Smallest packet size is 10
	if (recvlen < 12)
		return 0;

	// Consume input until a header is found
	unsigned p;
	for (p = 0; p < recvlen-3; p++) {
		if (usart_rxb[p+0] == 'H' &&
		    usart_rxb[p+1] == 'Z' &&
		    usart_rxb[p+2] == 'R' &&
		    usart_rxb[p+3] == '_')
			break;
	}
	// Discard data before the header arrived (or all of it if not found)
	if (p) {
		cm_disable_interrupts();  // Ugly but we need to hold reception for a bit
		usart_rxlen -= p;
		memmove(usart_rxb, &usart_rxb[p], usart_rxlen);
		cm_enable_interrupts();
		return 1;
	}

	if (recvlen < 12)
		return 0;

	// Decode length and check whether we can keep on processing
	char lenhi = usart_rxb[4], lenlo = usart_rxb[5];
	if (!(lenhi >= '@' && lenhi <= 'O') || !(lenlo >= '@' && lenlo <= '_')) {
		// Invalid length? Skip this packet, find the next one
		memset(usart_rxb, 0, 4);
		return 1;
	}
	unsigned pktlen = ((lenhi - '@') << 5) | (lenlo - '@');  // [0..511]
	if (pktlen*2+10 > recvlen)
		return 0;    // Partial packet

	// Process packet. Decode hex payload inplace, a bit of a hack :P
	uint8_t cksum = 0xde;
	char *bf = &usart_rxb[10];
	uint8_t *bf8 = (uint8_t*)&usart_rxb[10];
	for (unsigned i = 0; i < pktlen; i++) {
		bf8[i] = (unhex(bf[i*2+0]) << 4) | unhex(bf[i*2+1]);
		cksum ^= bf8[i];
	}

	uint16_t calccheck = cksum | ((~cksum) << 8);
	uint16_t rcheck = (unhex(usart_rxb[6]) << 12) |
	                  (unhex(usart_rxb[7]) <<  8) |
	                  (unhex(usart_rxb[8]) <<  4) |
	                  (unhex(usart_rxb[9]));
	if (calccheck != rcheck) {
		// Invalid checksum, skip packet!
		memset(usart_rxb, 0, 4);
		return 1;
	}

	// Call process_packet routines
	process_payload(bf8, pktlen);

	// Consume data for the next packet
	cm_disable_interrupts();
	usart_rxlen -= pktlen*2+10;
	if (usart_rxlen)
		memmove(usart_rxb, &usart_rxb[pktlen*2+10], usart_rxlen);
	cm_enable_interrupts();

	return 1;
}

void start_usb();
void usbpoll();

void init_clock() {
	// SysTick interrupt every N clock pulses: set reload to N-1
	// Interrupt every ms assuming we have 72MHz clock
	nvic_set_priority(NVIC_SYSTICK_IRQ, 0);
	nvic_enable_irq(NVIC_SYSTICK_IRQ);
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	systick_set_reload(8999);
	systick_interrupt_enable();
	systick_counter_enable();
}

volatile uint32_t systick_counter_ms = 0;
void sys_tick_handler() {
	systick_counter_ms++;
}

int main() {
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	// Enable GPIO clocks
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	rcc_periph_clock_enable(RCC_USART2);
	rcc_periph_clock_enable(RCC_AFIO);

	// Setup SPI and DMA
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_SPI1);

	// Enable it as SPI1_TX pin, need remapping!
	AFIO_MAPR |= AFIO_MAPR_SPI1_REMAP;
	gpio_set_mode(LED_PORT_NAME, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, LED_PORT_PIN);

	rcc_periph_reset_pulse(RST_SPI1);
	SPI1_I2SCFGR = 0;

	// Use 36Mhz / 16 = 2.25Mhz (T=444ns)
	spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_32, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
		SPI_CR1_CPHA_CLK_TRANSITION_2, SPI_CR1_DFF_8BIT, SPI_CR1_LSBFIRST);
	// spi_set_unidirectional_mode(SPI1);
	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);
	spi_enable(SPI1);

	// Configure RTC (if it is not running already!)
	// rtc_auto_awake(RCC_HSE, 62500);    // 8MHz / 128 / 62500 = 1Hz
	rtc_auto_awake(RCC_LSE, 32768);    // 32768Hz / 2^15 = 1Hz

	// Initialize the UART comms to the BLE device
	gpio_set_mode(BLE_UART_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, BLE_UART_PIN_TX);
	gpio_set_mode(BLE_UART_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BLE_UART_PIN_RX);
	usart_set_baudrate(BLE_UART_DEV, 9600);
	usart_set_databits(BLE_UART_DEV, 8);
	usart_set_stopbits(BLE_UART_DEV, USART_STOPBITS_1);
	usart_set_parity(BLE_UART_DEV, USART_PARITY_NONE);
	usart_set_mode(BLE_UART_DEV, USART_MODE_TX_RX);
	usart_set_flow_control(BLE_UART_DEV, USART_FLOWCONTROL_NONE);
	usart_enable_rx_interrupt(BLE_UART_DEV);
	usart_enable(BLE_UART_DEV);

	// 1ms clock for effects and stuff
	init_clock();

	// Setup interrupts. UART should have a higher interrupt.
	nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);
	nvic_set_priority(NVIC_DMA1_CHANNEL7_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL7_IRQ);
	nvic_set_priority(NVIC_USART2_IRQ, 0);
	nvic_enable_irq(NVIC_USART2_IRQ);

	start_usb();

	int butst = 0;
	uint32_t colbuf[LED_CNT];
	uint64_t mscounter64 = 0, lastupd64 = 0;
	uint32_t prevmscnt = 0;

	while (1) {
		uint32_t secs = rtc_get_counter_val();
		unsigned hours = (secs / 3600) % 12; // Hours
		unsigned mints = (secs / 60) % 60;   // Mins
		if (!hours) hours = 12;

		if (work_mode == MODE_NORMAL || work_mode == MODE_DEMO_PLAY) {
			const t_rule_state * action;
			if (work_mode == MODE_NORMAL) {
				const t_ruleset * rs = current_ruleset();
				action = evaluate_rules(rs->rules, rs->rule_cnt, secs);
			} else {
				action = &demo_action;
			}

			uint32_t extra_code = SHOW_A_DORMIR(action->flags) ? CHEEKY_ADORMIR : 0;
			uint32_t greet_code = greet_tbl[action->flags & MSK_GREET];
			uint32_t main_code = 0;
			switch (SHOW_TARGET(action->flags)) {
			case 0:
				main_code = render_time(hours, mints); break;
			case 1:
				main_code = render_late_time(); break;
			default:
				break;
			};

			// GRB888 format/ordering for the LEDs
			uint32_t main_color  = (action->mcol[1] << 16) | (action->mcol[0] << 8) | action->mcol[2];
			uint32_t greet_color = (action->gcol[1] << 16) | (action->gcol[0] << 8) | action->gcol[2];
			uint32_t extra_color = (action->ecol[1] << 16) | (action->ecol[0] << 8) | action->ecol[2];

			// Clear buffer with black, to blit on top of it.
			memset(&colbuf[0], 0, sizeof(colbuf));

			blit_buffer(&colbuf[0], main_code,  main_color);
			blit_buffer(&colbuf[0], greet_code, greet_color);
			blit_buffer(&colbuf[0], extra_code, extra_color);

			// Do the row swap :)
			for (unsigned i = 1; i < LED_ROWS; i += 2) {
				for (unsigned j = 0; j < LED_COLS / 2; j++) {
					uint32_t t = colbuf[i * LED_COLS + j];
					colbuf[i * LED_COLS + j] = colbuf[i * LED_COLS + (LED_COLS - 1 - j)];
					colbuf[i * LED_COLS + (LED_COLS - 1 - j)] = t;
				}
			}
		}
		else if (work_mode == MODE_TEST) {
			// We light an LED every 100ms
			uint16_t mstick = systick_counter_ms / 100;
			memset(colbuf, 0, sizeof(colbuf));
			colbuf[mstick % LED_CNT] = 0xFFFFFF;   // White full brightness
		}
		else if (work_mode == MODE_DEVDEMO) {
			// TODO: Implement some funny demo mode
		}

		// Handle button
		int bt = gpio_get(GPIOB, GPIO9);
		if (bt != butst) {
			if (bt) {
				// Add one minute
				uint32_t secs = rtc_get_counter_val();
				rtc_set_counter_val(secs + 60);
			}
			butst = bt;
		}

		// Blaster the information to the LEDs using SPI+DMA
		// This takes ~1.2us*144*24 = 4.2ms, so we update at 100Hz (every 10ms)
		if (spi_dma_done && lastupd64 + MS_UPDATE_FREQ < mscounter64) {
			lastupd64 = mscounter64;
			push_buffer_via_spi(colbuf, LED_CNT);
		}

		// Check out the USART, and process any input chars
		usart_poll();
		// Process any incoming packets
		while (process_usart());

		// A bit of USB polling
		usbpoll();

		// Update 64 bit millisecond counter.
		uint32_t curr_time = systick_counter_ms;
		uint32_t mspass = curr_time - prevmscnt;
		prevmscnt = curr_time;
		mscounter64 += mspass;

		iwdg_reset();
	}
}

