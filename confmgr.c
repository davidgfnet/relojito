
#include <stddef.h>
#include <string.h>
#include "confmgr.h"

#include <libopencm3/stm32/flash.h>

#define FLASH_CONFIG_ADDR      (0x08000000 + 63*1024)    // Use "last" flash block #63

// Default config if flash is corrupt, is to use a regular white clock
const t_ruleset def_rules = {
	.rule_cnt = 1,
	.rules = {
		{ .condition = {.start_time = 0, .end_time = 0, .dow = 0, .dom = 0},
		  .state = {.tran = 0, .flags = 0,
	      .mcol = {0xff, 0x00, 0x7f}, .gcol = {0xff, 0x00, 0x7f}, .ecol = {0xff, 0x00, 0x7f} }
	    }
	}
};

typedef struct {
	uint32_t checksum;
	t_ruleset rs;
} t_rule_flash_cfg;

static uint32_t calc_checksum(const t_rule_flash_cfg *cfg) {
	uint32_t ret = 0xc1f94e12;
	for (unsigned i = offsetof(t_rule_flash_cfg, rs)/sizeof(uint32_t); i < sizeof(*cfg)/sizeof(uint32_t); i++)
		ret ^= ((uint32_t*)cfg)[i];
	return ret;
}

static int invalid_cfg(const t_rule_flash_cfg *cfg) {
	if (cfg->checksum != calc_checksum(cfg))
		return 1;

	if (cfg->rs.rule_cnt >= MAX_RULES)
		return 2;

	return 0;
}

const t_ruleset * current_ruleset() {
	// Validate the contents of the flash
	const t_rule_flash_cfg *cfg = (t_rule_flash_cfg*)FLASH_CONFIG_ADDR;
	if (!invalid_cfg(cfg))
		return &cfg->rs;

	// Return default hardcoded configuration
	return &def_rules;
}

int flash_new_cfg(const t_ruleset *rs) {
	t_rule_flash_cfg flashcfg;
	memcpy(&flashcfg.rs, rs, sizeof(*rs));

	// Generate checksum for it
	flashcfg.checksum = calc_checksum(&flashcfg);

	// Validate it before running it
	int ierr = invalid_cfg(&flashcfg);
	if (ierr)
		return ierr;

	flash_unlock();
	flash_erase_page(FLASH_CONFIG_ADDR);   // Single page config

	// Flash config now
	const uint16_t *databuf = (uint16_t*)&flashcfg;
	for (unsigned i = 0; i < 1024/2; i++)
		flash_program_half_word(FLASH_CONFIG_ADDR + i*2, databuf[i]);

	flash_lock();
	return 0;
}


