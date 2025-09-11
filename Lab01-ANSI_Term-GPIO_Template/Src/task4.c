//------------------------------------------------------------------------------------
// maze.c
//------------------------------------------------------------------------------------
//
// Task 4: [Depth] Maze
//
//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include "stm32f769xx.h"
#include "hello.h"
#include <stdint.h>

void drawMaze();
void setMazeArray();
int reachedExit(uint8_t posx, uint8_t posy);

//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
int main(void)
{
    Sys_Init();

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    fflush(stdout);

    HAL_Delay(1000);

    __HAL_RCC_GPIOJ_CLK_ENABLE();				// Enable clock for peripheral bus on GPIO Port J
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOJEN;
	GPIOJ->MODER |= 1024U; 						// GPIO J Pin 5 initialization (Output mode)
	GPIOJ->BSRR = (uint16_t)GPIO_PIN_5; 		// Turn on Green LED (LED2)
	GPIOJ->BSRR = (uint32_t)GPIO_PIN_5 << 16;	// Turn off Green LED (LED2)

	// enable blue button ==> PA0
	__HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // player's starting x and y positions
    // measured from the top left of the maze
	uint8_t posx = 1;
	uint8_t posy = 1;

	char inputChar;
	uint8_t blueButton;

	// stores 10x10 maze with an extra 2 rows and 2 columns
	// for the top, bottom, left, and right side of the maze
	// represented by 0's and 1's. 0 ==> no wall, 1 ==> wall
	int mazeArr[12][12];

	drawMaze();
	setMazeArray(mazeArr);

	printf("\033[?25l");
    printf("\033[5;20H");		// place cursor at beginning of maze
	printf("\033[38;5;220m");	// foreground - white
	printf("O");				// print player character
    printf("\033[5;20H");
	fflush(stdout);

    while(1)
    {

    	inputChar = getchar();

    	// check if blue button is pushed
    	blueButton = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);  // read PE5
    	if (blueButton) {
    		// reset maze and place player back at the start
    	    printf("\033[2J\033[;H"); 	// Erase screen & move cursor to home position
    	    drawMaze();
    	    printf("\033[?25l");
    	    printf("\033[5;20H");		// place cursor at beginning of maze
    		printf("\033[38;5;220m");	// foreground - white
    		printf("O");				// print player character
    	    printf("\033[5;20H");
    		fflush(stdout);
    		posx = 1;
    		posy = 1;
    		continue;
    	}

    	// move the player if keys w, a, s, or d are pressed
    	switch (inputChar) {
			case 119: // w
				posy--;
				if (reachedExit(posx, posy)) return 0;;
				if (mazeArr[posy][posx]) {
					// wall collision
					posy++; // backtrack
				} else {
					// move up
					printf("\033[%d;%dH", posy+5, posx+19); // delete previous character
					printf(" ");
					printf("\033[%d;%dH", posy+4, posx+19); // move player's character up
					printf("O");
					fflush(stdout);
				}
				break;
			case 97: // a
				posx--;
				if (reachedExit(posx, posy)) return 0;;
				if (mazeArr[posy][posx]) {
					// wall collision
					posx++; // backtrack
				} else {
					// move left
					printf("\033[%d;%dH", posy+4, posx+20); // delete previous character
					printf(" ");
					printf("\033[%d;%dH", posy+4, posx+19); // move player's character left
					printf("O");
					fflush(stdout);
				}
				break;
			case 115: // s
				posy++;
				if (reachedExit(posx, posy)) return 0;
				if (mazeArr[posy][posx]) {
					// wall collision
					posy--; // backtrack
				} else {
					// move down
					printf("\033[%d;%dH", posy+3, posx+19); // delete previous character
					printf(" ");
					printf("\033[%d;%dH", posy+4, posx+19); // move player's character down
					printf("O");
					fflush(stdout);
				}
				break;
			case 100: // d
				posx++;
				if (reachedExit(posx, posy)) return 0;
				if (mazeArr[posy][posx]) {
					// wall collision
					posx--; // backtrack
				} else {
					// move right
					printf("\033[%d;%dH", posy+4, posx+18); // delete previous character
					printf(" ");
					printf("\033[%d;%dH", posy+4, posx+19); // move player's character right
					printf("O");
					fflush(stdout);
				}
				break;
    	}
    }

    return 0;

}

