/*---------------------------------------------------------------------------------------*/
// The program aims to ajdust the speed of the DC motors according to the value of the
// proximity sensor.
//
// Date of creation: 05/06/2023
// Last updated: 06/06/2023
/*---------------------------------------------------------------------------------------*/

// PIC16F886 Configuration Bit Settings
// 'C' source line config statements
// CONFIG1
#pragma config FOSC = EXTRC_CLKOUT
#pragma config WDTE = ON
#pragma config PWRTE = OFF
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config CPD = OFF
#pragma config BOREN = ON
#pragma config IESO = ON
#pragma config FCMEN = ON
#pragma config LVP = ON

// CONFIG2
#pragma config BOR4V = BOR40V  // Brown-out Reset Selection bit (Brown-out Reset set to 4.0V)
#pragma config WRT = OFF       // Flash Program Memory Self Write Enable bits (Write protection off)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// Includes
#include <stdio.h>  // para poder usar sprintf()
#include <xc.h>

#include "./libraries/always.h"   // Useful structures and unions
#include "./libraries/delay.h"    // Several delays
#include "./libraries/lcd8x2.h"   // LCD for the robot
#include "./libraries/led_rgb.h"  // Robot's RGB LED
#include "./libraries/sensor.h"   // Line sensors, proximity sensors, and buzzer
#include "./libraries/serial.h"   // To use the serial communication channel
#include "./libraries/spi.h"      // SPI interface

// Definitions
#define VERSION "1.0"

// Outputs
#define LED RB5     // bit de sa� da para o LED
#define BUZZER RB7  // bit para buzzer

__EEPROM_DATA(0, 1, -1, 0, -1, 0, 0, 1);
__EEPROM_DATA(1, 0, 0, -1, 0, -1, 1, 0);

volatile int16_t counter1 = 0;
volatile int16_t counter2 = 0;

// Functions declarations
void pwm_init(void);
void pwm_set(int channel, int duty_cycle);
void t0_init(void);
void led_init(void);
void buzzer_init(void);
void beep(void);
void welcome_message(void);

volatile int last_state1 = 0;
volatile int last_state2 = 0;

void __interrupt() isr(void) {
    static int tick = 0;  // Counter of times Timer 0 interrupts
                          // Timer 0
                          // Interrupts approximately every 5 ms.

    if (TMR0IE && TMR0IF) {
        if (++tick >= 20) {  // 5 ms * 20 = 100 ms
            tick = 0;
        }

        TMR0 = 0xff - 98;
        TMR0IF = 0;
    }

    // IOC PORTB
    if (RBIE && RBIF) {
        char portB = PORTB;  // Reads PORTB, which also resets it

        // Gets the current state
        int state1 = (portB >> 3) & 0b11;
        int state2 = (portB >> 1) & 0b11;

        // Updates the counter values
        counter1 += (signed char)EEPROM_READ(state1 + 4 * last_state1);
        counter2 += (-1) * ((signed char)EEPROM_READ(state2 + 4 * last_state2));

        // Updates the last state
        last_state1 = state1;
        last_state2 = state2;

        RBIF = 0;  // Resets the interrupt flag
    }
}  // isr()


