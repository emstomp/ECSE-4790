//------------------------------------------------------------------------------------
// Task1.c
//------------------------------------------------------------------------------------
//
// Lab 1 Task 1
//
// Prints out printable characters
// pressed on the terminal keyboard.
//
//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include "stm32f769xx.h"
#include "hello.h"
#include <stdint.h>

#define ESC_KEY 27

//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
int main(void)
{
    Sys_Init();

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    fflush(stdout);

    // Need to enable clock for peripheral bus on GPIO Port J
    __HAL_RCC_GPIOJ_CLK_ENABLE(); 		 // Through HAL
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN; // or through registers

    GPIOJ->MODER |= 1024U; 	// Bitmask for GPIO J Pin 5 to Output mode
    GPIOJ->BSRR = (uint16_t)GPIO_PIN_5; 		// Turn on Green LED (LED2)
    GPIOJ->BSRR = (uint32_t)GPIO_PIN_5 << 16; 	// Turn off Green LED (LED2)
    GPIOJ->ODR ^= (uint16_t)GPIO_PIN_5; 		// Toggle LED2

    HAL_Delay(1000);

    char inputChar;

    printf("\0333[44;33m"); // blue background and yellow characters
    fflush();
    printf("\033[2;19H"); 	// center instruction text on line 2
    fflush();
    printf("Enter <ESC> or <CTRL> + [ to terminate\r\n\n");
    printf("\033[4;1H");
    fflush();

    while(1)
    {

    	inputChar = getchar();

    	if (inputChar == ESC_KEY) { // terminate program
    		printf("program terminated.\r\n\n");
    		return 1;
    	} else if (inputChar >= 32 && inputChar <= 126) { // printable characters
    		printf("The keyboard character is %c.\r\n\n", inputChar);
    	} else {
    		printf("The received value is not printable\r\n\n");
    	}

    }
}
