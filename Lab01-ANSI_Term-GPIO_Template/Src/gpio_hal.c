//------------------------------------------------------------------------------------
// gpio_hal.c
//------------------------------------------------------------------------------------
//
// Task 3 - GPIO using HAL
//
//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include "stm32f769xx.h"
#include "hello.h"
#include <stdint.h>

void GPIOrun();

//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
int main(void)
{
    Sys_Init();

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    fflush(stdout);

    HAL_Delay(1000);

    __HAL_RCC_GPIOA_CLK_ENABLE();  // Enable clock for GPIOA
    __HAL_RCC_GPIOD_CLK_ENABLE();  // Enable clock for GPIOD
    __HAL_RCC_GPIOC_CLK_ENABLE();  // Enable clock for GPIOC
    __HAL_RCC_GPIOF_CLK_ENABLE();  // Enable clock for GPIOF
    __HAL_RCC_GPIOJ_CLK_ENABLE();  // Enable clock for GPIOJ

    // Configure pins to input
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // Configure pins to output
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);	// PJ13

    GPIO_InitStruct.Pin  = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct); // PJ5

    GPIO_InitStruct.Pin  = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);	//PA12

    GPIO_InitStruct.Pin  = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); //PD4

    while(1)
    {

    	GPIOrun();
    }

}

//------------------------------------------------------------------------------------
// Function to handle GPIO logic
//------------------------------------------------------------------------------------
void GPIOrun(){

	uint8_t state_PC6 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6);  // read PC6
	uint8_t state_PC7 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7);  // read PC7
	uint8_t state_PJ1 = HAL_GPIO_ReadPin(GPIOJ, GPIO_PIN_1);  // read PJ1
	uint8_t state_PF6 = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_6);  // read PF6

	if (state_PC7) { 	// Turn on LED1
		HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_13, GPIO_PIN_SET);
	} else {			// Turn off LED1
		HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_13, GPIO_PIN_RESET);
	}

	if (state_PC6) {	// Turn on LED2
		HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_5, GPIO_PIN_SET);
	} else {			// Turn off LED2
		HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_5, GPIO_PIN_RESET);
	}

    if (state_PF6) {	// Turn on LED3
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    }else{				// Turn off LED3
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    }

    if (state_PJ1) {	// Turn on LED4
    	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
    } else {			// Turn off LED4
    	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);
    }
}
