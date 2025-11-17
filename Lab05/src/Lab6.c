/**************************************************************
 * If not using B03 revision DISCO board, remove the predefined
 * symbol USE_STM32F769I_DISCO_REVB03 from the project properties:
 *    C/C++ Build -> Settings -> MCU GCC Compiler -> Preprocessor
 * This is only needed if using the LCD
 */


#include "stm32f769xx.h"
#include "stm32f7xx_hal.h"
#include "stm32f769i_discovery_lcd.h"

#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "jpeg_utils.h"

#include "init.h"
#include "helper_functions.h"

/* Defines */
#define LCD_FRAME_BUFFER        0xC0000000
#define JPEG_OUTPUT_DATA_BUFFER 0xC0200000

/* Simple buffer size for JPEG streaming */
#define JPEG_BUFFER_SIZE        4096U

/* Global Variables */
uint32_t MCU_TotalNb = 0;
volatile uint32_t num_bytes_decoded = 0;
uint8_t  input = 0x1B;

JPEG_HandleTypeDef   jpeg_handle;
JPEG_ConfTypeDef     jpeg_info;
DMA2D_HandleTypeDef  DMA2D_Handle;

/* FatFs globals */
FATFS   SDFatFs;
FIL     MyFile;
FIL     jpegFile;
char    SDPath[4];

/* JPEG streaming helpers */
uint8_t                JPEG_InputBuffer[JPEG_BUFFER_SIZE];
volatile uint8_t       JPEG_EOF             = 0;
volatile uint8_t       JpegDecodeFinished   = 0;

DMA_HandleTypeDef hdmaIn;
DMA_HandleTypeDef hdmaOut;

void DMA2D_CopyBuffer(uint32_t *pSrc, uint32_t *pDst, uint16_t x, uint16_t y, JPEG_ConfTypeDef *jpeg_info);

/* ==== Application code ====================================================*/

