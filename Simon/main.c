#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void initIO(void) {
    DDRB |= (1 << 0);
    PORTB |= (1 << 0);
}

int main(void) {
	initIO();

	while (1) {
	}
	return 0; // never reached
}
