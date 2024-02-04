# Microcontrollers

In this project, we assembled our own hardware for a processing board and developed the software that allowed us to explore various aspects of using microcontrollers in automation and robotics. Among the activities carried out were the acquisition of digital and analog data, presentation of data on LEDs and a liquid crystal display (LCD), serial communication, motor control via PWM, and position reading using encoders.

For a better understanding, the following order is recommended:

 1. Analog sensor reading 
 2. Serial Communication
 3. DC motor
 4. Autonomous task 

For further information on each topic, please refer to its respective folder README.

## Softwares 
Besides the code editor, other softwares used in this project were:

- DipTrace – Electronic Design Automation (EDA) software package for schematic capture and printed circuit board layout.
- MPLAB X – Integrated Development Environment (IDE) for Microchip's PIC microcontrollers, based on NetBeans.
- MPLAB XC8 – C language compiler for Microchip's 8-bit PIC microcontrollers.
- Bootloader AN1310 – Used to load student programs into the PIC program memory via the serial channel. It comes with a built-in terminal emulator.
- FTDI (VCP) Driver – Used for USB-serial interface for communication between the Bootloader and the PIC.

--- 

### Updates

-   May 06th, 2023: Initiation of the project development.
-   June 27nd, 2023: Successful completion of the project.
-   Feburary 3rd, 2024: Project translated and documented in English.

### Further Information

The code presented in this repository was delivered as a project in the 'Microprocessors in Automation and Robotics' course within the Applied and Computer Mathematics bachelor's degree @ POLI-USP. 

Except for `always.h`, the libraries used in this project are the intellectual property of third parties and therefore cannot be distributed.