int main(void)
{
	Sys_Init();
	input = 0;   // make sure we actually draw in PuTTY

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    printf("\033c"); // Reset device
    fflush(stdout);

    /*
     * Leave this section even if you don't have an LCD
     */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_BLACK);

    /* ================= Task 1: SD card file browser ================= */
    FRESULT res;
    FATFS   SDFatFs;
    FIL     file;
    DIR     dir;
    FILINFO fno;
    char    SDPath[4];

    char fileNames[32][64];
    int  fileCount = 0;
    int  idx = 0;

    /* Link SD driver to FatFs */
    if (FATFS_LinkDriver(&SD_Driver, SDPath) != 0) {
        printf("FATFS_LinkDriver failed\r\n");
        while (1);
    }

    /* Mount the filesystem */
    res = f_mount(&SDFatFs, (TCHAR const *)SDPath, 1);
    if (res != FR_OK) {
        printf("f_mount error %d\r\n", res);
        while (1);
    }

    /* Open root directory and list files */
    res = f_opendir(&dir, "0:/");
    if (res != FR_OK) {
        printf("f_opendir error %d\r\n", res);
        while (1);
    }

    printf("Files on SD card (root directory):\r\n");
    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            break;          // end of directory
        }

        if (fno.fattrib & AM_DIR) {
            continue;       // skip directories
        }

        if (fileCount < 32) {
            snprintf(fileNames[fileCount],
                     sizeof(fileNames[fileCount]),
                     "%s", fno.fname);
            printf("%2d: %s\r\n", fileCount, fileNames[fileCount]);
            fileCount++;
        }
    }
    f_closedir(&dir);

    if (fileCount == 0) {
        printf("No files found on SD card.\r\n");
    } else {
        /* Let user choose a file */
        printf("\r\nEnter index of file to display: ");
        fflush(stdout);
        scanf("%d", &idx);

        if (idx < 0 || idx >= fileCount) {
            printf("\r\nInvalid index, defaulting to 0\r\n");
            idx = 0;
        }

        char path[80];
        snprintf(path, sizeof(path), "0:/%s", fileNames[idx]);
        printf("\r\nOpening %s\r\n", path);

        res = f_open(&file, path, FA_READ);
        if (res == FR_OK) {
            printf("\r\n----- Contents of %s -----\r\n", fileNames[idx]);
            char buffer[128];
            UINT br;

            do {
                res = f_read(&file, buffer, sizeof(buffer) - 1, &br);
                if (res != FR_OK) {
                    printf("\r\nf_read error %d\r\n", res);
                    break;
                }
                buffer[br] = '\0';
                printf("%s", buffer);
            } while (br > 0);

            printf("\r\n----- End of file -----\r\n");
            f_close(&file);
        } else {
            printf("\r\nf_open error %d\r\n", res);
        }
    }

    /* Pause so you can read the file before Task 2 */
    printf("\r\nTask 1 completed! Press Enter to continue\r\n");

    /* Flush leftover newline from the previous scanf */
    int ch;
    while ((ch = getchar()) != '\n' && ch != '\r' && ch != EOF) { }
    /* Wait for a fresh key press (Enter) */
    getchar();


    /* --------------------------------------------------------------------- */
    /* Task 2 : JPEG decode WITHOUT DMA                                      */
    /* --------------------------------------------------------------------- */

    /* Initialize JPEG peripheral */
    jpeg_handle.Instance = JPEG;
    HAL_JPEG_Init(&jpeg_handle);

    /* Ask user for JPEG filename (in root of SD) */
    char jpegName[64];
    char jpegPath[80];

    printf("Task 2: enter JPEG filename in 0:/ to decode (e.g. image.jpg): ");
    scanf("%63s", jpegName);
    snprintf(jpegPath, sizeof(jpegPath), "0:/%s", jpegName);

    res = f_open(&jpegFile, jpegPath, FA_READ);
    if (res != FR_OK)
    {
    	printf("Failed to open JPEG file %s (error %d)\r\n", jpegPath, res);
    	while (1);
    }

    /* Reset streaming state */
    JPEG_EOF           = 0;
    JpegDecodeFinished = 0;
    num_bytes_decoded  = 0;

    /* Prime the first input buffer from the file */
    UINT bytesRead = 0;
    res = f_read(&jpegFile, JPEG_InputBuffer, JPEG_BUFFER_SIZE, &bytesRead);
    if (res != FR_OK || bytesRead == 0)
    {
    	printf("Failed to read JPEG file (error %d)\r\n", res);
    	while (1);
    }

    /* Start decode in polling mode (no DMA) */
    if (HAL_JPEG_Decode(&jpeg_handle,
    					JPEG_InputBuffer,
						bytesRead,
						(uint8_t *)JPEG_OUTPUT_DATA_BUFFER,
						JPEG_BUFFER_SIZE,
						HAL_MAX_DELAY) != HAL_OK)
    {
    	printf("HAL_JPEG_Decode error\r\n");
    	while (1);
    }

    /* Wait for decode complete callback just in case */
    while (!JpegDecodeFinished) { }

    /* Get image info and print size */
    HAL_JPEG_GetInfo(&jpeg_handle, &jpeg_info);
    printf("Task 2: Image size = %lu x %lu\r\n",
    		(unsigned long)jpeg_info.ImageWidth,
			(unsigned long)jpeg_info.ImageHeight);

    f_close(&jpegFile);

    /* Print the image in PuTTY */
    uint8_t *raw_output = colorConversion((uint8_t *)JPEG_OUTPUT_DATA_BUFFER, num_bytes_decoded);
    if (input != 0x1B)
    	printPutty(raw_output, &jpeg_info);

    printf("\r\nTask 2 completed! Press any key to continue \r\n");
    getchar();
    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    printf("\033c"); // Reset device
    fflush(stdout);

    /* --------------------------------------------------------------------- */
    /* Task 3 : JPEG decode WITH DMA                                         */
    /* --------------------------------------------------------------------- */

    /* Ask again for JPEG filename (can be same as above) */
    printf("Task 3: enter JPEG filename in 0:/ to decode with DMA: ");
    scanf("%63s", jpegName);
    snprintf(jpegPath, sizeof(jpegPath), "0:/%s", jpegName);

    res = f_open(&jpegFile, jpegPath, FA_READ);
    if (res != FR_OK)
    {
    	printf("Failed to open JPEG file %s (error %d)\r\n", jpegPath, res);
    	while (1);
    }

    /* Reset streaming state */
    JPEG_EOF           = 0;
    JpegDecodeFinished = 0;
    num_bytes_decoded  = 0;

    /* Prime first input buffer */
    res = f_read(&jpegFile, JPEG_InputBuffer, JPEG_BUFFER_SIZE, &bytesRead);
    if (res != FR_OK || bytesRead == 0)
    {
    	printf("Failed to read JPEG file (error %d)\r\n", res);
    	while (1);
    }

    /* Start decode using DMA */
    if (HAL_JPEG_Decode_DMA(&jpeg_handle,
    						JPEG_InputBuffer,
							bytesRead,
							(uint8_t *)JPEG_OUTPUT_DATA_BUFFER,
							JPEG_BUFFER_SIZE) != HAL_OK)
    {
    	printf("HAL_JPEG_Decode_DMA error\r\n");
    	while (1);
    }

    /* Wait for completion */
    while (!JpegDecodeFinished) { }

    /* Get image info and print size */
    HAL_JPEG_GetInfo(&jpeg_handle, &jpeg_info);
    printf("Task 3: Image size = %lu x %lu\r\n",
    		(unsigned long)jpeg_info.ImageWidth,
			(unsigned long)jpeg_info.ImageHeight);

    f_close(&jpegFile);

    /* Print the image in PuTTY */
    raw_output = colorConversion((uint8_t *)JPEG_OUTPUT_DATA_BUFFER, num_bytes_decoded);
    if (input != 0x1B)
    	printPutty(raw_output, &jpeg_info);

    printf("Task 3 completed! Press any key to continue \r\n");
    getchar();
    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    printf("\033c"); // Reset device
    fflush(stdout);

    /* --------------------------------------------------------------------- */
    /* Task 4 (Depth) : DMA2D -> LCD framebuffer                             */
    /* --------------------------------------------------------------------- */

	uint32_t xPos = (BSP_LCD_GetXSize() - jpeg_info.ImageWidth)/2;
	uint32_t yPos = (BSP_LCD_GetYSize() - jpeg_info.ImageHeight)/2;

	DMA2D_CopyBuffer((uint32_t *)raw_output, (uint32_t *)LCD_FRAME_BUFFER, xPos , yPos, &jpeg_info);
	printPutty2D((uint8_t *)LCD_FRAME_BUFFER, xPos, yPos, &jpeg_info);

	while (1) { }   // done
}

