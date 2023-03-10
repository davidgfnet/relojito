
#include <stdint.h>

typedef struct {
	uint16_t year;
	uint8_t month, day, hour, min, sec;
	uint8_t dayofweek;
} t_decomp_time;

t_decomp_time decompose_time(uint32_t ts);