void drawMaze() {

	printf("WASD to move\r\n");
	printf("Push blue button to reset\r\n\n");

	printf("\033[?25l"); 		// make the cursor invisible
	printf("\033[38;5;21m");	//foreground - blue
    printf("\033[48;5;0m");		//background - black

    // fill entire screen black
    for (int i = 0; i < 20; i++) {
    	for (int j = 0; j < 80; j++) {
    		printf(" ");
    	}
    }

    // print maze
    printf("\033[4;19H");
    printf("------------");
    printf("\033[5;19H");
    printf("|      |   |");
    printf("\033[6;19H");
    printf("| | || |   |");
    printf("\033[7;19H");
    printf("| |  | |   |");
    printf("\033[8;19H");
    printf("|  | |  || |");
    printf("\033[9;19H");
    printf("|| | ||  | |");
    printf("\033[10;19H");
    printf("|  |   |   |");
    printf("\033[11;19H");
    printf("| |||| |||||");
    printf("\033[12;19H");
    printf("|    | |   |");
    printf("\033[13;19H");
    printf("|||| | | | |");
    printf("\033[14;19H");
    printf("|    |   | |");
    printf("\033[15;19H");
    printf("---------- -");

    fflush(stdout);

}

void setMazeArray(int mazeArr[][12]) {

	// fill maze array with zeroes
	for (int i = 0; i < 12; i++) {
		for (int j = 0; j < 12; j++) {
			mazeArr[i][j] = 0;
		}
	}

	// place 1s where there are walls
	for (int i = 0; i < 12; i++) {
		// outer sides of the maze
		mazeArr[0][i] = 1;
		mazeArr[i][0] = 1;
		mazeArr[11][i] = 1;
		mazeArr[i][11] = 1;
	}

	// walls inside of the maze
	mazeArr[1][7] = 1;
	mazeArr[2][2] = 1;
	mazeArr[2][4] = 1;
	mazeArr[2][5] = 1;
	mazeArr[2][7] = 1;
	mazeArr[2][9] = 1;
	mazeArr[2][10] = 1;
	mazeArr[3][2] = 1;
	mazeArr[3][5] = 1;
	mazeArr[3][7] = 1;
	mazeArr[4][3] = 1;
	mazeArr[4][5] = 1;
	mazeArr[4][7] = 1;
	mazeArr[5][1] = 1;
	mazeArr[5][3] = 1;
	mazeArr[5][5] = 1;
	mazeArr[5][6] = 1;
	mazeArr[5][9] = 1;
	mazeArr[6][3] = 1;
	mazeArr[6][7] = 1;
	mazeArr[7][2] = 1;
	mazeArr[7][3] = 1;
	mazeArr[7][4] = 1;
	mazeArr[7][5] = 1;
	mazeArr[7][7] = 1;
	mazeArr[7][8] = 1;
	mazeArr[7][9] = 1;
	mazeArr[7][10] = 1;
	mazeArr[8][5] = 1;
	mazeArr[8][7] = 1;
	mazeArr[9][1] = 1;
	mazeArr[9][2] = 1;
	mazeArr[9][3] = 1;
	mazeArr[9][5] = 1;
	mazeArr[9][7] = 1;
	mazeArr[9][9] = 1;
	mazeArr[10][5] = 1;
	mazeArr[10][9] = 1;
}

// prints message and returns 1 if player has reached the exit
// return 0 if player has not yet reached the exit
int reachedExit(uint8_t posx, uint8_t posy) {

	if (posx == 10 && posy == 10) { // exit is at (10, 10) on the maze

    	GPIOJ->ODR ^= (uint16_t)GPIO_PIN_5; // turn on LED

    	printf("\033[16;0H");
    	printf("You've reached the exit!\r\n\n");
		printf("\033[%d;%dH", posy+3, posx+19); // delete previous character
		printf(" ");
		printf("\033[%d;%dH", posy+4, posx+19); // move character down
		printf("O");
    	fflush(stdout);

		return 1;
	}
	return 0;
}
