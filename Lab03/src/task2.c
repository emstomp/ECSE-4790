//------------------------------------
// Lab 3 - Part 1: UART - Lab03_uart.c
//------------------------------------
//

#include "init.h"

uint8_t UART1_rxBuffer[1] = {0};	// rx character buffer for UART1
uint8_t UART6_rxBuffer[1] = {0};	// rx character buffer for UART6
uint8_t UART1_rxLog[2] = {0};		// records last two character inputs from UART1
uint8_t UART6_rxLog[2] = {0};		// records last two character inputs from UART6
uint8_t halt_program = 0;			// 1: halt  	0: continue

UART_HandleTypeDef huart6 = {};

void initGPIO();
void USART1_IRQHandler();
void USART6_IRQHandler();
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void command(char c, UART_HandleTypeDef *huart);

// main Execution Loop
int main(void) {

	// initialize the system
	Sys_Init();

	// initialize GPIO
	initGPIO();

	// enable NVIC interrupts
	HAL_NVIC_EnableIRQ(USART1_IRQn);
	HAL_NVIC_EnableIRQ(USART6_IRQn);

	// initialize USART1 with 115200 baud rate
	initUart(&USB_UART, 115200, USART1);

	// initialize USART6 with 38400 baud rate
	initUart(&huart6, 38400, USART6);

	// enable RX interrupt
	HAL_UART_Receive_IT(&USB_UART, UART1_rxBuffer, 1);
	HAL_UART_Receive_IT(&huart6, UART6_rxBuffer, 1);

	while (1) {

		if (halt_program) {	// halt program
			HAL_NVIC_DisableIRQ(USART1_IRQn); // disable interrupts
			HAL_NVIC_DisableIRQ(USART6_IRQn);
			char exit_str[] = "\r\n\n\t- Exit. \r\n\n";
			uart_print(&USB_UART, exit_str);
			while (1) {};
		}

	}
}

// initialize GPIO
void initGPIO() {

	// LD3 ==> D13, A12
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

// handle USB/UART Interrupts with HAL
void USART1_IRQHandler() {
	HAL_UART_IRQHandler(&USB_UART);
}

void USART6_IRQHandler() {
	HAL_UART_IRQHandler(&huart6);
}

// UART RX callback function
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	char nl[] = "\r\n";

	if (huart->Instance == USART1) {

		if (UART1_rxBuffer[0] != '#' && UART1_rxLog[1] != '#') {
			uart_print(&USB_UART, (char*) UART1_rxBuffer); 	// print out character to USART1
		}
		uart_print(&huart6, (char*) UART1_rxBuffer); 	// print out character to USART6

		if (UART1_rxBuffer[0] == 27) {	// halt program
			halt_program = 1;
			return;
		}

		// log character
		UART1_rxLog[0] = UART1_rxLog[1];
		UART1_rxLog[1] = UART1_rxBuffer[0];

		if (UART1_rxBuffer[0] != '#') {
			// print new lines
			uart_print(&USB_UART, nl);
		}

		// start of command
		if (UART1_rxLog[0] == '#') {
			command(UART1_rxLog[1], huart);
		}

		// re-enable RX interrupt
		HAL_UART_Receive_IT(&USB_UART, UART1_rxBuffer, 1);

	} else if (huart->Instance == USART6) {

		if (UART6_rxBuffer[0] != '#' && UART6_rxLog[1] != '#') {
			uart_print(&USB_UART, (char*) UART6_rxBuffer); 	// print out character to UART1
		}

		if (UART6_rxBuffer[0] == 27) {	// halt program
			halt_program = 1;
			return;
		}

		// log character
		UART6_rxLog[0] = UART6_rxLog[1];
		UART6_rxLog[1] = UART6_rxBuffer[0];

		if (UART6_rxBuffer[0] != '#') {
			// print new lines
			uart_print(&USB_UART, nl);

		}

		if (UART6_rxLog[0] == '#') {
			command(UART6_rxLog[1], huart);
		}

		// re-enable RX interrupt
		HAL_UART_Receive_IT(&huart6, UART6_rxBuffer, 1);

	}

}

// terminal control commands
void command(char c, UART_HandleTypeDef *huart) {
	HAL_NVIC_DisableIRQ(USART1_IRQn);	// disable interrupts
	switch (c) {
		case 'c':	// clear terminal
			char clr_terminal_str[] = "\033[2J\033[H";
			uart_print(&USB_UART, clr_terminal_str);
			break;
		case 'e':	// end program
			halt_program = 1;
			// char esc_char = 27;
			// uart_putchar(&huart6, &esc_char); // send <ESC> to USART6
			return;
			break;
		case 'i':	// toggle LED LD3
			if (huart->Instance == USART6) {
				HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
			}
			break;
	}
	HAL_NVIC_EnableIRQ(USART1_IRQn);	// enable interrupt
}




