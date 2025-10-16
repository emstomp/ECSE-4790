//------------------------------------
// Lab 3 - Part 1: UART - Lab03_uart.c
//------------------------------------
//

#include "init.h"

void halt_program(char c);

UART_HandleTypeDef huart6 = {};

// Main Execution Loop
int main(void) {

	// Initialize the system
	Sys_Init();

	// Initialize USART1 with 115200 baud rate
	initUart(&USB_UART, 115200, USART1);

	// Initialize USART6 with 38400 baud rate
	initUart(&huart6, 38400, USART6);

	char c1[1];
	char c6[1];

	while (1) {

		// receive character from USART1
		if (HAL_UART_Receive(&USB_UART, (uint8_t *)c1, 1, 10) == HAL_OK) {
			uart_putchar(&USB_UART, c1);	// send character to USART1
			uart_putchar(&huart6, c1);		// send character to USART6
			halt_program(c1[0]);			// check if character == <ESC>
		}

		// receive character from USART6
		if (HAL_UART_Receive(&huart6, (uint8_t *)c6, 1, 10) == HAL_OK) {
			uart_putchar(&USB_UART, c6);	// send character to USART1
			halt_program(c6[0]);			// check if character == <ESC>
		}

	}

}

void halt_program(char c) {
	if (c == 27) {
		char exit_str[] = "\r\n\n\t1 Exit. \r\n\n";
		uart_print(&USB_UART, exit_str);
		while (1) {}	// halt program
	}
}


