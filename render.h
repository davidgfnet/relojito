
// Renders a regular (kosher) time
// hour: 1 to 12, minutes: 0 to 59
uint32_t render_time(unsigned hour, unsigned minutes);

uint32_t render_late_time();

// Generate LED light buffer out of the expression
void blit_buffer(uint32_t *buffer, uint32_t bm, uint32_t color);