void main(void) {
    spi_init();      // initialize SPI for LCD, LED RGB, battery, compass
    led_rgb_init();  // initialize RGB LED
    sensor_init();
    lcd_init();     // initialize LCD
    sensor_init();  // initialize sensors

    // Local board initializations
    t0_init();      // initialize Timer 0 for periodic interruption (~5 ms)
    led_init();     // initialize LED for debugging
    buzzer_init();  // initialize buzzer
    pwm_init();

    // Initialization of PORTB
    IOCB = 0b00011110;  // enable IOC on encoder ports
    RBIE = 1;           // enable Port B interruption

    // Configuration of digital ports
    // enable input
    TRISB1 = 1;
    TRISB2 = 1;
    TRISB3 = 1;
    TRISB4 = 1;

    // configure as digital input
    ANS10 = 0;
    ANS8 = 0;
    ANS9 = 0;
    ANS11 = 0;

    GIE = 1;

    delay_s(2);
    lcd_clear();
    lcd_show_cursor(OFF);
    welcome_message();  // write the welcome message
    beep();             // sound signal
    lcd_clear();        // clear the LCD for the next instructions

    char str[9];
    int diff_count1 = 0;
    int diff_count2 = 0;
    int16_t last1 = 0;
    int16_t last2 = 0;
    int spd1 = 0;
    int spd2 = 0;
    int spd;
    char text[9];  // auxiliary string for 8 characters
    int AD_data, est;
    int pwm_max = 60;

    while (1) {
        // Counting encoder pulses
        // Display the count values on the LCD
        pwm_set(1, 600);
        pwm_set(2, 600);

        sprintf(str, "r1: %4d", counter1);
        lcd_goto(0);
        lcd_puts(str);
        delay_ms(1);
        sprintf(str, "r2: %4d", counter2);
        lcd_goto(64);
        lcd_puts(str);

        // Calculation of wheel speeds
        if (flag) {  // check if the interrupt flag is activated

            // calculate differences
            diff_count1 = counter1 - last1;
            spd1 = (diff_count1 / 48) * 6.28 * 21 / 0.1;
            diff_count2 = counter2 - last2;
            spd2 = (diff_count2 / 48) * 6.28 * 21 / 0.1;

            // update stored values
            last1 = counter1;
            last2 = counter2;

            sprintf(str, "r1: %d mm/s", spd1);
            lcd_goto(0);
            lcd_puts(str);
            sprintf(str, "r2: %d mm/s", spd2);
            lcd_goto(64);
            lcd_puts(str);

            flag = 0;  // reset the flag
        }

        // Routine to avoid obstacles
        AD_data = sensorNear_read();  // read the value of the proximity sensor
        est = ((2914 / (AD_data + 5)) - 1) * 10;
        sprintf(text, "%04d mm", est);  // create a string with the value

        est /= 10;

        if (est <= 20)  // if the cart is 20 cm or less from an obstacle
        {
            // the cart's speed decreases linearly to zero as it approaches an obstacle
            spd = (est - 4) / est * pwm_max;
            pwm_set(1, spd);
            pwm_set(2, spd);
        } else {                  // if the cart is more than 20 cm away from the obstacle
            pwm_set(1, pwm_max);  // speed is maximum
        }

        // display distance reading
        lcd_goto(0);
        lcd_puts(text);

        // display wheel speeds
        sprintf(str, "v1:%d v2:%d", spd1, spd2);
        lcd_goto(64);
        lcd_puts(text);
    }  // while
}  // main

void pwm_init(void) {
    TRISC2 = 1;  // Disable output (CCP1 - enhanced)
    TRISC1 = 1;  // Disable output (CCP2)

    // Calculation of the desired PWM period
    // Fosc = 20 MHz, Fpwm = 19.53 kHz, TMR2 pre-scaler -> 4
    PR2 = 255;

    // Configuration of PWM output

    // CCP1
    CCP1CONbits.CCP1M = 0b1100;
    CCP1CONbits.P1M = 0;  // Single output

    // CCP2 - does not have advanced functions, so CCP2M does not exist
    CCP2CONbits.CCP2M = 0b1100;

    // CCP1
    CCPR1L = 0;

    // Start PWM with duty cycle 0
    CCP1CONbits.DC1B = 0;

    // CCP2
    CCPR2L = 0;

    // TMR2 setup
    TMR2IF = 0;            // Clear TMR2 flag
    T2CONbits.T2CKPS = 0;  // Configure pre-scaler to 1:1
    TMR2ON = 1;            // Turn on TMR2

    while (!TMR2IF)
        ;  // Wait for TMR2 overflow to start PWM cycle

    TRISC2 = 0;  // Enable output (CCP1 - enhanced)
    TRISC1 = 0;  // Enable output (CCP2)
}


void pwm_set(int channel, int duty_cycle) {
    if (channel == 1) {            // CCP1
        CCPR1L = duty_cycle >> 2;  // Shift the value to select only the
                                   // 8 most significant bits
    }

    if (channel == 2) {            // CCP2
        CCPR2L = duty_cycle >> 2;  // Shift the value to select only the
                                   // 8 most significant bits
    }
}


void t0_init(void) {
    // Timer 0 is used for periodic interruption approximately every 5 ms
    OPTION_REGbits.T0CS = 0;  // use internal clock FOSC/4
    OPTION_REGbits.PSA = 0;   // Prescaler is for Timer 0, not for WDT
    OPTION_REGbits.PS = 7;    // set Prescaler to 1:256
    TMR0 = 0xff - 98;         // initial value of Timer 0 for 5.0176 ms
    TMR0IE = 1;               // enable Timer 0 interruption
}

void led_init(void) {
    TRISB5 = 0;
    ANS13 = 0;
    LED = 0;
}

void buzzer_init(void) {
    TRISB7 = 0;
    BUZZER = 0;
}

void beep(void) {
    BUZZER = ON;
    delay_big_ms(200);
    BUZZER = OFF;
}

void welcome_message(void) {
    lcd_goto(0);
    lcd_puts("AT06");
    lcd_goto(64);
    lcd_puts("T1-G5");
    delay_s(2);
}