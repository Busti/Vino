#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define PIXELS 90

#define HIBIT 0x1C
#define LOBIT 0x14

#define DIST 3

/* Timings:
 *
 * Symbol	Min	Typical	Max		Units	Parameter
 * T0H		150	300	450		ns	0 code, high level time
 * T0L		750	900	1050		ns	0 code,  low level time
 * T1H		450	600	750		ns	1 code, high level time
 * T1L		450	600	750		ns	1 code, low  level time
 * RES		38	80	infinity	us	Reset code, low level time
 */

void writeByte(uint8_t byte) {
    for(uint8_t bit = 0; bit < 8; bit++) {
        PORTB = HIBIT;
        asm("nop\n\t");
        if (byte & 0x80) {
            asm("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t");
        }
        PORTB = LOBIT;
        byte <<= 1;
        asm("nop\n\tnop\n\tnop\n\tnop\n\t");
    }
}

void writeRGBW(uint8_t r, uint8_t g, int8_t b, uint8_t w) {
    writeByte(g);
    writeByte(r);
    writeByte(b);
    writeByte(w);
}

void writeHue(uint8_t hue) {
    if(hue < 85) {
        writeRGBW(255 - hue * 3, 0, hue * 3, 0);
    } else if(hue < 170) {
        hue -= 85;
        writeRGBW(0, hue * 3, 255 - hue * 3, 0);
    } else {
        hue -= 170;
        writeRGBW(hue * 3, 255 - hue * 3, 0, 0);
    }
}

int main(void) {
    DDRB = 0x08; //Set PB3 as output.
    PORTB = LOBIT; //Activate pull-up resistors.
    uint8_t lights[] = {
        0, 0, 100, 185, 185, 220, 242
    };
    uint8_t numl = sizeof(lights);
    while(1) {
        while(~PINB & 0x04) {
            if (~PINB & 0x10) {
                uint8_t colors[PIXELS / DIST];
                uint8_t prev = 0;
                for(uint8_t i = 0; i < PIXELS / DIST; i++) {
                    uint8_t color = 0;
                    do {
                        uint8_t random = rand();
                        color = lights[random % numl];
                    } while(color == prev);
                    prev = color;
                    colors[i] = color;
                }
                while ((~PINB & 0x04) && (~PINB & 0x10)) {
                    for(uint8_t i = 0; i < 12; i++) {
                        writeRGBW(0, 0, 0, 0);
                    }
                    for(uint8_t i = 0; i < 90; i++) {
                        if(!(i % DIST)) {
                            writeHue(colors[i % PIXELS / DIST]);
                        } else {
                            writeRGBW(0, 0, 0, 0);
                        }
                    }
                    _delay_ms(50);
                }
            }
            for(uint8_t i = 0; i < PIXELS + 12; i++) {
                writeRGBW(0, 0 , 0, 0);
            }
            _delay_ms(1);
        }
        for(uint8_t i = 0; i < 15; i++) {
           for(uint8_t j = 0; j < 12; j++) {
                if(j < i) {
                    writeRGBW(255, 80, 0, 120);
                } else {
                    writeRGBW(0, 0, 0, 0);
                }
            }
            for(uint8_t j = 0; j < 6; j++) {
                for(uint8_t k = 0; k < 15; k++) {
                   if (i > k) {
                       writeRGBW(255, 80, 0, 120);
                   } else {
                       writeRGBW(0, 0, 0, 0);
                   }
                }
            }
        _delay_ms(50);
        }
        while(PINB & 0x04) {
            for(uint8_t i = 0; i < 102; i++) {
                 writeRGBW(255, 80, 0, 120);
            }
            _delay_ms(10);
        }
        for(uint8_t i = 0; i < 15; i++) {
            for(uint8_t j = 0; j < 12; j++) {
                if(j > i) {
                    writeRGBW(255, 80, 0, 120);
                } else {
                    writeRGBW(0, 0, 0, 0);
                }
            }
            for(uint8_t j = 0; j < 6; j++) {
                for(uint8_t k = 0; k < 15; k++) {
                   if (i < k) {
                       writeRGBW(255, 80, 0, 120);
                   } else {
                       writeRGBW(0, 0, 0, 0);
                   }
                }
            }
        _delay_ms(50);
        }
    }
}
