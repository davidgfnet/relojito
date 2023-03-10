
#ifndef __RULES__HH__
#define __RULES__HH__

#include <stdint.h>

#define FLG_A_BONDIA      0x01   // bits 0-1 indicate bon/a X
#define FLG_A_BONATARD    0x02
#define FLG_A_BONANIT     0x03

#define MSK_GREET         0x03   // bits 0-1 indicate greet
#define MSK_A_DORMIR      0x04   // bit 2 indicates "a dormir"
#define MSK_TARGET        0xF0   // bits 4-5 indicate what to render (0 show time, 1 show 3/4d15, other nothing)

#define TRAN_IN(x)         ((x) & 0xF)
#define TRAN_OUT(x)        ((x) >>  4)
#define SHOW_A_DORMIR(x)   ((x) & MSK_A_DORMIR)
#define SHOW_TARGET(x)     (((x) & MSK_TARGET) >> 4)

typedef struct {
	uint16_t start_time, end_time;      // Interval time
	uint8_t dow, dom;                   // Day of week (inverted bitmap) and month (1-31)
	uint8_t pwm_cnt, pwm_period;        // Counter and period for PWM messages
} t_rule_cond;

typedef struct {
	uint8_t tran;        // 0 default, 1 no transition, 2 fade in/out, 3 falling, 4 typewriter, ...
	uint8_t tran_speed;  // 16 LUT speed
	uint8_t flags;       // Bitfields (see above)
	uint8_t mcol[3], gcol[3], ecol[3];
} t_rule_state;

typedef struct {
	t_rule_cond  condition;
	t_rule_state state;
} t_rule;

extern const uint32_t greet_tbl[4];

const t_rule_state* evaluate_rules(const t_rule *rules, unsigned rulecnt, uint32_t utime);

#endif


