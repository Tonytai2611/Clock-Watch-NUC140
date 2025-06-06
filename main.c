#include <stdio.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"

// ----------------------------- MACROS ---------------------------------
#define HXT_STATUS (1 << 0)
#define LXT_STATUS (1 << 1)
#define PLL_STATUS (1 << 2)
#define LIRC_STATUS (1 << 3)
#define HIRC_STATUS (1 << 4)

#define PLLCON_FB_DV_VAL 8
#define TCMPR_VALUE 1000000 - 1
#define BUZZER_BEEP_DELAY 100000

// ----------------------------- ENUMS ---------------------------------
typedef enum {
    IDLE,
    ALARM_SET,
    COUNT,
    PAUSE,
    CHECK
} SystemState;

// ----------------------------- FUNCTION DECLARATIONS -----------------
void System_Initial_Setup(void);
void LED_Intitial_Setup(void);
void Keyboard_Initial_Setup(void);
void Segments_Initial_Setup(void);
void System_Config(void);
void Timer0_Initial_Setup(void);

// Interrupts
void TMR0_IRQHandler(void);
void EINT1_IRQHandler(void);

// System working mode configuration
void IDLE_Mode_Setup(void);
void ALARM_SET_Mode_Setup(void);
void COUNT_Mode_Setup(void);
void PAUSE_Mode_Setup(void);
void CHECK_Mode_Setup(void);

// LED/Segment management functions
void turnOnSegmentWithNumberByDigits(int digits, int number);
void turnOnSelectedLED(short led);
void turnOffAllSegmentsLED(void);

// Utility supporting functions
int getKeyPressed(void);
uint8_t getNumberPattern(int number);
int getTimeBetweenLap(int currentRecorded, int previousRecorded);
void triggerBuzzer(void);

// ----------------------------- GLOBAL VARIABLES ----------------------
volatile SystemState currentState = IDLE;
volatile int alarmTime = 0;
volatile int elapsedTime = 0;
volatile int recordedTimeList[5];
volatile int lapIndex = 0;
volatile int lapIndexCount = 0;
volatile int previousRecordedTime = 0;

// ----------------------------- MAIN FUNCTION -------------------------
int main(void) {
    // System initialization
    System_Initial_Setup();
    LED_Intitial_Setup();
    Keyboard_Initial_Setup();
    Segments_Initial_Setup();

    uint8_t keyPressed = 0;
    _Bool isPaused = TRUE;

    while (1) {
        keyPressed = getKeyPressed();

        // Toggle PAUSE/COUNT mode with Key 1
        if (keyPressed == 1) {
            while (keyPressed == getKeyPressed()); // Wait for release
            isPaused = !isPaused;
            currentState = (isPaused) ? PAUSE : COUNT;
        }

        // State machine
        switch (currentState) {
            case IDLE:
                IDLE_Mode_Setup();
                if (keyPressed == 3) { // Key 3 switches to ALARM_SET mode
                    while (keyPressed == getKeyPressed());
                    ALARM_SET_Mode_Setup();
                }
                break;

            case ALARM_SET:
                ALARM_SET_Mode_Setup();
                if (!(PB->PIN & (1 << 15))) { // Increment alarm time with PB15
                    while (!(PB->PIN & (1 << 15))); // Wait for release
                    // CLK_SysTickDelay(10000);        // Debounce delay
                    alarmTime = (alarmTime + 1) % 60;
                    
                } else if (keyPressed == 3) { // Key 3 returns to IDLE mode
                    while (keyPressed == getKeyPressed());
                    IDLE_Mode_Setup();
                }
                break;

            case COUNT:
                COUNT_Mode_Setup();
                if (elapsedTime / 10 == alarmTime) { // Alarm triggered
                    triggerBuzzer();      // Sound buzzer
                }
                if (keyPressed == 9) { // Record lap time with Key 9
                    while (keyPressed == getKeyPressed()); // Wait for release
                    recordedTimeList[lapIndex] = getTimeBetweenLap(elapsedTime, previousRecordedTime);
                    lapIndex = (lapIndex + 1) % 5;
                    previousRecordedTime = elapsedTime;
                }
                break;

            case PAUSE:
                PAUSE_Mode_Setup();
                lapIndexCount = 0; // Reset lap index count

                if (keyPressed == 9) { // Reset with Key 9
                    while (keyPressed == getKeyPressed()); // Wait for release
                    elapsedTime = 0;
                    IDLE_Mode_Setup();

                } else if (keyPressed == 5) { // Enter CHECK mode with Key 5
                    while (keyPressed == getKeyPressed()); // Wait for release
                    CHECK_Mode_Setup();
                }
                break;

            case CHECK:
                turnOnSegmentWithNumberByDigits(3, lapIndexCount + 1);
                turnOnSegmentWithNumberByDigits(2, ((recordedTimeList[lapIndexCount] % 600) / 10) / 10);
                turnOnSegmentWithNumberByDigits(1, (recordedTimeList[lapIndexCount] / 10) % 10);
                turnOnSegmentWithNumberByDigits(0, (recordedTimeList[lapIndexCount] % 10));
                
                if (keyPressed == 5) { // Exit CHECK mode with Key 5
                    while (keyPressed == getKeyPressed()); // Wait for release
                    PAUSE_Mode_Setup();
                }
                break;
        }
    }
}

