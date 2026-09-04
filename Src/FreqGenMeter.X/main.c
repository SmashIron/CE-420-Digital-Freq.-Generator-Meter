// Michael Collins, Joshua Tewolde
// Date: 9/31/2026
// Microcomputer Systems Final Project Summer 2026

// Description: Frequency Generator and Meter
// Description

//Performs the following tasks:
    // Task 1: S
    // Task 2: R
    // Task 3: D


// configuration bit settings, Fcy=80MHz, Fpb=40MHz
#pragma config POSCMOD=XT, FNOSC=PRIPLL
#pragma config FPLLIDIV=DIV_2, FPLLMUL=MUL_20
#pragma config FPLLODIV=DIV_1, FPBDIV=DIV_2
#pragma config FWDTEN=OFF, CP=OFF, BWP=OFF

#include <stdio.h>
#include <stdlib.h>
#include <plib.h>
#include <xc.h>
#include <sys/attribs.h>
#include "config.h"

#include "lcd.h"

//Kitchen timer states typedef
//typedef enum {
//    START,
//    RUNNING,
//    PAUSE,
//    END
//} KitchenState;

// Variables
char x;

// Functions
void initMain(void);
void turnOnSeg(int n);
void msDelay(int ms);

//Timers (Multi-Vector Interrupt)

// Task 2: Refresh 7-segment Displays to display 4 byte buffer
// 50-Hz timer with lowest priority
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2_ISR(void)
{
    char strMsg[80];
    IFS0bits.T2IF = 0;    // Reset Interrupt Flag Status bit.
    sprintf(strMsg, "O Freq:%04d", x);
    LCD_WriteStringAtPos(strMsg, 0, 0);
    sprintf(strMsg, "I Freq:%04d", x+1);
    LCD_WriteStringAtPos(strMsg, 1, 0);
}

// Task 3: Decrement timer buffer according to minutes/seconds
// 1-Hz timer with highest priority
void __ISR(_TIMER_3_VECTOR, ipl3) Timer3_ISR(void)
{
    IFS0bits.T3IF = 0;    // Reset Interrupt Flag Status bit.
}

main()
{
//Configuration
    initMain(); //Initialize the clock and GPIO
    
    //Configure Timer 2 for 20 ms periodic interrupt
    //(49,999+1)*32/80,000,000 = 0.02 seconds
    T2CON = 0;          //Turn Timer2 OFF
    TMR2 = 0;           //Clear the count of Timer2
    PR2 = 0xC34F;       //Period register set to 49,999
    T2CON = 0x0050;     //Set pre-scaler to 32 -> (101)b
    
    IPC2bits.T2IP = 2;  //Set Timer2 priority level to 2
    IEC0bits.T2IE = 1;  //Enable Timer2 interrupt
    
    //Configure Timer 3 for 0.2 second periodic interrupt
    //(62499+1)*256/80,000,000 = 0.2 seconds
    T3CON = 0;          //Turn Timer3 OFF
    TMR3 = 0;           //Clear the count of Timer3
    PR3 = 0xF423;       //Period register set to 62,499
    T3CON = 0x0070;     //Set pre-scaler to largest value 256 -> (111)b
    
    IPC3bits.T3IP = 3;  //Set Timer3 priority level to 3
    IEC0bits.T3IE = 1;  //Enable Timer3 interrupt
    
// Multi-vector
    INTEnableSystemMultiVectoredInt();
    T2CONSET = 0x8000;  //Turn Timer 2 ON (will always be ON)
    T3CONSET = 0x8000;  //Turn Timer 3 ON (will always be ON)



    LCD_Init();
    //LCD_WriteStringAtPos("Output Freq: 100", 0, 0);
    //LCD_WriteStringAtPos("Input Freq: 150", 1, 0);
    
    
// Task 1: S
    x = 0;    // Represents current column that is pulled low
    
    //Program
    while (1)
    {
//        LCD_DisplayClear();
        x++;
        msDelay(2000);
    }

}; //main

//Initialize Main
void initMain(void) {
    SYSTEMConfigPerformance(80000000);
    DDPCONbits.JTAGEN = 0;  // Statement is required to use Pin RA0 as IO

    ANSELA = 0;  // PORTA all digital
    ANSELB = 0;  // PORTB all digital
    ANSELC = 0;  // PORTC all digital
    ANSELG = 0;  // PORTG all digital
    ANSELF = 0;  // PORTF all digital
    
    // The four pins driving keypad columns are configured as output;
    TRISCbits.TRISC2 = 0;  // Column 3 configured as output
    TRISCbits.TRISC1 = 0;  // Column 2 configured as output
    TRISCbits.TRISC4 = 0;  // Column 1 configured as output
    TRISGbits.TRISG6 = 0;  // Column 0 configured as output
    
    // The four pins driving keypad rows are configured as input;
    TRISGbits.TRISG9 = 1;  // Row 3 configured as output
    TRISGbits.TRISG8 = 1;  // Row 2 configured as output
    TRISGbits.TRISG7 = 1;  // Row 1 configured as output
    TRISCbits.TRISC3 = 1;  // Row 0 configured as output
}

//Delays for the specified number of milliseconds, ms
void msDelay(int ms) {
    int j = 4000*ms;
    for (j; j != 0; j--);
}