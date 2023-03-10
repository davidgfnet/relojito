
#include <stdint.h>

#include "constants.h"
#include "render.h"


// Calculate time to render
// Time goes in the format:
//   O'Clock and up to 7 minutes past:
//      -->  Es|Son La|Les Una|Dos|Tres|...|Dotze EnPunt|BenTocades
//   Other times
//      -->  Es|Son Un|Dos|Tres Quarts IMig? D'|De Una|Dos|Tres|...|Dotze
//   Fun times
//      -->  Son Tres Quarts De Quinze

// Renders a regular (kosher) time
// hour: 1 to 12, minutes: 0 to 59
uint32_t render_time(unsigned hour, unsigned minutes) {
	uint32_t ret = 0;
	unsigned next_hour = (hour == 12) ? 1 : (hour + 1);

	if (minutes >= 58 || minutes <= 8) {
		unsigned curhour = (minutes >= 58) ? next_hour : hour;

		if (curhour == 1)
			ret |= (VERB_ES | ART_LA | HOUR_UNA);
		else
			ret |= (VERB_SON | ART_LES | (HOUR_ZERO << curhour));
		
		if (minutes > 2 && minutes < 10) {
			if (hour == 1)
				ret |= HOUR_TOCADE;  // wow much hack
			else
				ret |= HOUR_TOCADES;
		}
		else
			ret |= HOUR_ENPUNT;
	}
	else {
		if (minutes >= 51)
			ret |= (VERB_SON | QUART_TRES | QUART_QUARTS | QUART_IMIG);
		else if (minutes >= 43)
			ret |= (VERB_SON | QUART_TRES | QUART_QUARTS);
		else if (minutes >= 35)
			ret |= (VERB_SON | QUART_DOS  | QUART_QUARTS | QUART_IMIG);
		else if (minutes >= 27)
			ret |= (VERB_SON | QUART_DOS  | QUART_QUARTS);
		else if (minutes >= 23)
			ret |= (VERB_ES  | QUART_UN   | QUART_QUART  | QUART_IMIG);
		else
			ret |= (VERB_ES  | QUART_UN   | QUART_QUART);

		ret |= (next_hour == 1 || next_hour == 11) ? PREP_D : PREP_DE;
		ret |= (HOUR_ZERO << next_hour);
	}
	return ret;
}

//  Llegenda:
//  [58 ..  2]  En punt
//  [ 3 ..  8]  Ben tocades
//  [9 ..  22]  Un quart (molt aprox)
//  [23 .. 26]  Un quart i mig
//  [27 .. 34]  Dos quarts
//  [35 .. 42]  Dos quarts i mig
//  [43 .. 50]  Tres quarts
//  [51 .. 57]  Tres quarts i mig

uint32_t render_late_time() {
	return VERB_SON | QUART_TRES | QUART_QUARTS | PREP_DE | CHEEKY_QUINZE;
}

// Generate LED light buffer out of the expression
void blit_buffer(uint32_t *buffer, uint32_t bm, uint32_t color) {
	for (unsigned i = 0; i < 12*12; i++)
		if (charmap[i] & bm)
			buffer[i] = color;
}


