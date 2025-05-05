#include <msp430.h>

#define HEATER_PIN    BIT0    // P1.0
#define FAN_PIN       BIT1    // P1.1
#define TARGET_TEMP   28.0f   // Hem ısıtma/soğutma eşik değeri
#define HYSTERESIS    2.0f
typedef enum { HEATING_STATE, COOLING_STATE } SystemState;
static SystemState current_state;

// Donanımı bir kere başlat
void setup() {
    WDTCTL = WDTPW | WDTHOLD;
    P1DIR |= HEATER_PIN | FAN_PIN;
    ADC10CTL1 = INCH_4   | ADC10SSEL_3;   // A4, SMCLK
    ADC10CTL0 = ADC10SHT_3 | ADC10ON;     // 64CLK sample, ADC on
    ADC10AE0 |= BIT4;                     // P1.4 analog
    ADC10CTL0 |= ENC;
}

// Son 5 ölçümün medyanını döndürür
float read_temp() {
    static float buf[5];
    static unsigned char idx = 0;
    float s[5], t;
    unsigned char i, j;

    ADC10CTL0 |= ADC10SC;
    while (ADC10CTL0 & ADC10BUSY);

    buf[idx] = ((float)ADC10MEM * 3.53f / 1023.0f) / 0.01f;
    idx = (idx + 1) % 5;

    // Küçükten büyüğe sıralayıp ortadaki(2.) elemanı al
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

// Mevcut state'e göre pinleri kesin olarak ayarlar
void apply_state(SystemState st) {
    if (st == HEATING_STATE) {
        P1OUT |=  HEATER_PIN;
        P1OUT &= ~FAN_PIN;
    } else {
        P1OUT |=  FAN_PIN;
        P1OUT &= ~HEATER_PIN;
    }
}

// Sıcaklığa göre anında geçiş
void control_system() {
    float temp = read_temp();
    //feature branch değişiklikleri
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
}

int main() {
    unsigned char i;

    setup();

    // Filtre dolsun diye birkaç ölçüm alıyoruz
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

    // Ana döngü
    while (1) {
        control_system();
        __delay_cycles(8000000);
    }
}
