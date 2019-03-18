/**
 * main.c
 * Author: Stephen Glass
 * Description: Blink an LED using the task scheduler. Another LED blinks at a period proportional
 * to the result of a distance sensor connected to an ADC channel. Subsystem module is used for
 * logging and debugging.
 *
 * Dependencies: embedded-software library
 * Board: MSP430F5529
 */

#include <msp430.h> 
#include <math.h>
#include "library.h"
#include "project_settings.h"
#include "stephen_game.h"
#include "timing.h"
#include "subsystem.h"
#include "task.h"
#include "uart.h"
#include "hal_general.h"

/* Define if you want logging of LED blinking events */
//#define LOG_LED_EVENTS


#define GPIO_LED1       BIT0  // P1.0
#define GPIO_LED1_OUT   P1OUT // PORT1
#define GPIO_LED1_DIR   P1DIR // P1DIR

#define GPIO_LED2       BIT7  // P4.7
#define GPIO_LED2_OUT   P4OUT // PORT4
#define GPIO_LED2_DIR   P4DIR // P4DIR

#define TASK_VERSION    (version_t)0x01010014u

void InitGPIO(void);
void BlinkLED(uint8_t type);
void SetClk24MHz(void);
void SetVcoreUp (unsigned int level);

static uint8_t sys_id;

void InitGPIO(void) {
    /* LED1 */
    GPIO_LED1_DIR |= (GPIO_LED1); // Configure LED as output
    GPIO_LED1_OUT &= ~(GPIO_LED1); // Default disable LED
    /* LED2 */
    GPIO_LED2_DIR |= (GPIO_LED2); // Configure LED as output
    GPIO_LED2_OUT &= ~(GPIO_LED2); // Default disable LED
}

void BlinkLED(uint8_t type) {
    switch(type) {
    case GPIO_LED1:
        GPIO_LED1_OUT ^= (GPIO_LED1);
        LogMsg(sys_id, "[BlinkLED] LED1 blinked");
        break;
    case GPIO_LED2:
        GPIO_LED2_OUT ^= (GPIO_LED2);
        LogMsg(sys_id, "[BlinkLED] LED2 blinked");
        break;
    default:
        break;
    }
}

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
	SetClk24MHz();
	
	/* Initialize the F5529 GPIO */
	InitGPIO();

	DisableInterrupts();
	Timing_Init();
	Task_Init();
	UART_Init(SUBSYSTEM_UART);
	/* Increase the baud rate for faster response */
	UART_ReconfigureBaud(SUBSYSTEM_UART, 460800);
	EnableInterrupts();

	/* Initialize LED blinking subsystem for logging */
	sys_id = Subsystem_Init("task", TASK_VERSION, 0);
	UART_printf(SUBSYSTEM_UART, "System Initialized\r\n");
	UART_printf(SUBSYSTEM_UART, "Type '$game fly1 play' to begin...\r\n");

    #ifndef LOG_LED_EVENTS
        Log_MuteSys(sys_id);
    #endif

    /* Initialize the game code */
    StephenGame_Init();
    /* Enable echo so user can see what is being typed */
    Log_EchoOn();

	while(1) {
	    SystemTick();
	}
}

void SetClk24MHz() {
    // Increase Vcore setting to level3 to support fsystem=25MHz
    // NOTE: Change core voltage one level at a time..
    SetVcoreUp (0x01);
    SetVcoreUp (0x02);
    SetVcoreUp (0x03);

    P5SEL |= BIT2+BIT3;
    UCSCTL6 &= ~XT2OFF; // Enable XT2
    UCSCTL6 &= ~XT2BYPASS;
    UCSCTL3 = SELREF__XT2CLK; // FLLref = XT2
    UCSCTL4 |= SELA_2 + SELS__DCOCLKDIV + SELM__DCOCLKDIV;

    UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
    // Loop until XT1,XT2 & DCO stabilizes - In this case only DCO has to stabilize
    do
    {
        UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG); // Clear XT2,XT1,DCO fault flags
        SFRIFG1 &= ~OFIFG;                          // Clear fault flags
    } while (SFRIFG1&OFIFG);                        // Test oscillator fault flag

    // Disable the FLL control loop
    __bis_SR_register(SCG0);

    // Select DCO range 24MHz operation
    UCSCTL1 = DCORSEL_7;
    /* Set DCO Multiplier for 24MHz
    (N + 1) * FLLRef = Fdco
    (5 + 1) * 4MHz = 24MHz  */
    UCSCTL2 = FLLD0 + FLLN0 + FLLN2;
    // Enable the FLL control loop
    __bic_SR_register(SCG0);

    /* Worst-case settling time for the DCO when the DCO range bits have been
     changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
     UG for optimization.
     32 x 32 x 24MHz / 4MHz = 6144 = MCLK cycles for DCO to settle */
    __delay_cycles(70000);

    // Loop until XT1,XT2 & DCO stabilizes - In this case only DCO has to stabilize
    do {
        // Clear XT2,XT1,DCO fault flags
        UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
        // Clear fault flags
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG); // Test oscillator fault flag
}

void SetVcoreUp (unsigned int level)
{
    // Open PMM registers for write
    PMMCTL0_H = PMMPW_H;
    // Set SVS/SVM high side new level
    SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
    // Set SVM low side to new level
    SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;
    // Wait till SVM is settled
    while ((PMMIFG & SVSMLDLYIFG) == 0);
    // Clear already set flags
    PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);
    // Set VCore to new level
    PMMCTL0_L = PMMCOREV0 * level;
    // Wait till new level reached
    if ((PMMIFG & SVMLIFG))
        while ((PMMIFG & SVMLVLRIFG) == 0);
    // Set SVS/SVM low side to new level
    SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
    // Lock PMM registers for write access
    PMMCTL0_H = 0x00;
}

