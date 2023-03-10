
// Define the chunks of text that can be displayed
// Up to 32 independent-ish words

#define VERB_ES       (1U << 0)
#define VERB_SON      (1U << 1)

#define ART_LA        (1U << 2)
#define ART_LES       (1U << 3)

#define HOUR_ZERO     (1U << 3)   // Fake for hour math!!
#define HOUR_UNA      (1U << 4)
#define HOUR_DUES     (1U << 5)
#define HOUR_TRES     (1U << 6)
#define HOUR_QUATRE   (1U << 7)
#define HOUR_CINC     (1U << 8)
#define HOUR_SIS      (1U << 9)
#define HOUR_SET     (1U << 10)
#define HOUR_VUIT    (1U << 11)
#define HOUR_NOU     (1U << 12)
#define HOUR_DEU     (1U << 13)
#define HOUR_ONZE    (1U << 14)
#define HOUR_DOTZE   (1U << 15)
#define HOUR_ENPUNT  (1U << 16)
#define HOUR_TOCADE  (1U << 17)  // Lleideta FTW
#define HOUR_TOCADES (1U << 18)

#define QUART_UN     (1U << 19)
#define QUART_DOS    (1U << 20)
#define QUART_TRES   (1U << 21)
#define QUART_QUART  (1U << 22)
#define QUART_QUARTS (1U << 23)
#define QUART_IMIG   (1U << 24)

#define PREP_D       (1U << 25)
#define PREP_DE      (1U << 26)

#define GREET_BONDIA      (1U << 27)
#define GREET_BONATARDA   (1U << 28)
#define GREET_BONANIT     (1U << 29)

#define CHEEKY_QUINZE     (1U << 30)    // For "tres quarts de quinze"
#define CHEEKY_ADORMIR    (1U << 31)    // For emphasis!

// Map letters to different bits.
// Odd rows are written in reverse (since the LED strip goes backwards)

#define CH_BON      (GREET_BONATARDA | GREET_BONANIT | GREET_BONDIA)
#define CH_BONA     (GREET_BONATARDA | GREET_BONANIT)
#define CH_QUART    (QUART_QUART | QUART_QUARTS)
#define CH_BENTOC   (HOUR_TOCADE | HOUR_TOCADES)

// Coloring masks
#define COL_MSK_TIME   0x07FFFFFF
#define COL_MSK_GREET  0x38000000      // Bon dia/
#define COL_MSK_EXTRA  0x40000000      // A dormir!

// Note this is expressed left to right, row by row.
// This might need to be converted/fixed if the matrix is connected with reversed raws.

static const uint32_t charmap[12*12] = {
	// B O N A D I A T A R D A
	CH_BON, CH_BON, CH_BON, CH_BONA, GREET_BONDIA, GREET_BONDIA,
	GREET_BONDIA, GREET_BONATARDA, GREET_BONATARDA, GREET_BONATARDA, GREET_BONATARDA, GREET_BONATARDA,

	// N I T S A W D O R M I R
	GREET_BONANIT, GREET_BONANIT, GREET_BONANIT, 0, CHEEKY_ADORMIR, 0,
	CHEEKY_ADORMIR, CHEEKY_ADORMIR, CHEEKY_ADORMIR, CHEEKY_ADORMIR, CHEEKY_ADORMIR, CHEEKY_ADORMIR,

	// K E S O N H U N D O S X
	0, VERB_ES, VERB_ES | VERB_SON, VERB_SON, VERB_SON, 0,
	QUART_UN, QUART_UN, QUART_DOS, QUART_DOS, QUART_DOS, 0,

	// Y T R E S X Q U A R T S
	0, QUART_TRES, QUART_TRES, QUART_TRES, QUART_TRES, 0,
	CH_QUART, CH_QUART, CH_QUART, CH_QUART, CH_QUART, QUART_QUARTS,

	// I F M I G V L A L E S M
	QUART_IMIG, 0, QUART_IMIG, QUART_IMIG, QUART_IMIG, 0,
	ART_LA, ART_LA, ART_LES, ART_LES, ART_LES, 0,

	// D E Ḋ U N A N O U S I S
	PREP_DE, PREP_DE, PREP_D, HOUR_UNA, HOUR_UNA, HOUR_UNA,
	HOUR_NOU, HOUR_NOU, HOUR_NOU, HOUR_SIS, HOUR_SIS, HOUR_SIS,

	// Q U A T R E F D O T Z E
	HOUR_QUATRE, HOUR_QUATRE, HOUR_QUATRE, HOUR_QUATRE, HOUR_QUATRE, HOUR_QUATRE,
	0, HOUR_DOTZE, HOUR_DOTZE, HOUR_DOTZE, HOUR_DOTZE, HOUR_DOTZE,

	// T R E S Y S E T V U I T
	HOUR_TRES, HOUR_TRES, HOUR_TRES, HOUR_TRES, 0, HOUR_SET,
	HOUR_SET, HOUR_SET, HOUR_VUIT, HOUR_VUIT, HOUR_VUIT, HOUR_VUIT,

	// D U E S X D E U C I N C
	HOUR_DUES, HOUR_DUES, HOUR_DUES, HOUR_DUES, 0, HOUR_DEU,
	HOUR_DEU, HOUR_DEU, HOUR_CINC, HOUR_CINC, HOUR_CINC, HOUR_CINC,

	// O N Z E J E N W P U N T
	HOUR_ONZE, HOUR_ONZE, HOUR_ONZE, HOUR_ONZE, 0, HOUR_ENPUNT,
	HOUR_ENPUNT, 0, HOUR_ENPUNT, HOUR_ENPUNT, HOUR_ENPUNT, HOUR_ENPUNT,

	// G B E N P Q U I N Z E F
	0, CH_BENTOC, CH_BENTOC, CH_BENTOC, 0, CHEEKY_QUINZE,
	CHEEKY_QUINZE, CHEEKY_QUINZE, CHEEKY_QUINZE, CHEEKY_QUINZE, CHEEKY_QUINZE, 0,

	// P U C H I T O C A D E S
	0, 0, 0, 0, 0, CH_BENTOC,
	CH_BENTOC, CH_BENTOC, CH_BENTOC, CH_BENTOC, CH_BENTOC, HOUR_TOCADES,
};



