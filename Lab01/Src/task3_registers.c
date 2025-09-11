//------------------------------------------------------------------------------------
// Hello.c
//------------------------------------------------------------------------------------
//
// Test program to demonstrate serial port I/O. This program writes a message on
// the console using the printf() function, and reads characters using the getchar()
// function. An ANSI escape sequence is used to clear the screen if a '2' is typed.
// A '1' repeats the message and the program responds to other input characters with
// an appropriate message.
//
// Any valid keystroke turns on the green LED on the board; invalid entries turn it off
//

//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include "stm32f769xx.h" // STM32 HAL header
#include "hello.h"       // Custom header for this program
#include <stdint.h>       // Fixed-width integer types

//------------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------------
uint8_t fault = 0; // Indicates if a non-printable character was received
#define ESC_KEY 27 // ASCII value for the Escape key

void drawScreen(); // Function prototype for drawing the screen
void GPIOrun();    // Function prototype for GPIO logic

// Buffer to store characters for display (10 rows, 72 columns + null terminator)
char arr[10][73];
uint16_t id = 0;          // Current index in the buffer
uint16_t PrintChar = 0;   // Count of printable characters received
uint16_t NotPrintChar = 0; // Count of non-printable characters received

// List of non-printable ASCII characters
char badList[22] = {127, 9, 12, 13, 16, 17, 18, 19, 20, 33, 34, 35, 36, 37, 38, 39, 40};

char inputChar; // Variable to store the current input character

//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
int main(void)
{
    Sys_Init(); // Initialize the system (defined in init.c)

    printf("\033[2J\033[;H"); // Clear the screen and move the cursor to the home position

    fflush(stdout); // Ensure the output buffer is flushed

    HAL_Delay(1000); // Pause for a second

    // Enable the clock for GPIO ports
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;  // Enable GPIOC clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN;  // Enable GPIOJ clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;  // Enable GPIOF clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN   // Enable GPIOA clock
                   | RCC_AHB1ENR_GPIODEN; // Enable GPIOD clock

    // Configure GPIO pins as input or output
    GPIOC->MODER &= ~((3U << (6 * 2)) | (3U << (7 * 2))); // PC6, PC7 as input
    GPIOJ->MODER &= ~(3U << (1 * 2));                    // PJ1 as input
    GPIOF->MODER &= ~(3U << (6 * 2));                    // PF6 as input

    // Enable pull-up resistors for input pins
    GPIOC->PUPDR &= ~((3U << (6 * 2)) | (3U << (7 * 2))); // Clear pull-up/down for PC6, PC7
    GPIOC->PUPDR |= ((1U << (6 * 2)) | (1U << (7 * 2)));  // Set pull-up for PC6, PC7
    GPIOJ->PUPDR &= ~(3U << (1 * 2));                    // Clear pull-up/down for PJ1
    GPIOJ->PUPDR |= (1U << (1 * 2));                     // Set pull-up for PJ1
    GPIOF->PUPDR &= ~(3U << (6 * 2));                    // Clear pull-up/down for PF6
    GPIOF->PUPDR |= (1U << (6 * 2));                     // Set pull-up for PF6

    // Configure output pins
    GPIOJ->MODER &= ~((3U << (13 * 2)) | (3U << (5 * 2))); // Clear PJ13, PJ5
    GPIOJ->MODER |= ((1U << (13 * 2)) | (1U << (5 * 2)));  // Set PJ13, PJ5 as output
    GPIOA->MODER &= ~(3U << (12 * 2));                    // Clear PA12
    GPIOA->MODER |= (1U << (12 * 2));                     // Set PA12 as output
    GPIOD->MODER &= ~(3U << (4 * 2));                     // Clear PD4
    GPIOD->MODER |= (1U << (4 * 2));                      // Set PD4 as output

    // Initialize the display buffer with '.' characters
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 72; j++) {
            arr[i][j] = '.';
        }
        arr[i][72] = '\0'; // Null terminator for each row
    }

    while (1) {
        drawScreen(); // Update the screen
        GPIOrun();    // Run GPIO logic

        // Clear Overrun Error if it occurred
        if (USART1->ISR & USART_ISR_ORE) {
            volatile char dump = (char)USART1->RDR; // Read RDR to clear the error
            (void)dump;
            USART1->ICR |= USART_ICR_ORECF; // Clear the overrun flag
        }

        if ((inputChar = 0) != EOF) {//if you want to run the terminal thing put getChar instead of 0, get char blocks the GPIO from updating and I am not doing interups for ts
            if (inputChar == ESC_KEY) {
                break; // Exit the loop if the Escape key is pressed
            }

            uint8_t good = 1; // Flag to check if the character is printable
            for (int k = 0; k < 22; k++) {
                if (inputChar == badList[k]) {
                    good = 0; // Mark as non-printable
                    break;
                }
            }

            if (good == 1) {
                fault = 0; // Reset fault flag
                PrintChar++; // Increment printable character count

                if (id >= 720) {
                    id = id - 73; // Adjust index to prevent overflow

                    // Shift rows up to make space for new data
                    for (int i = 0; i < 9; i++) {
                        for (int j = 0; j < 72; j++) {
                            arr[i][j] = arr[i + 1][j];
                        }
                    }

                    // Clear the last row
                    for (int j = 0; j < 72; j++) {
                        arr[9][j] = '.'; // ASCII for period
                    }
                } else {
                    // Save the character in the buffer
                    arr[id / 72][id % 72] = inputChar;
                }

                id++; // Increment the buffer index
            } else {
                fault = 1; // Set fault flag
                NotPrintChar++; // Increment non-printable character count
                drawScreen(); // Update the screen
            }
        }
    }
}

