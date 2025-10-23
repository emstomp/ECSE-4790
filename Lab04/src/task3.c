//--------------------------------
// Lab 4 - Sample - Lab04_sample.c
//--------------------------------
//
// Task 3: Simple Assembly Math and Logic
//

#include "init.h"

void task_set_1();
void task_set_2();

// Main Execution Loop
int main(void)
{
	//Initialize the system
	Sys_Init();

	task_set_1();
	// task_set_2();

}

void task_set_1() {

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

	// 4. evaluate equation (2x/3) + 5  = (2x + 15)/3 with MAC commands
	int32_t res4;
	int32_t c = 15;
	asm("MLA %[dest], %[r1], %[r2], %[r3]"	// 2x + 15
			: [dest] "=r" (res4)
			: [r1] "r" (x), [r2] "r" (multiplier), [r3] "r" (c));
	int32_t res5;
	asm("SDIV %[out], %[in1], %[in2]"	// res5 = res4 / 3
			: [out] "=r" (res5)
			: [in1] "r" (res4), [in2] "r" (divisor));
	printf("task set 1.4: %ld \r\n\n", res5);

}

void task_set_2() {

	// 1. load and add two integer numbers
	asm("LDR r2, =0x00000156");	// load 342 into r2
	asm("LDR r3, =0x00000064");	// load 100 into r3
	asm("ADD r4, r2, r3"); 		// add r2 and r3, store into r4
	int32_t var;
	asm("STR r4, %0" : "=m" (var));		// var = r4
	printf("task set 2.1: %ld \r\n\n", var);

	// 2. multiply 2 single precision floats
	float f1 = 1.5;
	float f2 = 4.6;
	float mres;
	asm("FMUL %[out], %[in1], %[in2]"	// mres = f1 * f2
			: [out] "=w" (mres)
			: [in1] "w" (f1), [in2] "w" (f2));
	printf("task set 2.2: %f\r\n\n", mres);

	// 3. evaluate equation (2x/3) + 5
	float x;
	printf("Enter x: ");
	scanf("%f", &x);

	float res1;
	float multiplier = 2.0;
	asm("VMUL %[out], %[], %[]"		// res1 = x * 2.0
			: [out] "=w" (res1)
			: [in1] "w" (x), [in2] "w" (multiplier));
	float res2;
	float divisor = 3.0;
	asm("VDIV %[out], %[in1], %[in2]"	// res2 = res1 / 3.0
			: [out] "=w" (res2)
			: [in1] "w" (res1), [in2] "w" (divisor));
	float res3;
	float c = 5.0;
	asm("VADD %[out], %[in1] %[in2]"	// res3 = res2 + 5.0
			: [out] "=w" (res3)
			: [in1] "w" (res2), [in2] "w" (c));
	printf("task set 2.3: %f\r\n\n", res3);

	// 4. evaluate equation (2x/3) + 5  = (2x + 15)/3 with MAC commands
	float res4 = 15.0;
	asm volatile ("VMLA.F32 %[out], %[in1], %[in2]"	// res4 = 2.0 * x + 15.0
			: [out] "+&w" (res4)
			: [in1] "w" (x) , [in2] "w" (multiplier));
	float res5;
	asm("VDIV %[out], %[in1], %[in2]"	// res5 = res4 / 3.0
			: [out] "=w" (res5)
			: [in1] "w" (res4), [in2] "w" (divisor));
	printf("task set 2.4: %f\r\n\n", res5);

}

