#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

//TM1637 Pins
#define CLK_PIN     BIT6
#define DIO_PIN     BIT7
#define CLK_DIR     P1DIR
#define DIO_DIR     P1DIR
#define CLK_OUT     P1OUT
#define DIO_OUT     P1OUT
#define DIO_IN      P1IN

//Heating and Cooling Pins
#define HEATER_PIN    BIT0
#define FAN_PIN       BIT1
#define TARGET_TEMP   30.0f
#define HYSTERESIS    2.0f

typedef enum { HEATING_STATE, COOLING_STATE } SystemState;
static SystemState current_state;

void setup() {
    WDTCTL = WDTPW | WDTHOLD;
    P1DIR |= HEATER_PIN | FAN_PIN;
    ADC10CTL1 = INCH_4   | ADC10SSEL_3;   // A4, SMCLK
    ADC10CTL0 = ADC10SHT_3 | ADC10ON;     // 64CLK sample, ADC on
    ADC10AE0 |= BIT4;                     // P1.4 analog
    ADC10CTL0 |= ENC;

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
    CLK_DIR |= CLK_PIN;
    DIO_DIR |= DIO_PIN;

    // Start state: Both Pins High
    CLK_OUT |= CLK_PIN;
    DIO_OUT |= DIO_PIN;
}

float read_temp() {
    static float buf[5];
    static unsigned char idx = 0;
    float s[5], t;
    unsigned char i, j;

    ADC10CTL0 |= ADC10SC;
    while (ADC10CTL0 & ADC10BUSY);

    buf[idx] = ((float)ADC10MEM * 3.53f / 1023.0f) / 0.01f;
    idx = (idx + 1) % 5;
    for (i = 0; i < 5; i++) s[i] = buf[i];
    for (i = 0; i < 4; i++) {
        for (j = i+1; j < 5; j++) {
            if (s[i] > s[j]) {
                t = s[i]; s[i] = s[j]; s[j] = t;
            }
        }
    }
    return s[2];
}

void apply_state(SystemState st) {
    if (st == HEATING_STATE) {
        P1OUT |=  HEATER_PIN;
        P1OUT &= ~FAN_PIN;
    } else {
        P1OUT |=  FAN_PIN;
        P1OUT &= ~HEATER_PIN;
    }
}

int control_system() {
    float temp = read_temp();

    if (current_state == HEATING_STATE) {
        if (temp >= TARGET_TEMP + HYSTERESIS) {
            current_state = COOLING_STATE;
            apply_state(current_state);
        }
    } else {
        if (temp <= TARGET_TEMP - HYSTERESIS ) {
            current_state = HEATING_STATE;
            apply_state(current_state);
        }
    }

    return (int)temp;
}

void TM1637_delay() {
    __delay_cycles(100); //100us
}

void TM1637_setDIOInput() {
    DIO_DIR &= ~DIO_PIN;
}


void TM1637_setDIOOutput() {
    DIO_DIR |= DIO_PIN;
}


void TM1637_start() {
    TM1637_setDIOOutput();
    CLK_OUT |= CLK_PIN;
    DIO_OUT |= DIO_PIN;
    TM1637_delay();
    DIO_OUT &= ~DIO_PIN;
    TM1637_delay();
    CLK_OUT &= ~CLK_PIN;
}


void TM1637_stop() {
    TM1637_setDIOOutput();
    CLK_OUT &= ~CLK_PIN;
    DIO_OUT &= ~DIO_PIN;
    TM1637_delay();
    CLK_OUT |= CLK_PIN;
    TM1637_delay();
    DIO_OUT |= DIO_PIN;
}


void TM1637_writeByte(uint8_t b) {
    TM1637_setDIOOutput();
    int i;
    for ( i = 0; i < 8; i++) {
        CLK_OUT &= ~CLK_PIN;
        if (b & 0x01) {
            DIO_OUT |= DIO_PIN;
        } else {
            DIO_OUT &= ~DIO_PIN;
        }
        TM1637_delay();
        CLK_OUT |= CLK_PIN;
        TM1637_delay();
        b >>= 1;
    }


    CLK_OUT &= ~CLK_PIN;
    TM1637_setDIOInput();
    TM1637_delay();
    CLK_OUT |= CLK_PIN;
    TM1637_delay();

    CLK_OUT &= ~CLK_PIN;
    TM1637_setDIOOutput();
}


void TM1637_sendCommand(uint8_t cmd) {
    TM1637_start();
    TM1637_writeByte(cmd);
    TM1637_stop();
}


void TM1637_displayByte(uint8_t addr, uint8_t data) {
    TM1637_start();
    TM1637_writeByte(0xC0 | addr);
    TM1637_writeByte(data);
    TM1637_stop();
}


const uint8_t segData[] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};


void TM1637_displayNumber(int num) {
    TM1637_sendCommand(0x40);

    TM1637_start();
    TM1637_writeByte(0xC0);
    TM1637_writeByte(segData[(num / 1000) % 10]);
    TM1637_writeByte(segData[(num / 100) % 10]);
    TM1637_writeByte(segData[(num / 10) % 10]);
    TM1637_writeByte(segData[num % 10]);
    TM1637_stop();

    TM1637_sendCommand(0x88 | 0x07); // brightness = max (0x88 to 0x8F)
}

void main(void) {
   setup();
   int i;
    for (i = 0; i < 5; i++) {
        read_temp();
        __delay_cycles(800000); //8MHZ timer için 0.1 sn
    }

    // Başlangıç state’ini ayarla
    if (read_temp() >= TARGET_TEMP)
        current_state = COOLING_STATE;
    else
        current_state = HEATING_STATE;
    apply_state(current_state);

    while (1) {
        int temperature = control_system();
        TM1637_displayNumber(temperature);
        __delay_cycles(1000000); //wait for 1 sec.
    }
}