//------------------------------------------------------------------------------------
// Function to handle GPIO logic
//------------------------------------------------------------------------------------
void GPIOrun() {
    uint8_t state_PC6 = (GPIOC->IDR >> 6) & 0x1; // Read PC6
    uint8_t state_PC7 = (GPIOC->IDR >> 7) & 0x1; // Read PC7
    uint8_t state_PJ1 = (GPIOJ->IDR >> 1) & 0x1; // Read PJ1
    uint8_t state_PF6 = (GPIOF->IDR >> 6) & 0x1; // Read PF6

    if (state_PC6) {
        GPIOJ->BSRR = (1U << 13); // Turn on LED1
    } else {
        GPIOJ->BSRR = (1U << (13 + 16)); // Turn off LED1
    }

    if (state_PC7) {
        GPIOJ->BSRR = (1U << 5); // Turn on LED2
    } else {
        GPIOJ->BSRR = (1U << (5 + 16)); // Turn off LED2
    }

    if (state_PJ1) {
        GPIOA->BSRR = (1U << 12); // Turn on PA12
    } else {
        GPIOA->BSRR = (1U << (12 + 16)); // Turn off PA12
    }

    if (state_PF6) { // Logic for this one is inverted
        GPIOD->BSRR = (1U << (4 + 16)); // Turn off PD4
    } else {
        GPIOD->BSRR = (1U << 4); // Turn on PD4
    }
}

//------------------------------------------------------------------------------------
// Function to draw the screen
//------------------------------------------------------------------------------------
void drawScreen() {
    printf("\033[?25l"); // Hide the cursor
    printf("\033[38;5;220m"); // Set foreground color
    printf("\033[48;5;24m"); // Set background color
    printf("\033[2;19H");
    printf("Enter <ESC> or <CTRL> + [ to terminate\r\n\n");

    // Print a line of dashes to mark the start of the printable zone
    printf("\033[3;0H");
    for (int i = 0; i < 80; i++) {
        printf("-");
    }

    // Print the buffer content
    for (int i = 0; i < 10; i++) {
        printf("\033[%d;5H", i + 4); // Move to row i+4, column 5
        printf("%.*s", 72, arr[i]); // Print 72 characters from the row
    }

    // Print a line of dashes to mark the end of the printable zone
    printf("\033[14;0H");
    for (int i = 0; i < 80; i++) {
        printf("-");
    }

    if (fault == 1) {
        // Display error message for non-printable character
        printf("\033[16;0H");
        printf("\033[38;5;196m"); // Set foreground color to red
        printf("The received value $%x is ’not printable’ ", inputChar);
        printf("\033[38;5;220m"); // Reset foreground color

        fflush(stdout);
        HAL_Delay(100);
        printf("\033[16;0H");
        printf("\033[48;5;196m"); // Set background color to red
        printf("The received value $%x is ’not printable’ ", inputChar);
        fflush(stdout);
        HAL_Delay(100);
        printf("\033[16;0H");
        printf("The received value $%x is ’not printable’ ", inputChar);
        printf("\033[48;5;24m"); // Reset background color
        fflush(stdout);
    } else {
        // Clear the error message area
        printf("\033[16;0H");
        for (int i = 0; i < 80; i++) {
            printf(" ");
        }
    }

    // Display character counts
    printf("\033[21;0H");
    printf("# of Characters Received:");
    printf("\033[22;0H");
    printf("Printable             Non-Printable");
    printf("\033[23;0H");
    printf("%d", PrintChar);
    printf("\033[23;22H");
    printf("%d", NotPrintChar);

    printf("\033[?25h"); // Show the cursor
    fflush(stdout);
}
