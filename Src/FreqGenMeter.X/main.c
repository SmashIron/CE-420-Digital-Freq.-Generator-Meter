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

//Display states typedef
typedef enum {
    state_DISPLAY,
    state_EDIT
} displayState;

// Variables
char key;
volatile int currentOutFreq = 0; //In Hz
volatile int currentInFreq = 0;  //In Hz
volatile int potentialOutFreq = 0;
int freqDivider = 0;

char lcdLine1[16], lcdLine2[16];    //Each line can have up to 16 characters
displayState state = state_DISPLAY;

// Functions
void initMain(void);
void configTimers(void);
void findOCsettings(int desiredFreq, int *prescaler, int *prValue, int *OCXRS);
void msDelay(int ms);

//Timers (Multi-Vector Interrupt)

// Task 2: Refresh LCDs
// 50-Hz timer with lowest priority
void __ISR(_TIMER_1_VECTOR, ipl2) Timer1_ISR(void)
{
    char strMsg[80];
    IFS0bits.T1IF = 0;    // Reset Interrupt Flag Status bit.
    
    //Print top line
    if (state == state_DISPLAY) {
        sprintf(strMsg, "O Hz:%10d", currentOutFreq);
    }
    else {  //In edit State
        sprintf(strMsg, "OEHz:%10d", potentialOutFreq);
    }
    LCD_WriteStringAtPos(strMsg, 0, 0);
    
    //Print bottom line
    sprintf(strMsg, "I Hz:%10d", currentInFreq);
    LCD_WriteStringAtPos(strMsg, 1, 0);
}

// Task 3: Calculate input frequency
// 5-Hz timer with highest priority
void __ISR(_TIMER_2_VECTOR, ipl3) Timer2_ISR(void)
{
    freqDivider++;
        currentInFreq = 5*TMR4; //Obtain external clock counter
        TMR4 = 0;               //Reset counter
        IFS0bits.T2IF = 0;      // Reset Interrupt Flag Status bit.
        freqDivider = 0;
}