/* ==== DMA2D copy helper ===================================================*/

void DMA2D_CopyBuffer(uint32_t *pSrc, uint32_t *pDst, uint16_t x, uint16_t y, JPEG_ConfTypeDef *jpeg_info)
{
	uint32_t source      = (uint32_t)pSrc;
	uint32_t destination = (uint32_t)pDst +
			               ((uint32_t)y * BSP_LCD_GetXSize() + (uint32_t)x) * 4U;

	/*
	 * Provided: width offset calculation
	 * From DMA2D example and STM32 application note
	 */
	uint32_t width_offset = 0;
	if(jpeg_info->ChromaSubsampling == JPEG_420_SUBSAMPLING)
	{
		if((jpeg_info->ImageWidth % 16) != 0)
			width_offset = 16 - (jpeg_info->ImageWidth % 16);
	}

	if(jpeg_info->ChromaSubsampling == JPEG_422_SUBSAMPLING)
	{
		if((jpeg_info->ImageWidth % 16) != 0)
			width_offset = 16 - (jpeg_info->ImageWidth % 16);
	}

	if(jpeg_info->ChromaSubsampling == JPEG_444_SUBSAMPLING)
	{
		if((jpeg_info->ImageWidth % 8) != 0)
			width_offset = 8 - (jpeg_info->ImageWidth % 8);
	}

	/*##-1- Configure the DMA2D Mode, Color Mode and output offset #############*/
	DMA2D_Handle.Instance          = DMA2D;
	DMA2D_Handle.Init.Mode         = DMA2D_M2M;              // memory to memory
	DMA2D_Handle.Init.ColorMode    = DMA2D_OUTPUT_ARGB8888;  // matches LCD
	DMA2D_Handle.Init.OutputOffset = BSP_LCD_GetXSize() - jpeg_info->ImageWidth;
	DMA2D_Handle.Init.AlphaInverted= DMA2D_NO_MODIF_ALPHA;
	DMA2D_Handle.Init.RedBlueSwap  = DMA2D_RB_REGULAR;

	/*##-2- DMA2D Callbacks Configuration ######################################*/
	DMA2D_Handle.XferCpltCallback  = NULL;

	/*##-3- Foreground Configuration ###########################################*/
	DMA2D_Handle.LayerCfg[1].AlphaMode      = DMA2D_NO_MODIF_ALPHA;
	DMA2D_Handle.LayerCfg[1].InputAlpha     = 0xFF;
	DMA2D_Handle.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
	DMA2D_Handle.LayerCfg[1].InputOffset    = width_offset;
	DMA2D_Handle.LayerCfg[1].RedBlueSwap    = DMA2D_RB_REGULAR;
	DMA2D_Handle.LayerCfg[1].AlphaInverted  = DMA2D_NO_MODIF_ALPHA;

	/* DMA2D Initialization */
	if (HAL_DMA2D_Init(&DMA2D_Handle) != HAL_OK)
	{
		printf("HAL_DMA2D_Init error\r\n");
		while (1);
	}

	/* DMA2D Config Layer */
	if (HAL_DMA2D_ConfigLayer(&DMA2D_Handle, 1) != HAL_OK)
	{
		printf("HAL_DMA2D_ConfigLayer error\r\n");
		while (1);
	}

	/* DMA2D Start */
	if (HAL_DMA2D_Start(&DMA2D_Handle,
						source,
						destination,
						jpeg_info->ImageWidth,
						jpeg_info->ImageHeight) != HAL_OK)
	{
		printf("HAL_DMA2D_Start error\r\n");
		while (1);
	}

	/* DMA2D Poll for Transfer */
	HAL_DMA2D_PollForTransfer(&DMA2D_Handle, HAL_MAX_DELAY);
}

