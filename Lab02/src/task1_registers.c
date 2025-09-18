//----------------------------------
// Lab 2 - Template file
//----------------------------------

// -- Imports ---------------
//
#include "init.h"
#include <stdio.h>

//
//
// -- Prototypes ------------
//
void Init_GPIO();
void Init_Timer();

//
// -- Global Variables ------
//
uint8_t button_pressed;


//
//
// -- Code Body -------------
//

int main() {
	Sys_Init();
	// Init_Timer();
	Init_GPIO();

	button_pressed = 0;

	while (1) {

		if (button_pressed) {
			printf("button pressed \t");
			button_pressed = 0;
		}

	}
}

//
//
// -- Init Functions ----------
//
void Init_Timer() {
	// To do:
	//
	// Enable the TIM6 interrupt (through NVIC).
	//
	// Enable TIM6 clock
	// ** New F23: necessary clocks are all enabled in Sys_Init()!
	// **          you do not need to do this!
	//
	// Set the timer clock rate and period
	// sysclk = 216MHz
	// TIM6->PSC = 215999999; // PSC+1
	// TIM6->ARR = 999;
	// Enable update events and generation of interrupt from update
	// IM6->DIER |= TIM_DIER_UIE; // (0x1UL << (0U)) = 1

	// Start the timer
	//
}

void Init_GPIO() {
	// To do:
    //
	// Configure pins as
	// Set Pin 13/5 to output. (LED1 and LED2)
	GPIOJ->MODER |= 0x4000400U;

	// GPIO Interrupt
	// By default pin PA0 will trigger the interrupt, change EXTICR1 to route proper pin
	// SYSCFG->EXTICR[0] // EXTICR1-4 are confusingly an array [0-3].
	SYSCFG->EXTICR[0] |= 9U; // PJ0

	// Set Pin 0 as input (button) with pull-down.
	//GPIOA->PUPDR
	GPIOJ->MODER &= ~(3U); 		// PJ0 as input
	GPIOJ->PUPDR &= ~(3U);   	// Clear pull-up/down for PJ0
	GPIOJ->PUPDR |= 2U;			// PJ0 pull-down

	// Set interrupt enable for EXTI0.
	NVIC->ISER[EXTI0_IRQn/32] = (uint32_t) 1 << (EXTI0_IRQn % 32);

	// Unmask interrupt.
	EXTI->IMR |= 1U;

	// Register for rising edge.
	EXTI->RTSR |= 1U;

	// And register for the falling edge.
	EXTI->FTSR &= ~(1U);

}

//
//
// -- ISRs (IRQs) -------------
//
// Change as needed if not using TIM6
void TIM6_DAC_IRQHandler() {
	// Clear Interrupt Bit
	// TIM6->SR &= ~(1U);

	// Other code here:

}

// Non-HAL GPIO/EXTI Handler
void EXTI0_IRQHandler() {
	// Clear Interrupt Bit by setting it to 1.
	EXTI->PR |= 1U;

	// Other code here:
	button_pressed = 1;
	for (int i = 0; i < 100; i++) {
		asm("nop");
	}

}

//HAL - GPIO/EXTI Handler
void xxx_IRQHandler() {
	//HAL_GPIO_EXTI_IRQHandler(???);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	// ISR code here.
}
