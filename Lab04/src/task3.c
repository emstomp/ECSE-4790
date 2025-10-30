//--------------------------------
// Lab 4 - Sample - Lab04_sample.c
//--------------------------------
//
// Task 3: Simple Assembly Math and Logic
//

#include <math.h>
#include "init.h"

void task_set_1();
void task_set_2();
int numDigits(int num);

// Main Execution Loop
int main(void)
{
	//Initialize the system
	Sys_Init();

	printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
	fflush(stdout);

	task_set_1();
	task_set_2();

}

void task_set_1() {

	printf("\n\n");

	// 1. load and add two integer numbers
	asm("LDR r2, =0x000004B0");	// load 1200 into r2
	asm("LDR r3, =0x00000019");	// load 25 into r3
	asm("ADD r4, r2, r3"); 		// add r2 and r3, store into r4
	int32_t var;
	asm("STR r4, %0" : "=m" (var));		// var = r4
	printf("task set 1.1: %ld \r\n\n", var);

	// 2. multiply 2 int32_t variables
	int32_t m1 = 4;
	int32_t m2 = 25;
	int32_t mres;
	asm("MUL %[out], %[in1], %[in2]"	// mres = m1 * m2
			: [out] "=r" (mres)
			: [in1] "r" (m1), [in2] "r" (m2));
	printf("task set 1.2: %ld \r\n\n", mres);

	// 3. evaluate equation (2x/3) + 5
	int32_t x;
	printf("Enter x: ");
	scanf("%ld", &x);
	printf("%ld\r\n\n", x);

	int32_t res1;
	int32_t multiplier = 2;
	asm("MUL %[out], %[in1], %[in2]"	// res1 = 2x
			: [out] "=r" (res1)
			: [in1] "r" (x), [in2] "r" (multiplier));
	int32_t res2;
	int32_t divisor = 3;
	asm("SDIV %[out], %[in1], %[in2]"	// res2 = res1 / 3
			: [out] "=r" (res2)
			: [in1] "r" (res1), [in2] "r" (divisor));
	int32_t res3;
	asm("ADD %[out], %[in1], #5"		// res3 = res2 + 5
			: [out] "=r" (res3)
			: [in1] "r" (res2));
	printf("task set 1.3: %ld \r\n\n", res3);

	// 4. evaluate equation (2x/3) + 5 = (2x + 15)/3 with MAC commands
	int32_t res4;
	int32_t c = 15;
	asm("MLA %[dest], %[r1], %[r2], %[r3]"	// res4 = 2x + 15
			: [dest] "=r" (res4)
			: [r1] "r" (x), [r2] "r" (multiplier), [r3] "r" (c));
	int32_t res5;
	asm("SDIV %[out], %[in1], %[in2]"		// res5 = res4 / 3
			: [out] "=r" (res5)
			: [in1] "r" (res4), [in2] "r" (divisor));
	printf("task set 1.4: %ld \r\n\n", res5);

}

void task_set_2() {

	printf("\n\n");

	// 1. load and add two integer numbers
	asm("LDR r2, =0x00000156");	// load 342 into r2
	asm("LDR r3, =0x00000064");	// load 100 into r3
	asm("ADD r4, r2, r3"); 		// add r2 and r3, store into r4
	int32_t var;
	asm("STR r4, %0" : "=m" (var));		// var = r4
	printf("task set 2.1: %ld \r\n\n", var);

	// 2. multiply 2 single precision floats
	float f1 = 1.5;
	float f2 = 2.6;
	float mres;
	asm("VMUL.F32 %[out], %[in1], %[in2]"	// mres = f1 * f2
			: [out] "=t" (mres)
			: [in1] "t" (f1), [in2] "t" (f2));
	printf("task set 2.2: %f\r\n\n", (double) mres);

	// 3. evaluate equation (2x/3) + 5
	int whole;			// numbers left of decimal point
	printf("Enter x: ");
	scanf("%d", &whole);
	printf("%d.", whole);
	int fract;			// numbers right of decimal point
	scanf("%d", &fract);
	printf("%d = ", fract);

	int exponent = -1 * numDigits(fract);
	float x = whole + (fract * ( pow(10, exponent) ));	// x = whole.fract
	printf("%f \r\n\n", (double) x);

	float res1;
	float multiplier = 2.0;
	asm("VMUL.F32 %[out], %[in1], %[in2]"	// res1 = x * 2.0
			: [out] "=t" (res1)
			: [in1] "t" (x), [in2] "t" (multiplier));
	float res2;
	float divisor = 3.0;
	asm("VDIV.F32 %[out], %[in1], %[in2]"	// res2 = res1 / 3.0
			: [out] "=t" (res2)
			: [in1] "t" (res1), [in2] "t" (divisor));
	float res3;
	float c = 5.0;
	asm("VADD.F32 %[out], %[in1], %[in2]"	// res3 = res2 + 5.0
			: [out] "=t" (res3)
			: [in1] "t" (res2), [in2] "t" (c));
	printf("task set 2.3: %f\r\n\n", (double) res3);

	// 4. evaluate equation (2x/3) + 5 with MAC commands
	float a = 2.0;
	float b = 3.0;
	float f;
	asm("VDIV.F32 %[out], %[in1], %[in2]"	// f = a/b = 2.0/3.0
				: [out] "=t" (f)
				: [in1] "t" (a), [in2] "t" (b));
	float res4 = 5.0;
	asm volatile ("VMLA.F32 %[out], %[in1], %[in2]"	// res4 = 2.0/3.0 * x + 5
				: [out] "+&t" (res4)
				: [in1] "t" (x) , [in2] "t" (f));
	printf("task set 2.4: %f\r\n\n", (double) res4);

}

// returns number of digits in int
int numDigits(int num) {
	int digits = 0;
	if (num == 0) digits = 1;        // 0 has one digit
	    else {
	        int n = num < 0 ? -num : num; // handle negative
	        while (n > 0) {
	            n /= 10;
	            digits++;
	        }
	    }

	return digits;
}


