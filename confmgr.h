
#ifndef CONFMGR__HH__
#define CONFMGR__HH__

#include "rules.h"

#define MAX_RULES   16

typedef struct {
	uint32_t rule_cnt;
	t_rule rules[MAX_RULES];
} t_ruleset;

const t_ruleset * current_ruleset();
int flash_new_cfg(const t_ruleset *rs);

#endif

