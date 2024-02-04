/*---------------------------------------------------------------------------------------*/
// The program aims for serial communication with another robot.
//
// Date of creation: 22/05/2023
// Last updated: 23/05/2023
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
#include "./libraries/battery.h"  // Robot's battery level measurement
#include "./libraries/compass.h"  // Robot's compass
#include "./libraries/delay.h"    // Several delays
#include "./libraries/key.h"      // To use the board's switch
#include "./libraries/lcd8x2.h"   // LCD for the robot
#include "./libraries/led_rgb.h"  // Robot's RGB LED
#include "./libraries/sensor.h"   // Line sensors, proximity sensors, and buzzer
#include "./libraries/serial.h"   // To use the serial communication channel
#include "./libraries/spi.h"      // SPI interface

// Definitions
#define VERSION "2.3"

// Outputs
#define LED RB5     // Output bit for the LED
#define BUZZER RB7  // Bit for the buzzer

// Macros
#define testbit(var, bit) ((var) & (1 << (bit)))
#define setbit(var, bit) ((var) |= (1 << (bit)))
#define clrbit(var, bit) ((var) &= ~(1 << (bit)))

volatile char current = '0';  // Volatile global variable to store the current character

/*----------------------------------------------------------------------------------------------------------------*/
/* Auxiliary functions */

void __interrupt() isr(void) {
    static int tick = 0;  // Counter of times Timer 0 interrupts

    // Timer 0
    // Interrupts approximately every 5 ms.
    // Controls the debounce time of the key in conjunction with the I-O-C of PORT B

    if (TMR0IE && TMR0IF) {
        if (++tick >= 10) {  // 5 ms * 10  = 50 ms
            // Every 50 ms
            tick = 0;   // Reset the tick for a new count
            current++;  // Update the character

            // We don't want all characters from the ASCII table,
            // only letters and numbers, so
            if (current == ':') {  // The character following "9" in the ASCII table is ":"
                // Therefore, if the character is ":", it means we need
                // to go back to the beginning of the letters (uppercase)
                current = 'A';            // So, we go back to A.
            } else if (current == '[') {  // Analogously, the character following "Z" is "[", so
                current = '0';            // We go to '0', as we have completed the letters (again, uppercase).
            }
        }

        key_debounce(2);

        TMR0 = 0xff - 98;
        TMR0IF = 0;
    }

    if (RBIE && RBIF) {
        char portB = PORTB;
        key_read(portB);
        RBIF = 0;
    }

}  // isr()


/* Initializations */

// Timer 0 Initialization
void t0_init(void) {
    // Timer 0 is used for periodic interruption approximately every 5 ms
    OPTION_REGbits.T0CS = 0;  // Use internal clock FOSC/4
    OPTION_REGbits.PSA = 0;   // Prescaler is for Timer 0, not for WDT
    OPTION_REGbits.PS = 7;    // Adjust Timer 0 Prescaler to 1:256
    TMR0 = 0xff - 98;         // Initial value of Timer 0 for 5.0176 ms
    TMR0IE = 1;               // Enable Timer 0 interruption
}

// Initialize LED
void led_init(void) {
    TRISB5 = 0;  // RB5 is output for LED
    ANS13 = 0;   // RB5/AN13 is digital
    LED = 0;
}

// Initialize Buzzer
void buzzer_init(void) {
    TRISB7 = 0;  // RB7 is output for BUZZER
    BUZZER = 0;
}

void beep() {
    BUZZER = ON;
    delay_big_ms(200);
    BUZZER = OFF;
}

void welcome_message() {
    lcd_goto(0);
    lcd_puts("AT05");
    lcd_goto(64);
    lcd_puts("T1-G5");
    delay_s(4);  // 4-second delay, could be done with interruption
}


/*----------------------------------------------------------------------------------------------------------------*/

void main(void) {
    char serialIn = 255;
    char keyIn = FALSE;

    spi_init();      // initialize SPI for LCD, LED RGB, battery, compass
    led_rgb_init();  // initialize RGB LED
    battery_init();  // initialize battery reading
    compass_init();  // initialize compass
    sensor_init();   // initialize sensors
    lcd_init();      // initialize LCD
    sensor_init();   // initialize sensors

    /* Local board initializations */
    t0_init();      // initialize Timer 0 for periodic interruption (~5 ms)
    serial_init();  // initialize serial communication channel
    key_init();     // initialize key
    led_init();     // initialize LED for debugging
    buzzer_init();  // initialize buzzer

    GIE = 1;

    delay_s(2);
    lcd_clear();
    lcd_show_cursor(OFF);

    welcome_message();  // write the welcome message
    beep();             // sound signal
    lcd_clear();        // clear the LCD for the next instructions

    lcd_show_cursor(ON);  // turn on the cursor for aesthetic effect in the welcome message

    int pos = 0;    // auxiliary for the position of sent characters
    int pos2 = 64;  // auxiliary for the position of received characters

    char temp;   // temp variable to check if the character has changed
                 // (avoids executing lcd_puts in every cycle of the main loop)
    temp = '%';  // initialize with any value different from 0

    while (1) {                    // infinite loop
        if (current != temp) {     // if the index recorded in temp has changed
            lcd_goto(pos);         // go to the current position
            lcd_putchar(current);  // write character on the LCD
            temp = current;        // update temp
        }

        keyIn = key_pressed();

        if (keyIn) {     // if the button is pressed
            pos++;       // move to the next position on the LCD
            temp = '%';  // reset temp (remembering that the value was
                         // chosen because it is impossible)

            putch(current);  // send the character chosen by the serial channel

            if (pos > 7) {  // if the position exceeds the limit of characters on the display
                pos = 0;    // return to position 0
            }

            current = '0';  // reset the character so that the sequence starts again from
                            // zero in the new position
        }

        serialIn = chkchr();  // receive the character

        if (serialIn != 255) {      // detect arrival of character in the serial buffer
            lcd_goto(pos2);         // go to the current position of the second line
            pos2++;                 // increment the position for writing the next character
            lcd_putchar(serialIn);  // write character on the LCD

            if (pos2 > 71) {  // if the position exceeds the limit of characters on the display
                pos2 = 64;    // reset the value of pos2 to the first position on the second line
            }
        }
    }  // while
}  // main
