
#include "rules.h"
#include "util.h"
#include "constants.h"

#define SECS_IN_DAY  (24*60*60)

const uint32_t greet_tbl[4] = {
	0,
	GREET_BONDIA,
	GREET_BONATARDA,
	GREET_BONANIT
};

// Default rule (if none applies) is to display the time using white color
static const t_rule_state def_state = {
	.tran = 0, .tran_speed = 0, .flags = 0,
	.mcol = {0xff, 0xff, 0xff}, .gcol = {0xff, 0xff, 0xff}, .ecol = {0xff, 0xff, 0xff},
};

// Evaluates rules, being 0 the default rule (last to evaluate, or to fallback)
const t_rule_state* evaluate_rules(const t_rule *rules, unsigned rulecnt, uint32_t utime) {
	t_decomp_time tdec = decompose_time(utime);
	unsigned daymin = tdec.min + tdec.hour * 60;   // Between 0 and 11*60 + 59
	unsigned dow = tdec.dayofweek;

	for (unsigned i = 0; i < rulecnt; i++) {
		// Check if time restriction matches (or doesnt apply)
		const t_rule_cond *cond = &rules[i].condition;
		if ((cond->start_time == 0 && cond->end_time == 0) ||
		    (daymin >= cond->start_time && daymin <= cond->end_time)) {
			if ((~cond->dow) & (1 << dow)) {
				if (!cond->dom || cond->dom == tdec.day) {
					// Process pwm for the current time
					uint32_t time_ref = (utime % SECS_IN_DAY) - cond->start_time * 60;
					// time_ref is a value in seconds from 0 to #secs in day, starting at rule edge
					uint32_t pwm_val = time_ref % ((cond->pwm_period + 1) * 2);
					if (!cond->pwm_period || !cond->pwm_cnt || pwm_val < cond->pwm_cnt) {
						// This rule evaluates to true!
						return &rules[i].state;
					}
				}
			}
		}
	}
	// Default rule!
	return &def_state;
}

