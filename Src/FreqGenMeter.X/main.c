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

//Macros for keypad columns and rows
#define Col3 LATCbits.LATC2
#define Col2 LATCbits.LATC1
#define Col1 LATCbits.LATC4
#define Col0 LATGbits.LATG6
#define Row0 PORTGbits.RG9
#define Row1 PORTGbits.RG8
#define Row2 PORTGbits.RG7
#define Row3 PORTCbits.RC3

//Kitchen timer states typedef
//typedef enum {
//    START,
//    RUNNING,
//    PAUSE,
//    END
//} KitchenState;

// Variables
char x;
int currentOutFreq = 0; //In Hz
int currentInFreq = 0;  //In Hz
char lcdLine1[16], lcdLine2[16];    //Each line can have up to 16 characters

// Functions
void initMain(void);
void msDelay(int ms);

//Timers (Multi-Vector Interrupt)

// Task 2: Refresh 7-segment Displays to display 4 byte buffer
// 50-Hz timer with lowest priority
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2_ISR(void)
{
    char strMsg[80];
    IFS0bits.T2IF = 0;    // Reset Interrupt Flag Status bit.
    sprintf(strMsg, "O Hz:%04d", TMR4);
    LCD_WriteStringAtPos(strMsg, 0, 0);
    sprintf(strMsg, "I Hz:%04d", TMR4+1);
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
    
    //Configure Timer 45 for external clock source
    T4CON = 0;          //Turn Timer4 OFF and set pre-scaler to smallest value 1 -> (000)b
    T5CON = 0;          //Turn Timer5 OFF
    T4CONbits.T32 = 1;  //Enable 32-bit mode
    T4CONbits.TCS = 1;  //Turn on external clock source
    PR4 = 0xFFFFFFFF;   //Largest possible period register
    TMR4 = 0;           //Clear the count of Timer45
    IEC0bits.T5IE = 0;  //Disable interrupt on Timer45
    IFS0bits.T5IF = 0;  //Clear any pending Timer 45 interrupt flag
    
    //Connect external clock source pin
    TRISDbits.TRISD9 = 1;       //RD9 as input
    T4CKRbits.T4CKR = 0b0000;   //Select RPD9 for T4CK according to datasheet
    
    T4CONbits.ON = 1;   //Turn Timer45 ON
    
    
    
    
    
    //Setup OC4 with Timer 3 as double compare pulse train
    T3CON = 0;              //Disable Timer 3 and set pre-scaler
    
    OC4CON = 0;             //Turn off OC4
    OC4CONbits.OCM = 0b101; //Dual compare pulse train mode
    OC4CONbits.OC32 = 0;    //Select Timer 3
    OC4CONbits.OCTSEL = 1;  //Select Timer 3
    
    OC4R = 0;          //Set primary compare register
    OC4RS = 0x9FFF;         //Set secondary compare register
    PR3 = 0xFFFF;           //Set period register of Timer 3
    
    IEC0bits.OC4IE = 0;     //Disable interrupt on OC4
    IFS0bits.OC4IF = 0;     //Clear interrupt flag
    
    TRISDbits.TRISD11 = 0;      //RD11 as output
    RPD11Rbits.RPD11R = 0b1011; //RD11 selected to OC4 according to datasheet
    
    T3CONbits.ON = 1;       //Enable Timer 3
    OC4CONbits.ON = 1;      //Enable OC4
    
    
    
    

    
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
    
    // Task 1: Scan keypad inputs to update timer and state accordingly
    int col = 0;    // Represents current column that is pulled low
    int row = 0;    // Represents found row that is pulled low (key press)
    int found = 1;  // Represents a key press being found
    char key = 0;   // Newest key input
    
    //Key map of keypad by row then column
    char keyMap[4][4] = {{1,2,3,0xA},{4,5,6,0xB},{7,8,9,0xC},{0,0xF,0xE,0xD}};
    
    
    //Obtain user inputs from keypad
    while (1)
    {
        // Pull all columns to be high
        Col0 = 1;
        Col1 = 1;
        Col2 = 1;
        Col3 = 1;
        
        // Actively pull current column low
        switch (col) {
            case 0:
                Col0 = 0;
                break;
            case 1:
                Col1 = 0;
                break;
            case 2:
                Col2 = 0;
                break;
            default:
                Col3 = 0;
                break;
        }
        
        //Wait 1 ms
        msDelay(1);
        
        //Find which row, if any, is pressed to be low
        found = 1;
        if (Row0 == 0) row = 0;
        else if (Row1 == 0) row = 1;
        else if (Row2 == 0) row = 2;
        else if (Row3 == 0) row = 3;
        else found = 0;
        
        // Stay in "wait loop" until key is released
        if (found == 1) {
            switch (row) {
                case 0:
                    while (Row0 == 0) {/*wait until key is released*/}break;
                case 1:
                    while (Row1 == 0) {/*wait until key is released*/}break;
                case 2:
                    while (Row2 == 0) {/*wait until key is released*/}break;
                default:
                    while (Row3 == 0) {/*wait until key is released*/}break;
            }
            
            //Find corresponding key from column and row
            key = keyMap[row][col];
            
//            //State logic to determine how to update buffer and change state
//            switch (key) {
//                
//            }   //End of switch
        }   //End of key found
        
        //Go to next column and loop around if necessary
        (col == 3) ? (col = 0):(col++);
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