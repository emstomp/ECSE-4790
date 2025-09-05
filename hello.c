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

uint8_t fault =0 ;
#include <stdint.h>

#define ESC_KEY 27
void drawScreen();
char arr[10][72];
uint16_t id=0;
uint16_t PrintChar=0;
uint16_t NotPrintChar=0;
char badList[22]={127,9,12,13,16,17,18,19,20,33,34,35,36,37,38,39,40};
//8 - Backspace (127)
//9 - Tab
//12 - 5 in the numeric keypad when Num Lock is off
//13 - Enter
//16 - Shift
//17 - Ctrl
//18 - Alt
//19 - Pause/Break
//20 - Caps Lock
//27 - Esc
//32 - Space
//33 - Page Up
//34 - Page Down
//35 - End
//36 - Home
//37 - Left arrow
//38 - Up arrow
//39 - Right arrow
//40 - Down arrow
//44 - Print Screen
//45 - Insert
//46 - Delete

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


    HAL_Delay(1000); // Pause for a second. This function blocks the program and uses the SysTick and
    // Need to enable clock for peripheral bus on GPIO Port J
    __HAL_RCC_GPIOJ_CLK_ENABLE(); 	// Through HAL
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN; // or through registers
	GPIOJ->MODER |= 1024U; //Bitmask for GPIO J Pin 5 initialization (set it to Output mode): 0x00000400U or 1024U in decimal
	GPIOJ->BSRR = (uint16_t)GPIO_PIN_5; // Turn on Green LED (LED2)
	GPIOJ->BSRR = (uint32_t)GPIO_PIN_5 << 16; // Turn off Green LED (LED2)
	GPIOJ->ODR ^= (uint16_t)GPIO_PIN_5; // Toggle LED2
	for (int i = 0; i < 10; i++) {
	    for (int j = 0; j < 72; j++) {
	        arr[i][j] = '.';
	    }
	    arr[i][72] = '\0'; // null terminator
	}
    while(1)
    {

    	drawScreen();

    	//StackOverflow solution to the buffer overflowing, according to the guy who posted this "When I wrote this me and god knew how it works, now its just god"
    	// Clear Overrun Error if it happened
    	if (USART1->ISR & USART_ISR_ORE) {
    	    volatile char dump = (char)USART1->RDR;     // read RDR to clear
    	    (void)dump;
    	    USART1->ICR |= USART_ICR_ORECF;             // clear the overrun flag
    	}



    	inputChar = getchar();

    	if (inputChar == 27){
    		break;
    	}

    	uint8_t good = 1;
    	for(int k=0;k<22;k++){
    		if (inputChar == badList[k]){
    			good = 0;//Bools would be useful here but eh
    		}
    	}

    	if(good==1){
    		fault = 0;
    		PrintChar++;
			if (id >= 720){
				id = id - 73;
				for(int i=0;i<9;i++){
					for(int j=0;j<72;j++){
						arr[i][j] = arr[i+1][j];//move all the data one row down.

					};
				};
				for(int j=0;j<72;j++){
					arr[9][j] = 46;//46 is asci for period
				};
			}else{
				//save the character into the respective array
				arr[id/72][id%72]=inputChar;
			}
			id++;
    	}else{
    		fault =1;
    		NotPrintChar++;
    		drawScreen();

    	}

    }
}

void drawScreen(){ //idk why the pointer fixes it but stackoverflow knows better then me
	printf("\033[?25l"); // make the cursor invisible, just less jumpy
	printf("\033[38;5;220m");//foreground
    printf("\033[48;5;24m");//background
    printf("\033[2;19H");
    printf("Enter <ESC> or <CTRL> + [ to terminate\r\n\n");

    printf("\033[3;0H");
    for(int i =0;i<80;i++){// line of dashes to show the start of the printable zone
    	printf("-");
    }


    for (int i = 0; i < 10; i++) {
        printf("\033[%d;5H", i+4);          // move to row i+4, col 4
        printf("%.*s", 72, arr[i]);         // print 72 chars from arr[i] in one go
    }


    printf("\033[14;0H");
    for(int i =0;i<80;i++){	// line of dashes to show the end of the printable zone
        	printf("-");
        }


    if (fault == 1){
    	printf("\033[16;0H");
    	printf("\033[38;5;196m");//foreground red
    	printf("The received value $%x is ’not printable’",inputChar);
    	printf("\033[38;5;220m");//foreground normal
    }else{
    	printf("\033[16;0H");
		for(int i =0;i<80;i++){	// line of dashes to show the end of the printable zone
				printf(" ");
			}
    }


	printf("\033[21;0H");
	printf("# of Characters Received:");
	printf("\033[22;0H");
	printf("Printable             Non-Printable");
	printf("\033[23;0H");
	printf("%d",PrintChar);
	printf("\033[23;22H");
	printf("%d",NotPrintChar);





    printf("\033[?25h"); // make the cursor visible,
    fflush(stdout);

//    HAL_Delay(100);
}
