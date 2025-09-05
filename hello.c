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
void GPIOrun();
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
    // Enable clocks for ports C, J, F
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;  // GPIOC clock enable
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN;  // GPIOJ clock enable
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;  // GPIOF clock enable
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN  // PA
                   | RCC_AHB1ENR_GPIODEN; // PD
    // Set PC6, PC7 as input (bits 12-15 in MODER)
    GPIOC->MODER &= ~((3U << (6*2)) | (3U << (7*2)));
    // Set PJ1 as input
    GPIOJ->MODER &= ~(3U << (1*2));
    // Set PF6 as input
    GPIOF->MODER &= ~(3U << (6*2));

    // Pull-up for PC6, PC7
    GPIOC->PUPDR &= ~((3U << (6*2)) | (3U << (7*2)));  // clear bits first
    GPIOC->PUPDR |=  ((1U << (6*2)) | (1U << (7*2)));  // set to 01 = pull-up

    // Pull-up for PJ1
    GPIOJ->PUPDR &= ~(3U << (1*2));
    GPIOJ->PUPDR |=  (1U << (1*2));

    // Pull-up for PF6
    GPIOF->PUPDR &= ~(3U << (6*2));
    GPIOF->PUPDR |=  (1U << (6*2));


    // PJ13, PJ5 as outputs
    GPIOJ->MODER &= ~((3U << (13*2)) | (3U << (5*2))); // clear
    GPIOJ->MODER |=  ((1U << (13*2)) | (1U << (5*2))); // set as output

    // PA12 as output
    GPIOA->MODER &= ~(3U << (12*2));
    GPIOA->MODER |=  (1U << (12*2));

    // PD4 as output
    GPIOD->MODER &= ~(3U << (4*2));
    GPIOD->MODER |=  (1U << (4*2));





	for (int i = 0; i < 10; i++) {
	    for (int j = 0; j < 72; j++) {
	        arr[i][j] = '.';
	    }
	    arr[i][72] = '\0'; // null terminator
	}
    while(1)
    {

    	drawScreen();
    	GPIOrun();

    	//StackOverflow solution to the buffer overflowing, according to the guy who posted this "When I wrote this me and god knew how it works, now its just god"
    	// Clear Overrun Error if it happened
    	if (USART1->ISR & USART_ISR_ORE) {
    	    volatile char dump = (char)USART1->RDR;     // read RDR to clear
    	    (void)dump;
    	    USART1->ICR |= USART_ICR_ORECF;             // clear the overrun flag
    	}
    	if ((inputChar = getchar()) != EOF){

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

    	}}

    }
}

void GPIOrun(){

	uint8_t state_PC6 = (GPIOC->IDR >> 6) & 0x1;  // read PC6
	uint8_t state_PC7 = (GPIOC->IDR >> 7) & 0x1;  // read PC7
	uint8_t state_PJ1 = (GPIOJ->IDR >> 1) & 0x1;  // read PJ1
	uint8_t state_PF6 = (GPIOF->IDR >> 6) & 0x1;  // read PF6

	if(state_PC6){
    GPIOJ->BSRR = (1U << 13);  // LED1 on
	}else{GPIOJ->BSRR = (1U << (13+16));} // LED1 off


	if(state_PC7){
    GPIOJ->BSRR = (1U << 5);   // LED2 on
	}else{GPIOJ->BSRR = (1U << (5+16)); }// LED2 off


    if(state_PJ1){
    GPIOA->BSRR = (1U << 12);
    }else{GPIOA->BSRR = (1U << (12+16));}


    if(state_PF6){//logic for this one is inversed
	GPIOD->BSRR = (1U << (4+16));
    }else{GPIOD->BSRR = (1U << 4);}
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
    	printf("The received value $%x is ’not printable’ ",inputChar);
    	printf("\033[38;5;220m");//foreground normal

    	fflush(stdout);
    	HAL_Delay(100);
    	printf("\033[16;0H");
    	printf("\033[48;5;196m");//background
    	printf("The received value $%x is ’not printable’ ",inputChar);
    	fflush(stdout);
    	HAL_Delay(100);
    	printf("\033[16;0H");
    	printf("The received value $%x is ’not printable’ ",inputChar);
    	printf("\033[48;5;24m");//background
    	fflush(stdout);

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