/* ==== JPEG callbacks ======================================================*/

/*
 * Callback called whenever the JPEG needs more data
 */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
	(void)hjpeg;
	(void)NbDecodedData;

	if (JPEG_EOF)
	{
		/* No more data to provide */
		HAL_JPEG_ConfigInputBuffer(&jpeg_handle, JPEG_InputBuffer, 0);
		return;
	}

	UINT bytesRead = 0;
	FRESULT res = f_read(&jpegFile, JPEG_InputBuffer, JPEG_BUFFER_SIZE, &bytesRead);

	if (res != FR_OK || bytesRead == 0)
	{
		JPEG_EOF = 1;
		/* Signal end of data */
		HAL_JPEG_ConfigInputBuffer(&jpeg_handle, JPEG_InputBuffer, 0);
	}
	else
	{
		HAL_JPEG_ConfigInputBuffer(&jpeg_handle, JPEG_InputBuffer, bytesRead);
	}
}

/* Called when a portion of output data is ready */
void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
{
	(void)hjpeg;

	/* Keep total number of decoded bytes */
	num_bytes_decoded += OutDataLength;

	/* Configure next output buffer directly after the current one */
	uint8_t *nextBuf = pDataOut + OutDataLength;
	HAL_JPEG_ConfigOutputBuffer(&jpeg_handle, nextBuf, JPEG_BUFFER_SIZE);
}

/* Decode complete */
void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{
	(void)hjpeg;
	JpegDecodeFinished = 1;
}

/* Provided code for callback (unchanged)
 * Called when the jpeg header has been parsed
 * Adjust the width to be a multiple of 8 or 16 (depending on image configuration) (from STM examples)
 * Get the correct color conversion function to use to convert to RGB
 */