main()
{
//Configuration
    initMain(); //Initialize the clock and GPIO
    configTimers();
    LCD_Init();

// Multi-vector
    INTEnableSystemMultiVectoredInt();
    
    //Turn on all timers
    T1CONSET = 0x8000;  //Always ON
    T2CONSET = 0x8000;  //Always ON
    T4CONSET = 0x8000;  //Always ON


// Task 1: Scan for user inputs and change state
    int col = 0;    // Represents current column that is pulled low
    int row = 0;    // Represents found row that is pulled low (key press)
    int found = 1;  // Represents a key press being found
    key = 0;   // Newest key input
    
    //Key map of keypad by row then column
    const char keyMap[4][4] = {{1,2,3,0xA},{4,5,6,0xB},{7,8,9,0xC},{0,0xF,0xE,0xD}};
    
    //Obtain user inputs from keypad
    while (1)
    {
        // Pull all columns to be high
        Col0 = 1; Col1 = 1; Col2 = 1; Col3 = 1;

        // Actively pull current column low
        switch (col) {
            case 0: Col0 = 0; break;
            case 1: Col1 = 0; break;
            case 2: Col2 = 0; break;
            default: Col3 = 0; break;
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
                case 0: while (Row0 == 0) {/*wait until key is released*/}break;
                case 1: while (Row1 == 0) {/*wait until key is released*/}break;
                case 2: while (Row2 == 0) {/*wait until key is released*/}break;
                default: while (Row3 == 0) {/*wait until key is released*/}break;
            }
            
            //Find corresponding key from column and row
            key = keyMap[row][col];
            
            //State logic to determine how to update frequencies and change state
            if (0 <= key && key <= 9) {   //Decimal character inputted
                if (state == state_DISPLAY) {
                    potentialOutFreq = key;
                    state = state_EDIT;
                }
                else {
                    //Shift current value left and truncate left most digits
                    potentialOutFreq = (potentialOutFreq*10 + key) % 10000000000;
                }
            }
            else if (key == 0xA) { //New output frequency entered
                //Check if new frequency is valid
                if (10 <= potentialOutFreq && potentialOutFreq <= 10000000) {
                    currentOutFreq = potentialOutFreq;
                    potentialOutFreq = 0;
                    int prescaler, prValue, OCXRS;
                    findOCsettings(currentOutFreq, &prescaler, &prValue, &OCXRS);
                    
                    
                    //Setup OC4 with Timer 3 as double compare pulse train
                    T3CON = 0;              //Disable Timer 3 and set pre-scaler
                    T3CONbits.TCKPS = prescaler;
                    
                    OC4CON = 0;             //Turn off OC4
                    OC4CONbits.OCM = 0b101; //Dual compare pulse train mode
                    OC4CONbits.OC32 = 0;    //Select Timer 3
                    OC4CONbits.OCTSEL = 1;  //Select Timer 3

                    OC4R = 0;          //Set primary compare register
                    OC4RS = OCXRS;     //Set secondary compare register
                    PR3 = prValue;     //Set period register of Timer 3

                    IEC0bits.OC4IE = 0;     //Disable interrupt on OC4
                    IFS0bits.OC4IF = 0;     //Clear interrupt flag

                    TRISDbits.TRISD11 = 0;      //RD11 as output
                    RPD11Rbits.RPD11R = 0b1011; //RD11 selected to OC4 according to datasheet

                    T3CONbits.ON = 1;       //Enable Timer 3
                    OC4CONbits.ON = 1;      //Enable OC4
                }
                state = state_DISPLAY;
            }
            else if (key == 0xC) {
                potentialOutFreq = 0;
                state = state_DISPLAY;
            }
            else if (key == 0xE) {
                OC4CON = 0; //Turn off OC4
                return 0;   //Exit safely
            }
        }   //End of key found
        
        //Go to next column and loop around if necessary
        (col == 3) ? (col = 0):(col++);
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

//Configure Timers ...
void configTimers(void) {
//Configure Timer 1 for 20 ms periodic interrupt
    //(24,999+1)*64/80,000,000 = 0.02 seconds
    T1CON = 0;          //Turn OFF
    TMR1 = 0;           //Clear the count
    PR1 = 24999;        //Period register set to 49,999
    T1CONbits.TCKPS = 2;//Set pre-scaler to 256 -> 3
    IPC1bits.T1IP = 2;  //Set priority level
    IEC0bits.T1IE = 1;  //Enable interrupt
    
//Configure Timer 2 for 1 s periodic interrupt
    //(62499+1)*256/80,000,000 = 0.2 seconds
    T2CON = 0;          //Turn OFF
    TMR2 = 0;           //Clear the count
    PR2 = 62999;        //Period register set to 62,499
    T2CON = 0x0070;     //Set pre-scaler to 256 -> (111)b
    IPC2bits.T2IP = 3;  //Set priority level
    IEC0bits.T2IE = 1;  //Enable interrupt
    
//Configure Timer 3 for OC4, but both start off
    T3CON = 0;          //Disable Timer 3 and set pre-scaler
    OC4CON = 0;         //Turn off OC4

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
}

//Find timer prescalar, PR Value
//PR is OC4RS*2-1
void findOCsettings(int desiredFreq, int *prescaler, int *prValue, int *OCXRS) {
    const int possiblePrescalar[] = {1, 2, 4, 8, 16, 32, 64, 256};
    int bestN = 0;
    int bestPrescalar = 0;
    
    int i;
    for (i = 0; i < 8; i++) {
        int possibleN = 80000000/(desiredFreq*possiblePrescalar[i]);
        if (possibleN % 2 == 1) {
            possibleN += 1; //Round to closest odd if not quotient isn't even
        }
        if (possibleN > bestN && possibleN < 65536) {
            bestN = possibleN;
            bestPrescalar = i;
        }
    }
        *prescaler = bestPrescalar;
        *prValue = bestN - 1;
        *OCXRS = (bestN/2);
}

//Delays for the specified number of milliseconds, ms
void msDelay(int ms) {
    int j = 4000*ms;
    for (j; j != 0; j--);
}