// ----------------------------- SYSTEM INITIALIZATION -----------------
void System_Initial_Setup(void) {
    SYS_UnlockReg(); // Unlock the register
    System_Config();
    Timer0_Initial_Setup();
    SYS_LockReg();  // Lock the register
}

void LED_Intitial_Setup(void) {
    GPIO_SetMode(PC, BIT12, GPIO_MODE_OUTPUT); 
    GPIO_SetMode(PC, BIT13, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT14, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT15, GPIO_MODE_OUTPUT);
    PC12 = 1; // LED5
    PC13 = 1; // LED6
    PC14 = 1; // LED7
    PC15 = 1; // LED8

    GPIO_SetMode(PD, BIT15, GPIO_MODE_OUTPUT);
    PD15 = 1;
}

void Keyboard_Initial_Setup(void) {
    GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);

    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT); // PB15 as input
    PB->IEN |= (1 << 15);                     // Enable interrupt
    NVIC->ISER[0] |= (1 << 3);                // Enable NVIC interrupt for GPB
    NVIC->IP[0] &= ~(0x03 << 30);             // Set priority 0

    // Enable debounce for keyboard pins
    PA->DBEN |= (1 << 3) | (1 << 4) | (1 << 5); // Enable debounce for PA3, PA4, PA5
    PB->DBEN |= (1 << 15);                      // Enable debounce for PB15

    // Set debounce clock
    GPIO->DBNCECON &= ~(0xFF << 0);             // Clear debounce clock settings
    GPIO->DBNCECON |= (0x9 << 0);               // Set to 512 clocks
    GPIO->DBNCECON |= (1 << 4);
}

void Segments_Initial_Setup(void) {
    // Set PC4-7 as U11-U14 as OUTPUT
    GPIO_SetMode(PC, BIT4, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT5, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT6, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);

    // Set PE0-7 as OUTPUT
    GPIO_SetMode(PE, BIT0, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT1, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT2, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT3, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT4, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT5, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT6, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PE, BIT7, GPIO_MODE_OUTPUT);

    // Set up port to drive LOW
    PC->DOUT &= ~(1 << 7);
    PC->DOUT &= ~(1 << 6);
    PC->DOUT &= ~(1 << 5);
    PC->DOUT &= ~(1 << 4);
}

// ----------------------------- SYSTEM CONFIGURATION ------------------
void System_Config(void) {
    CLK->PWRCON |= (1 << 0);  // Enable HXT
    while (!(CLK->CLKSTATUS & HXT_STATUS));

    CLK->PLLCON &= ~(1 << 19);
    CLK->PLLCON &= ~(1 << 16);
    CLK->PLLCON &= (~(0x01FF << 0));
    CLK->PLLCON |= PLLCON_FB_DV_VAL; // 50MHz
    CLK->PLLCON &= ~(1 << 18);
    while (!(CLK->CLKSTATUS & PLL_STATUS));

    CLK->CLKSEL0 &= (~(0x07 << 0));
    CLK->CLKSEL0 |= (0x02 << 0);
    CLK->CLKDIV &= ~(0x0F << 0);
}

void Timer0_Initial_Setup(void) {
    CLK->CLKSEL1 &= ~(0x07 << 8);
    CLK->CLKSEL1 |= (0x02 << 8);
    CLK->APBCLK |= (1 << 2);

    TIMER0->TCSR &= ~(0xFF << 0);
    TIMER0->TCSR |= (1 << 26);
    TIMER0->TCSR &= ~(0x03 << 27);
    TIMER0->TCSR |= (0x01 << 27);
    TIMER0->TCSR &= ~(1 << 24);
    TIMER0->TCSR |= (1 << 29);

    NVIC->ISER[0] |= (1 << 8);
    NVIC->IP[0] &= ~(0x03 << 6);

    TIMER0->TCMPR = TCMPR_VALUE;
}

// ----------------------------- INTERRUPT HANDLERS -------------------
void TMR0_IRQHandler(void) {
    if (TIMER0->TISR & (1 << 0)) {
        TIMER0->TISR |= (1 << 0);
    }

    if (currentState == COUNT) {
        elapsedTime++;
        PD->DOUT ^= (1 << 15);
    }
}

void EINT1_IRQHandler(void) {
    if (PB->ISRC & (1 << 15)) {
        PB->ISRC |= (1 << 15);
    }

    if (currentState == CHECK) {
        lapIndexCount = (lapIndexCount + 1) % 5;
    }
}

