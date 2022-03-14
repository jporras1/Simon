#include "simon.h"

const uint8_t buttons[4] = {
    0b00001010, 0b00000110, 0b00000011, 0b00010010
};
const uint8_t tones[4] = {
    239, 179, 143, 119
};
uint8_t lastKey;
uint8_t level = 0;
uint8_t maxLevel;
uint16_t randomSeed;
uint16_t seed;
volatile uint8_t nrot = 8;
volatile uint16_t time;

void sleepNow(void);
void playNote(uint8_t i, uint16_t t);
void gameOver(void);
void levelUpSound(void);
uint8_t simpleRandom4(void);
void resetRandomSeed(void);


void setup(void) {
    PORTB = 0b00011101; // enable pull-up resistors on 4 game buttons
    
    ADCSRA |= (1 << ADEN); // enable ADC
    ADCSRA |= (1 << ADSC); // start the conversion on unconnected ADC0 (ADMUX = 0b00000000 by default)
    // ADCSRA = (1 << ADEN) | (1 << ADSC); // enable ADC and start the conversion on unconnected ADC0 (ADMUX = 0b00000000 by default)
    while (ADCSRA & (1 << ADSC)); // ADSC is cleared when the conversion finishes
    seed = ADCL; // set seed to lower ADC byte
    ADCSRA = 0b00000000; // turn off ADC
    
    WDTCR = (1 << WDTIE); // start watchdog timer with 16ms prescaller (interrupt mode)
    sei(); // global interrupt enable
    TCCR0B = (1 << CS00); // Timer0 in normal mode (no prescaler)
    
    while (nrot); // repeat for fist 8 WDT interrupts to shuffle the seed
    
    TCCR0A = (1 << COM0B1) | (0 << COM0B0) | (0 << WGM01)  | (1 << WGM00); // set Timer0 to phase correct PWM
    
    maxLevel = ~eeprom_read_byte((uint8_t*) 0); // read best score from eeprom
    
    switch (PINB & 0b00011101) {
        case 0b00010101: // red button is pressed during reset
            eeprom_write_byte((uint8_t*) 0, 255); // reset best score
            maxLevel = 0;
            break;
        case 0b00001101: // green button is pressed during reset
            level = 255; // play random tones in an infinite loop
            break;
        case 0b00011001: // orange button is pressed during reset
            level = maxLevel; // start from max level and load seed from eeprom (no break here)
        case 0b00011100: // yellow button is pressed during reset
            seed = (((uint16_t) eeprom_read_byte((uint8_t*) 1)) << 8) | eeprom_read_byte((uint8_t*) 2);  // load seed from eeprom but start from first level
            break;
    }
}

int main(void) {
    setup();
    
    while (1) {
        //Play the sequence
        resetRandomSeed();
        for (uint8_t count = 0; count <= level; count++) { // never ends if level == 255
            //TODO: Fix this, it's where the program keeps breaking.
            _delay_loop_2(4400 + 29088 / (8 + level));
            //                _delay_loop_2(4400 + 489088 / (8 + level));
            playNote(simpleRandom4(), 0);
        }
        
        //Get the input from user.
        time = 0;
        lastKey = 5;
        resetRandomSeed();
        for (uint8_t count = 0; count <= level; count++) {
            char pressed = false;
            while (!pressed) {
                for (uint8_t buttonPressed = 0; buttonPressed < 4; buttonPressed++) {
                    if (!(PINB & buttons[buttonPressed] & 0b00011101)) {
                        if (time > 1 || buttonPressed != lastKey) {
                            playNote(buttonPressed, 0);
                            pressed = true;
                            uint8_t correct = simpleRandom4();
                            if (buttonPressed != correct) {
                                for (uint8_t j = 0; j < 3; j++) {
                                    _delay_loop_2(10000);
                                    playNote(correct, 20000);
                                }
                                _delay_loop_2(65535); //was originally 65536
                                gameOver();
                            }
                            time = 0;
                            lastKey = buttonPressed;
                            break;
                        }
                        time = 0;
                    }
                }
                if (time > 4000) {
                    sleepNow();
                }
            }
        }
        _delay_loop_2(65535); //old was 65536
        if (level < 254) {
            level++;
            levelUpSound(); // animation for completed level
            _delay_loop_2(45000);
        }
        else { // special animation for highest allowable (255th) level
            levelUpSound();
            gameOver(); // then turn off
        }
    }
    return 0; // never reached
}

void sleepNow(void){
    PORTB = 0b00000000; // disable all pull-up resistors
    cli(); // disable all interrupts
    WDTCR = 0; // turn off the Watchdog timer
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
}

void playNote(uint8_t index, uint16_t time) {
    if (time == 0) {
        time = 45000;
    }
    PORTB = 0b00000000;  // set all button pins low or disable pull-up resistors
    DDRB = buttons[index]; // set speaker and #i button pin as output
    OCR0A = tones[index];
    OCR0B = tones[index] >> 1;
    TCCR0B = (1 << WGM02) | (1 << CS01); // prescaler /8
    _delay_loop_2(time);
    TCCR0B = 0b00000000; // no clock source (Timer0 stopped)
    DDRB = 0b00000000;
    PORTB = 0b00011101;
}

void gameOver(void) {
    for (uint8_t index = 0; index < 4; index++) {
        playNote(3 - index, 25000);
    }
    if (level > maxLevel) {
        eeprom_write_byte((uint8_t*) 0, ~level); // write best score
        eeprom_write_byte((uint8_t*) 1, (seed >> 8)); // write high byte of seed
        eeprom_write_byte((uint8_t*) 2, (seed & 0b11111111)); // write low byte of seed
        
        for (uint8_t i = 0; i < 3; i++) { // play best score melody
            levelUpSound();
        }
    }
    sleepNow();
}

void levelUpSound(void) {
    for (uint8_t index = 0; index < 4; index++) {
        playNote(index, 25000);
    }
}

uint8_t simpleRandom4(void) {
    randomSeed = 2053 * randomSeed + 13849;
    uint8_t temp = randomSeed ^ (randomSeed >> 8); // XOR two bytes
    temp ^= (temp >> 4); // XOR two nibbles
    return (temp ^ (temp >> 2)) & 0b00000011; // XOR two pairs of bits and return remainder after division by 4
}

void resetRandomSeed(void) {
    randomSeed = seed;
}

ISR(WDT_vect) {
time++; // increase each 16 ms
if (nrot) { // random seed generation
    nrot--;
    seed = (seed << 1) ^ TCNT0;
}
}
