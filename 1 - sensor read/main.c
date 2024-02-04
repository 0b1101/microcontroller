/*---------------------------------------------------------------------------------------*/
// The program aims to assist in calibrating the proximity sensor installed on the cart.
//
// Date of creation: 06/05/2023
// Last updated: 10/05/2023
/*---------------------------------------------------------------------------------------*/

#include <xc.h>

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// CONFIG1
#pragma config FOSC = EC    // Oscillator Selection bits (EC: I/O function on RA6/OSC2/CLKOUT pin, CLKIN on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF   // Watchdog Timer Enable bit (WDT disabled and can be enabled by SWDTEN bit of the WDTCON register)
#pragma config PWRTE = ON   // Power-up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = ON   // RE3/MCLR pin function select bit (RE3/MCLR pin function is MCLR)
#pragma config CP = OFF     // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF    // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = OFF  // Brown Out Reset Selection bits (BOR disabled)
#pragma config IESO = ON    // Internal External Switchover bit (Internal/External Switchover mode is enabled)
#pragma config FCMEN = ON   // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is enabled)
#pragma config LVP = OFF    // Low Voltage Programming Enable bit (RB3 pin has digital I/O, HV on MCLR must be used for programming)

// CONFIG2
#pragma config BOR4V = BOR40V  // Brown-out Reset Selection bit (Brown-out Reset set to 4.0V)
#pragma config WRT = OFF       // Flash Program Memory Self Write Enable bits (Write protection off)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// Includes
#include <stdio.h>  // For sprintf() usage

#include "./libraries/always.h"  // Useful structures and unions
#include "./libraries/delay.h"   // Several delays
#include "./libraries/key.h"     // To use the board's switch
#include "./libraries/lcd8x2.h"  // LCD for the robot
#include "./libraries/sensor.h"  // Line sensors, proximity sensors, and buzzer
#include "./libraries/spi.h"     // SPI interface

// Definitions
#define VERSION "1.1"

// Outputs
#define LED RB5     // Output bit for the LED
#define BUZZER RB7  // Bit for the buzzer


// Definition of a global array to store the values of the measurements
volatile int counter = 0;
volatile int sum = 0;

void __interrupt() isr() {  // General interrupt handling routine
    // Locally declared static variables retain their value
    static int tick;  // Counter of times Timer 0 interrupts

    // Timer 0
    // Interrupts approximately every 5 ms.
    // Flashes the LED approximately every 1 second.
    // Controls the debounce time of the switch in conjunction with the I-O-C of PORT B
    if (T0IE && T0IF) {  // If it is an interrupt from Timer 0
        // Here the Timer 0 flag is used to determine when the AD conversion
        // should be performed
        // If Timer 0 interrupts every 5 ms, 50 ticks correspond to 250 ms
        // 4 * 250 ms = 1 s (4 AD measurements every second)
        if (++tick >= 50) {
            tick = 0;
            counter++;
            sum += sensorNear_read();
        }

        // Debounce da chave. Deve ser incluído na interrupção periódica de Timer.
        // São 2 ciclos para debounce entre 5 e 10 ms para interrupção de ~5ms.
        // Deve ser alterado de acordo com o tempo dado pela interrupção do Timer.
        // Por exemplo, se a interrupção for a cada 1 ms, o parâmetro deve ser 10
        // o que resulta num debounce de 9 a 10 ms.
        key_debounce(2);  // 2 is the number of cycles to achieve 5 to 10 ms

        TMR0 = 0xff - 98;  // TMR0_SETTING; reloads the count in Timer 0
        T0IF = 0;          // clears the interrupt flag
    }                      // end - handling of Timer 0

    // Interrupt-on-change of PORT B
    // The debounce time is controlled by Timer 0. The change of state of the switch
    // triggers a time count by Timer 0 from 5 to 10 ms. Every time there is a
    // change of state of the switch, the debounce time count is reset.

    if (RBIE && RBIF) {      // If it is a change of state in Port B
        char portB = PORTB;  // Reads Port B, resetting the interrupt condition along with RBIF = 0
        key_read(portB);     // Reads the key
        RBIF = 0;            // Resets the interrupt flag to be able to receive another interruption
    }                        // End - handling I-O-C PORT B

}  // End - Handling of all interruptions


/*--------------------------------------------------------------------------------*/
// Auxiliary functions

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

/// To use the BUZZER, SENSOR POWER must be turned on.
void beep() {           /// Plays a beep
    BUZZER = ON;        // Turn on buzzer
    delay_big_ms(300);  // for 0.3s
    BUZZER = OFF;       // Turn off buzzer
}

/// Displays the initial message on the LCD - adapted from HW-Test
void welcome_message() {
    lcd_goto(0);        // Go to the beginning of the 1st line
    lcd_puts("AT04");   // Display the string with the activity number on the LCD
    delay_ms(1);        // Allow time for the display to show the message
    lcd_goto(64);       // Go to the beginning of the 2nd line
    lcd_puts("T1-G5");  // Group and team number
}


/*--------------------------------------------------------------------------------*/

void main(void) {
    // Peripheral configurations
    // Local variables
    char sVar[9];        // Auxiliary string for 8 characters
    int countKey = 0;    // Counter for the number of times the key is pressed
    char keyIn = FALSE;  // Pressed key, TRUE = yes


    // Initializations

    // Initialize the robot
    spi_init();     // Initialize SPI for peripheral use
    lcd_init();     // Initialize LCD
    sensor_init();  // Initialize sensors

    // Local board initializations
    t0_init();      // Initialize Timer 0 for periodic interruption (~5 ms)
    key_init();     // Initialize the key
    led_init();     // Initialize LED for debugging
    buzzer_init();  // Initialize buzzer

    // Interruption control
    GIE = 1;  // Enable interruptions
    LED = 0;

    // Initial configurations

    // Sensors
    sensor_power(ON);  // Turn on sensor power, takes 40 ms to turn on the proximity sensor

    // LCD
    delay_s(4);            // Wait to read the reset message
    lcd_clear();           // Clear LCD, should not be used within loops as it takes a long time
    lcd_show_cursor(OFF);  // Turn off LCD cursor

    // Initial message on LCD
    welcome_message();  // Display initial message on LCD
    beep();             // Play a beep to signal that it is ready

    char text1[9];  // Auxiliary string for 8 characters
    char text2[9];  // Auxiliary string for 8 characters
    char keyIn = FALSE;
    int mean = 0;

    while (1) {
        keyIn = key_pressed();

        if (keyIn == TRUE) {
            counter = 0;
            sum = 0;
            LED = ~LED;
            beep();
        }

        if (counter == 9) {
            mean = sum / 10;  // Force conversion to int
            sprintf(text2, "%5d", mean);
            lcd_goto(64);     // Go to the beginning of the 1st line
            lcd_puts(text2);  // Display string on LCD to check if it's working
        }

        delay_ms(100);
    }
}