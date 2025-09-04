//------------------------------------------------------------------------------------
// Hello.c
//------------------------------------------------------------------------------------
//
// Test program to demonstrate serial port I/O.  This program writes a message on
// the console using the printf() function, and reads characters using the getchar()
// function.  An ANSI escape sequence is used to clear the screen if a '2' is typed.
// A '1' repeats the message and the program responds to other input characters with
// an appropriate message.
//
// Any valid keystroke turns on the green LED on the board; invalid entries turn it off
//


//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include "stm32f769xx.h"
#include "hello.h"

#include <stdint.h>

#define ESC_KEY 27

char inputChar;
//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
int main(void)
{
    Sys_Init(); // This always goes at the top of main (defined in init.c)

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    fflush(stdout); // By default, the print buffer (stdout) is "LINE BUFFERED", that is
                    // it only prints when a line is complete, usually done by adding '\n' to the end.
                    // A partial line (without termination in a '\n') may be force to print using this command.
                    // For other labs, we will change the stdout behavior to print immediately after
                    // ANY printf() call, not just ones that contain a '\n'.


    // Need to enable clock for peripheral bus on GPIO Port J
    __HAL_RCC_GPIOJ_CLK_ENABLE(); 	// Through HAL
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN; // or through registers
    //// Below two lines are example on how to access a register by knowing it's memory address
    //volatile uint32_t * RCC_AHB1_CLOCK_ENABLE = (uint32_t*) 0x40023830U; // Enable clock for peripheral bus on GPIO Port J
    //*RCC_AHB1_CLOCK_ENABLE |= 512U; // Bitmask for RCC AHB1 initialization: 0x00000200U or 512U in decimal

    GPIOJ->MODER |= 1024U; //Bitmask for GPIO J Pin 5 initialization (set it to Output mode): 0x00000400U or 1024U in decimal
    GPIOJ->BSRR = (uint16_t)GPIO_PIN_5; // Turn on Green LED (LED2)
    GPIOJ->BSRR = (uint32_t)GPIO_PIN_5 << 16; // Turn off Green LED (LED2)
    GPIOJ->ODR ^= (uint16_t)GPIO_PIN_5; // Toggle LED2

// It doesn't get lower level than this!


    //// This is an example of how to create a pointer to access a GPIO register.
    //// It is strongly recommended to follow the register access above (GPIOJ->...) and not this!!
    //// Access GPIOJ MODER register
    // volatile uint32_t * GREENLEDMODER = (uint32_t*) 0x40022400U; // Init GPIO J Pin 5 (LED2 with no Alt. func.) to Output
    // *GREENLEDMODER |= 1024U; // Bitmask for GPIO J Pin 5 initialization: 0x00000400U or 1024U in decimal
//    // Access GPIOJ BSRR register
//     volatile uint32_t * GREENLEDBSRR = (uint32_t*) 0x40022418U; // Address of GPIO J Bit Set/Reset Register
//     *GREENLEDBSRR = (uint16_t)0x0020U; // Turn on Green LED (LED2)

    HAL_Delay(1000); // Pause for a second. This function blocks the program and uses the SysTick and
                     // associated handler to keep track of time.

//    volatile uint32_t * GREENLEDODR = (uint32_t*) 0x40022414U; // Address of GPIO J Output Data Register
//    *GREENLEDODR ^= (uint16_t)0x0020U; // Toggle Green LED (LED2)

    printf("\0333[44;33m"); // blue background and yellow characters
    fflush();
    // TODO: center instruction text on line 2. cols: 80, rows: 24
    printf("\033[2;19H");
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

    	/** // Hello world
    	 * printf("Hello World!\r\n\n");
    	 * printf("( Welcome to Microprocessor Systems )\r\n\n\n");
    	 * printf("1=repeat, 2=clear, 0=quit.\r\n\n"); // Menu of choices

    	 * choice = getchar();
    	 * putchar(choice);

    	 * // select which option to run
    	 * // *GREENLEDBSRR = (uint16_t)0x0020U; // Turn on Green LED (LED2)
    	 * GPIOJ->BSRR = (uint16_t)0x0020U; // Turn on Green LED (LED2)
    	 * // HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_5, GPIO_PIN_SET);         // Turn green LED on (GPIO_PIN_SET == 1)
    	 * if (choice == '0')
         *   return 1;
         * else if(choice == '1')
         *   printf("\r\n\nHere we go again.\r\n\n");
         * else if(choice == '2')          // clear the screen with <ESC>[2J
         * {
         *   printf("\033[2J\033[;H");
         *   fflush(stdout);
         * }
         * else
         * {
         *   // Turn OFF LED to indicate incorrect selection
         *   // *GREENLEDBSRR = (uint32_t)0x0020U << 16; // Turn off Green LED (LED2)
         *	GPIOJ->BSRR = (uint16_t)0x0020U << 16; // Turn on Green LED (LED2)
         *   printf("\r\nA \"");
         *	putchar(choice);
         *   printf("\" is not a valid choice.\r\n\n");
         * }
    	 **/

    }
}

