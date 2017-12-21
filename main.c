#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdbool.h>

#define PIXELS 90

//                543210
#define HIBIT 0b00001000
#define LOBIT 0b00000000

#define DIST 3

/* Timings:
 *
 * Symbol	Min	Typical	Max		Units	Parameter
 * T0H		150	300	450		ns	0 code,		high level time
 * T0L		750	900	1050		ns	0 code,		low  level time
 * T1H		450	600	750		ns	1 code,		high level time
 * T1L		450	600	750		ns	1 code,		low  level time
 * RES		38	80	infinity	us	Reset code,	low  level time
 */

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

const uint8_t	ir_bnum		= 44;

bool 		ir_data		= false;
uint8_t  	ir_pos		= 0;
uint32_t 	ir_reading	= 0;
uint32_t	ir_code		= 0;
uint8_t		ir_repeating	= 0;

uint8_t		led_brightness	= 255;

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

void writeRGBW(uint8_t r, uint8_t g, int8_t b, uint8_t w, uint8_t brightness) {
	writeByte(g, brightness);
	writeByte(r, brightness);
	writeByte(b, brightness);
	writeByte(w, brightness);
}

void writeRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
	writeRGBW(r, g, b, 0, brightness);
}

void writeHue(uint8_t hue, uint8_t brightness) {
	if(hue < 85) {
		writeRGBW(255 - hue * 3, 0, hue * 3, 0, brightness);
	} else if(hue < 170) {
		hue -= 85;
		writeRGBW(0, hue * 3, 255 - hue * 3, 0, brightness);
	} else {
		hue -= 170;
		writeRGBW(hue * 3, 255 - hue * 3, 0, 0, brightness);
	}
}

void execute(uint8_t code) {
	switch(code) {
		case 1:
			if (led_brightness < 255)
				led_brightness += ir_repeating;
			break;
		case 2:
			if (led_brightness > 0)
				led_brightness -= ir_repeating;
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
	execute(code);
}

int main(void) {
	DDRB   = 0x08;		//Set PB3 as output.
	PORTB  = LOBIT;		//Activate pull resistors.

	GIMSK  = 0x20;		//Enable pin change interrupt.
	PCMSK  = 0x10;		//Enable interrupt on pins.

	TCCR0A = 0;		// Disable timer compare.
	TCCR0B = 5 << CS00;	// Setup   timer prescaler.

	sei();

	while(1) {
		for (uint8_t i = 0; i < 8; i++) {
			writeRGB(255, 0, 0, led_brightness);
		}

		_delay_ms(50);
	}
}

ISR(PCINT0_vect) {
	uint8_t time = TCNT0;		//Read time of last edge
	TCNT0 = 0;			//Reset timer
	bool state = PINB & 0x10;	//Get current interrupt pin state;

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
