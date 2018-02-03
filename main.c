#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdbool.h>

#define PIXELS 90

//                543210
#define HIBIT 0b00010000
#define LOBIT 0b00000000

#define DIST 3

/* Timings:
 *
 * Symbol	Min	Typical	Max		Units	Parameter
 * T0H		150	300	450		ns	0 code,		high level time
 * T0L		750	900	1050		ns	0 code,		low  level time
 * T1H		450	600	750		ns	1 code,		high level time
 * T1L		450	600	750		ns	1 code,		low  level time
 * RES		38	80	infinity	µs	Reset code,	low  level time
 */

typedef struct RGBW {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t w;
} RGBW;

const uint16_t ir_codes[] = {
//	brighter	darker		play / pause	on / off
	0x3AC5,		0xBA45,		0x827D,		0x02FD,
//	red		green		blue		white
	0x1AE5,		0x9A65,		0xA25D,		0x22DD,
//	orange		lime		mediumblue	rosa
	0x2AD5,		0xAA55,		0x926D,		0x12ED,
//	orangered	lightblue	violet		rosa
	0x0AF5,		0x8A75,		0xB24D,		0x32CD,
//	lightorange	cyan		vine		babyblue
	0x38C7,		0xB847,		0x7887,		0xF807,
//	yellow		turquoise	pink		babyblue
	0x18E7,		0x9867,		0x58A7,		0xD827,
//	up-red		up-green	up-blue		quick
	0x28D7,		0xA857,		0x6897,		0xE817,
//	down-red	down-green	down-blue	slow
	0x08F7,		0x8877,		0x48B7,		0xC837,
//	diy1		diy2		diy3		auto
	0x30CF,		0xB04F,		0x708F,		0xF00F,
//	diy4		diy5		diy6		flash
	0x10EF,		0x906F,		0x50AF,		0xD02F,
//	jump3		jump7		fade3		fade7
	0x20DF,		0xA05F,		0x609F,		0xE01F
};

const uint32_t led_colors[] = {
//	red		green		blue		white
	0x000000FF,	0x0000FF00,	0x00FF0000,	0xFF000000,
//	orange		lime		mediumblue	rosa
	0x000088FF,	0x0000FF22,	0x00FF4444,	0xFF22AAFF,
//	orangered	lightblue	violet		rosa
	0x000044FF,	0x0088FF22,	0x00FF0044,	0xFF0044FF,
//	lightorange	cyan		wine		babyblue
	0x002288FF,	0x00FFFF00,	0x004400FF,	0xFFBB2288,
//	yellow		turquoise	pink		babyblue
	0x0000CCFF,	0x00FFBB00,	0x00FF00FF,	0xFFFF0000
};

const uint8_t	ir_bnum		= 44;

bool 		ir_data		= false;
uint8_t  	ir_pos		= 0;
uint32_t 	ir_reading	= 0;
uint32_t	ir_code		= 0;
uint8_t		ir_repeating	= 0;

bool		led_running	= true;
uint8_t		led_brightness	= 20;
uint8_t		led_speed	= 128; //todo: implement

uint8_t		led_mode	= 0;
RGBW		led_color	= {0, 0, 0, 0};

uint8_t		led_mode_prev	= 0;
RGBW		led_color_prev	= {0, 0, 0, 0};

uint8_t		led_anim	= 255;

uint8_t sat_add(uint8_t x, uint8_t y) {
	uint8_t res = x + y;
	res |= -(res < x);
    
	return res;
}

uint8_t sat_sub(uint8_t x, uint8_t y) {
	uint8_t res = x - y;
	res &= -(res <= x);
	
	return res;
}

uint8_t lerp_byte(uint8_t a, uint8_t b, uint8_t x) {
	return (a * (255 - x) + b * x) >> 8;
}

RGBW lerp_color(const RGBW* a, const RGBW* b, uint8_t x) {
	uint8_t y = 255 - x;
	RGBW result = {
		(a->r * y + b->r * x) >> 8,
		(a->g * y + b->g * x) >> 8,
		(a->b * y + b->b * x) >> 8,
		(a->w * y + b->r * x) >> 8
	};

	return result;
}

void writeByte(uint8_t byte, uint8_t brightness) {
	cli();
	byte = (byte * brightness) >> 8;
	for(uint8_t bit = 0; bit < 8; bit++) {
		PORTB = HIBIT;
		asm volatile("nop\n");
		if (byte & 0x80) {
			asm volatile(
				"nop\n"
				"nop\n"
				"nop\n"
				"nop\n"
				"nop\n"
				"nop\n"
			);
		}
		PORTB = LOBIT;
		byte <<= 1;
		asm volatile(
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
		);
	}
	sei();
}

//void writeValue(uint8_t value, uint8_t brightness) {
//	writeByte(led_gamma[value], brightness);
//}

void writeRGBW(uint8_t r, uint8_t g, int8_t b, uint8_t w, uint8_t brightness) {
	//r = &led_gamma[r];
	//g = &led_gamma[g];
	//b = &led_gamma[b];
	//w = &led_gamma[w];

	//Write Order for SK6812 is GRBW
	writeByte(g, brightness);
	writeByte(r, brightness);
	writeByte(b, brightness);
	writeByte(w, brightness);
}

void writeRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
	writeRGBW(r, g, b, 0, brightness);
}

void writeColor(const RGBW* color, uint8_t brightness) {
	writeRGBW(
			color->r, 
			color->g,
			color->b,
			color->w,
			brightness
	);
}

//void writeHue(uint8_t hue, uint8_t brightness) {
//	if(hue < 85) {
//		writeRGBW(255 - hue * 3, 0, hue * 3, 0, brightness);
//	} else if(hue < 170) {
//		hue -= 85;
//		writeRGBW(0, hue * 3, 255 - hue * 3, 0, brightness);
//	} else {
//		hue -= 170;
//		writeRGBW(hue * 3, 255 - hue * 3, 0, 0, brightness);
//	}
//}

void setMode(uint8_t mode, uint32_t color) {
	led_color_prev.r = led_color.r;
	led_color_prev.g = led_color.g;
	led_color_prev.b = led_color.b;
	led_color_prev.w = led_color.w;
	led_color.r = color;
	led_color.g = color >> 8;
	led_color.b = color >> 16;
	led_color.w = color >> 24;
	led_mode_prev	= led_mode;
	led_mode	= mode;
	led_anim	= 255;
}

void execute(uint8_t code) {
	if (code > 4 && code < 25)
		setMode(0, led_colors[code - 5]);

	switch (code) {
		case 1:
			led_brightness = sat_add(led_brightness, ir_repeating);
			break;
		case 2:
			led_brightness = sat_sub(led_brightness, ir_repeating);
			break;
		case 3:
			if (led_running) {
				led_running = false;
			} else {
				led_running = true;
			}
			break;
		case 4:
			setMode(0, 0);
			break;
		case 28:
			led_speed = sat_add(led_speed, ir_repeating);
			break;
		case 32:
			led_speed = sat_sub(led_speed, ir_repeating);
		case 44:
			setMode(1, 0);
			break;
	}
}

uint8_t decode(uint16_t value) {
	for (uint8_t i = 0; i < ir_bnum; i++) {
		if (ir_codes[i] == value)
			return i + 1;
	}
	return 0;
}

void onRecieve(uint16_t value) {
	ir_repeating = 0;

	uint8_t code = decode(value);
	execute(code);
}

void onRepeat(uint16_t value) {
	if (ir_repeating < 255)
		ir_repeating++;
	
	uint8_t code = decode(value);

	if (code > 2 && code < 25) //Do not repeat these values.
		return;

	execute(code);
}

RGBW render(uint8_t pos, uint8_t mode, RGBW color) {
	switch (mode) {
		case 0:
			return color;
			break;
		case 1:
			return color;
			break;
	}
	return color;
}

int main(void) {
	DDRB   = 0x10;		//Set PB3 as output.
	PORTB  = LOBIT;		//Activate pull resistors.

	GIMSK  = 0x20;		//Enable pin change interrupt.
	PCMSK  = 0x08;		//Enable interrupt on pins.

	TCCR0A = 0;		// Disable timer compare.
	TCCR0B = 5 << CS00;	// Setup   timer prescaler.

	sei();

	setMode(0, led_colors[0]);

	while(1) {
		RGBW color = render(0, led_mode, led_color);
		//if (led_anim > 0) {
		//	RGBW prev = render(0, led_mode_prev, led_color_prev);
		//	color = lerp_color(&color, &prev, led_anim);
		//}
		//for (uint8_t i = 0; i < 8; i++)
		//	writeRGB(0, 0, (led_anim >> i) & 1 ? 255 : 0, 100);
		for (uint8_t i = 0; i < PIXELS; i++) {
			//RGBW color = render(i, led_mode, led_color);
			//if (led_anim > 0) {
			//	RGBW prev = render(i, led_mode_prev, led_color_prev);
			//	color = lerp_color(&color, &prev, led_anim);
			//}
			writeColor(&color, led_brightness);
		}
		_delay_ms(40);
		//Do non relevant stuff after the lights have been rendered...
		if (led_anim > 0)
			led_anim = sat_sub(led_anim, 5);
	}
}

ISR(PCINT0_vect) {
	uint8_t time = TCNT0;		//Read time of last edge
	TCNT0 = 0;			//Reset timer
	bool state = PINB & 0x08;	//Get current interrupt pin state;

	if (state && time > 128) {
		ir_pos = 0;
		ir_data = false;
	}

	if (ir_pos == 1) {
		if (time > 64) {
			ir_data = true;
			ir_code = 0;
		} else if (time > 32) {
			onRepeat(ir_code);
		}
	}

	if (ir_data && !state) {
		ir_reading <<= 1;
		if (time > 16)
			ir_reading |= 1;
	}

	if (ir_data && ir_pos > 64) {
		ir_code = ir_reading;
		ir_data = false;
		onRecieve(ir_code);
	}

	ir_pos++;
}