void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg, JPEG_ConfTypeDef *pInfo)
{
	// Have to add padding for DMA2D
	if(pInfo->ChromaSubsampling == JPEG_420_SUBSAMPLING)
	{
		if((pInfo->ImageWidth % 16) != 0)
			pInfo->ImageWidth += (16 - (pInfo->ImageWidth % 16));

		if((pInfo->ImageHeight % 16) != 0)
			pInfo->ImageHeight += (16 - (pInfo->ImageHeight % 16));
	}

	if(pInfo->ChromaSubsampling == JPEG_422_SUBSAMPLING)
	{
		if((pInfo->ImageWidth % 16) != 0)
			pInfo->ImageWidth += (16 - (pInfo->ImageWidth % 16));

		if((pInfo->ImageHeight % 8) != 0)
			pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
	}

	if(pInfo->ChromaSubsampling == JPEG_444_SUBSAMPLING)
	{
		if((pInfo->ImageWidth % 8) != 0)
			pInfo->ImageWidth += (8 - (pInfo->ImageWidth % 8));

		if((pInfo->ImageHeight % 8) != 0)
			pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
	}

	if(JPEG_GetDecodeColorConvertFunc(pInfo, &pConvert_Function, &MCU_TotalNb) != HAL_OK)
	{
		printf("Error getting DecodeColorConvertFunct\r\n");
		while(1);
	}
}

/* ==== JPEG MSP / DMA init + IRQ handlers =================================*/

void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{
	/* Enable JPEG and DMA clocks */
	__HAL_RCC_JPEG_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* Enable + setup JPEG IRQ */
	HAL_NVIC_SetPriority(JPEG_IRQn, 0x0F, 0);
	HAL_NVIC_EnableIRQ(JPEG_IRQn);

	/* ---------------------- Input DMA (memory -> JPEG) -------------------- */
	hdmaIn.Instance                 = DMA2_Stream3;
	hdmaIn.Init.Channel             = DMA_CHANNEL_9;
	hdmaIn.Init.Direction           = DMA_MEMORY_TO_PERIPH;
	hdmaIn.Init.PeriphInc           = DMA_PINC_DISABLE;
	hdmaIn.Init.MemInc              = DMA_MINC_ENABLE;
	hdmaIn.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	hdmaIn.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	hdmaIn.Init.Mode                = DMA_NORMAL;
	hdmaIn.Init.Priority            = DMA_PRIORITY_HIGH;
	hdmaIn.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
	hdmaIn.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	hdmaIn.Init.MemBurst            = DMA_MBURST_SINGLE;
	hdmaIn.Init.PeriphBurst         = DMA_PBURST_SINGLE;

	HAL_DMA_Init(&hdmaIn);

	/* Link hdmaIn to JPEG handle (input) */
	__HAL_LINKDMA(hjpeg, hdmain, hdmaIn);

	/* Enable + setup Input DMA IRQ */
	HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0x0F, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

	/* ---------------------- Output DMA (JPEG -> memory) ------------------- */
	hdmaOut.Instance                 = DMA2_Stream4;
	hdmaOut.Init.Channel             = DMA_CHANNEL_9;
	hdmaOut.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	hdmaOut.Init.PeriphInc           = DMA_PINC_DISABLE;
	hdmaOut.Init.MemInc              = DMA_MINC_ENABLE;
	hdmaOut.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	hdmaOut.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	hdmaOut.Init.Mode                = DMA_NORMAL;
	hdmaOut.Init.Priority            = DMA_PRIORITY_HIGH;
	hdmaOut.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
	hdmaOut.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	hdmaOut.Init.MemBurst            = DMA_MBURST_SINGLE;
	hdmaOut.Init.PeriphBurst         = DMA_PBURST_SINGLE;

	HAL_DMA_Init(&hdmaOut);

	/* Link hdmaOut to JPEG handle (output) */
	__HAL_LINKDMA(hjpeg, hdmaout, hdmaOut);

	/* Enable + setup Output DMA IRQ */
	HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0x0F, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);
}

void JPEG_IRQHandler(void)
{
	HAL_JPEG_IRQHandler(&jpeg_handle);
}

void DMA2_Stream3_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdmaIn);
}

void DMA2_Stream4_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdmaOut);
}
