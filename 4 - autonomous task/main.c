/*---------------------------------------------------------------------------------------*/
// The program aims to use the digital line sensors and analog proximity sensors
// for the execution of an autonomous robotic task.
//
// Date of creation: 19/06/2023
// Last updated: 20/06/2023
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
#include <stdio.h>  // For sprintf() usage
#include <xc.h>

#include "./libraries/always.h"   // Useful structures and unions
#include "./libraries/delay.h"    // Several delays
#include "./libraries/key.h"      // To use the board's switch
#include "./libraries/lcd8x2.h"   // LCD for the robot
#include "./libraries/led_rgb.h"  // Robot's RGB LED
#include "./libraries/pwm.h"      // PWM for tests
#include "./libraries/sensor.h"   // Line sensors, proximity sensors, and buzzer
#include "./libraries/serial.h"   // To use the serial communication channel
#include "./libraries/spi.h"      // SPI interface

// Definitions
#define VERSION "1.0"

// Outputs
#define LED RB5     // Output bit for the LED
#define BUZZER RB7  // Bit for the buzzer

void __interrupt() isr(void) {
    // Local variables declared static retain their values
    static int tick = 0;  // Timer 0 interruption counter

    // Timer 0
    // Interrupts every approximately 5 ms.
    // Blinks the LED every approximately 1 second.
    // Controls the debounce time of the switch in conjunction with the I-O-C of PORT B
    if (TMR0IE && TMR0IF) {  // if it's a Timer 0 interruption

        // Toggle the LED every second
        if (++tick >= 200) {  // if it interrupts 200 times, it's ~1 s
            tick = 0;         // reset the counter
        }                     // end - counting how many times the interruption occurs

        // Switch debounce. It should be included in the periodic Timer interruption.
        // It takes 2 cycles to debounce between 5 and 10 ms for an interruption of ~5ms.
        // It should be changed according to the time given by the Timer interruption.
        // For example, if the interruption is every 1 ms, the parameter should be 10
        // resulting in a debounce of 9 to 10 ms.
        key_debounce(2);  // 2 is the number of cycles to give 5 to 10 ms

        TMR0 = 0xff - 98;  // reload Timer 0 count for 5.0176ms
        TMR0IF = 0;        // clear interruption flag
    }                      // end - Timer 0 handling

    // Interrupt-on-change of PORT B used by the switch
    // The debounce time is controlled by Timer 0. The change of state of the switch
    // triggers a time count by Timer 0 from 5 to 10 ms. Every time
    // there is a change of state of the switch, the debounce time count is
    // reset.
    if (RBIE && RBIF) {      // if it's a change of state in Port B
        char portB = PORTB;  // read Port B, this resets the interruption condition along with RBIF = 0
        key_read(portB);     // read the switch
        RBIF = 0;            // reset the interruption flag to be able to receive another interruption
    }                        // end - I-O-C PORT B treatment
}  // end - Handling all interruptions


// Initialize Timer 0
void t0_init(void) {
    // Timer 0 is used for periodic interruption approximately every 5 ms.
    OPTION_REGbits.T0CS = 0;  // use internal clock FOSC/4
    OPTION_REGbits.PSA = 0;   // Prescaler is for Timer 0, not for WDT
    OPTION_REGbits.PS = 7;    // set Prescaler to 1:256
    TMR0 = 0xff - 98;         // initial value of Timer 0 for 5.0176 ms
    TMR0IE = 1;               // enable Timer 0 interruption
}

// Initialize LED
void led_init(void) {
    TRISB5 = 0;  // RB5 is an output for LED
    ANS13 = 0;   // RB5/AN13 is digital
    LED = 0;
}

// Initialize Buzzer
void buzzer_init(void) {
    TRISB7 = 0;  // RB7 is an output for BUZZER
    BUZZER = 0;
}

// Play a beep
// To use the BUZZER, the SENSOR POWER must be on.
void beep() {
    BUZZER = ON;        // turn on buzzer
    delay_big_ms(200);  // for 0.2s
    BUZZER = OFF;       // turn off buzzer
}

// Display the initial message on the LCD
void welcome_message(void) {
    lcd_goto(0);
    lcd_puts("AT06");
    lcd_goto(64);
    lcd_puts("T1-G5");
    delay_s(2);
}


void print_lcd(char dir) {
    lcd_goto(0);
    lcd_puts(dir);
}

void main(void) {
    spi_init();      // initialize SPI for LCD, LED RGB, battery, compass
    led_rgb_init();  // initialize RGB LED
    sensor_init();   // initialize sensors
    lcd_init();      // initialize LCD
    sensor_init();   // initialize sensors (Note: sensor_init() is called twice)

    // local board initializations
    t0_init();      // initialize Timer 0 for periodic interruption of ~5 ms
    led_init();     // initialize LED for debugging
    buzzer_init();  // initialize buzzer
    pwm_init();     // initialize PWM
    key_init();     // initialize key (switch)

    GIE = 1;  // enable global interruptions

    delay_s(2);
    lcd_clear();
    lcd_show_cursor(OFF);
    welcome_message();  // display welcome message on LCD
    beep();             // play a beep
    lcd_clear();

    sensor_power(ON);  // turn on sensor power

    int duty_max = 550;
    int duty_cycle;
    int sensor_linha, sensor_distance;
    char keyIn = FALSE;  // key pressed, TRUE = yes
    int isOn = FALSE;    // robot not activated yet
    char sVar[9];        // string variable


    while (1) {
        if (isOn == TRUE) {  // when the robot is turned on

            sensor_linha = sensorLine_read();  // read the line sensor
            sensor_distance = sensorNear_read();

            if (sensor_distance >= 500) {
                duty_cycle = 0;
                led_rgb_set_color(RED);
            } else {
                duty_cycle = duty_max - sensor_distance;
            }

            switch (sensor_linha) {
            case 2:
            case 7:
                pwm_set(1, duty_cycle);
                pwm_set(2, duty_cycle);    // move forward
                led_rgb_set_color(GREEN);  // green LED
                //                    print_lcd('f');
                break;
            case 6:
            case 4:
                pwm_set(1, duty_cycle);
                pwm_set(2, 5 * duty_cycle / 10);  // turn left
                led_rgb_set_color(BLUE);
                //                    print_lcd('e');
                break;
            case 3:
            case 1:
                pwm_set(1, 5 * duty_cycle / 10);
                pwm_set(2, duty_cycle);  // turn right
                led_rgb_set_color(MAGENTA);
                //                    print_lcd('d');
                break;
            default:
                pwm_set(1, 5 * duty_cycle / 10);
                pwm_set(2, duty_cycle);  // circular movement to the right
                LED = ~LED;              // blink LED while not finding the line
                led_rgb_set_color(BLACK);
                //                    print_lcd('E');
                break;
            }
        }

        keyIn = key_pressed();
        if (keyIn) {       // when the button is pressed
            isOn = !isOn;  // invert the current state
            pwm_set(1, 0);
            pwm_set(2, 0);  // circular movement to the right

            sprintf(sVar, "%d", isOn);
            lcd_goto(0);
            lcd_puts(sVar);
            delay_ms(150);
            lcd_goto(0);
            lcd_puts("     ");
        }

    }  // end - while
}  // end - main