// ----------------------------- MODE CONFIGURATIONS -------------------
void IDLE_Mode_Setup(void) {
    currentState = IDLE;
    turnOnSelectedLED(5);
    turnOnSegmentWithNumberByDigits(3, 0);
    turnOnSegmentWithNumberByDigits(2, 0);
    turnOnSegmentWithNumberByDigits(1, 0);
    turnOnSegmentWithNumberByDigits(0, 0);
}

void ALARM_SET_Mode_Setup(void) {
    currentState = ALARM_SET;
    turnOnSelectedLED(8);
    turnOnSegmentWithNumberByDigits(2, alarmTime / 10);
    turnOnSegmentWithNumberByDigits(1, alarmTime % 10);
}

void COUNT_Mode_Setup(void) {
    currentState = COUNT;
    turnOnSelectedLED(6);
    TIMER0->TCSR |= (1 << 30); // Timer0 starts counting

    turnOnSegmentWithNumberByDigits(3, elapsedTime / 600);
    turnOnSegmentWithNumberByDigits(2, ((elapsedTime % 600) / 10) / 10);
    turnOnSegmentWithNumberByDigits(1, (elapsedTime / 10) % 10);
    turnOnSegmentWithNumberByDigits(0, elapsedTime % 10);
}

void PAUSE_Mode_Setup(void) {
    currentState = PAUSE;
    turnOnSelectedLED(7);
    TIMER0->TCSR &= ~(1 << 30); // Timer0 stops counting

    turnOnSegmentWithNumberByDigits(3, elapsedTime / 600);
    turnOnSegmentWithNumberByDigits(2, ((elapsedTime % 600) / 10) / 10);
    turnOnSegmentWithNumberByDigits(1, (elapsedTime / 10) % 10);
    turnOnSegmentWithNumberByDigits(0, elapsedTime % 10);
}

void CHECK_Mode_Setup(void) {
    currentState = CHECK;
}

// ----------------------------- UTILITY FUNCTIONS --------------------
int getKeyPressed(void) {
    // Checking each column of the keyboard with all its rows to get 
    // exactly what key is pressed 

    // Check the first column, if button in this col is pressed, 
    // get the value of the pressed one into INT
    PA0 = 1; PA1 = 1; PA2 = 0;
    if (PA3 == 0) return 1;
    if (PA4 == 0) return 4;
    if (PA5 == 0) return 7;

    // Check the second column, if button in this col is pressed, 
    // get the value of the pressed one into INT
    PA0 = 1; PA1 = 0; PA2 = 1;
    if (PA3 == 0) return 2;
    if (PA4 == 0) return 5;
    if (PA5 == 0) return 8;

    // Check the third column, if button in this col is pressed, 
    // get the value of the pressed one into INT
    PA0 = 0; PA1 = 1; PA2 = 1;
    if (PA3 == 0) return 3;
    if (PA4 == 0) return 6;
    if (PA5 == 0) return 9;

    // If no keyboard is recorded to be pressed when function called
    // return 0 value
    return 0;
}

uint8_t getNumberPattern(int number) {
    int numberInBinaryPattern[] = { 
        0b10000010, // 0
        0b11101110, // 1
        0b00000111, // 2
        0b01000110, // 3
        0b01101010, // 4
        0b01010010, // 5
        0b00010010, // 6
        0b11100110, // 7
        0b00000010, // 8
        0b01000010, // 9
    };
    return numberInBinaryPattern[number];
}

int getTimeBetweenLap(int currentRecorded, int previousRecorded) {
    return currentRecorded - previousRecorded;
}

void turnOnSegmentWithNumberByDigits(int digits, int number) {
    // Get the binary number pattern
    PE->DOUT = getNumberPattern(number);

    // Turn off all the 7 segments
    turnOffAllSegmentsLED();
    
    // Turn on the PC with corresponding digit
    switch (digits) {
        case 0: 
            PC4 = 1; break;

		case 1: 
            PC5 = 1;
            // Only IDLE and CHECK Mode does not show the DOT
            PE1 = (currentState == IDLE) ? 1 : 0;
            break;

		case 2: 
            PC6 = 1; break;

		case 3: 
            PC7 = 1; break;
    }
    CLK_SysTickDelay(1000);
}

void turnOnSelectedLED(short led) {
    // As the LED is set to pull-up, turn off all LED by set
    // their value to 1
    PC12 = 1; PC13 = 1; PC14 = 1; PC15 = 1;

    // With the selected LED, turn it on
    switch (led) {
        case 5: PC12 = 0; break;
        case 6: PC13 = 0; break;
        case 7: PC14 = 0; break;
        case 8: PC15 = 0; break;
    }
}

void turnOffAllSegmentsLED(void) {
    PC4 = 0; PC5 = 0; PC6 = 0; PC7 = 0;
}

void triggerBuzzer(void) {
    turnOffAllSegmentsLED();

    for (int i = 0; i < 6; i++) {
        PB->DOUT ^= (1 << 11);
        CLK_SysTickDelay(BUZZER_BEEP_DELAY);
    }
    PB->DOUT |= (1 << 11); // Turn off buzzer to ensure it stops
}