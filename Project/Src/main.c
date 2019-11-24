/* Includes ------------------------------------------------------------------*/
#include "main.h"

//#define DEBUG
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
FATFS SDFatFs;  /* File system object for SD card logical drive */
FIL MyFile;     /* File object */
char SDPath[4]; /* SD card logical drive path */
uint8_t workBuffer[_MAX_SS];

// Go to application code
typedef  void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);

/* Private functions ---------------------------------------------------------*/
static bool checkHexFile(FIL *MyFile, char* fileNameStr);
static bool writeFlash(FIL *MyFile, char* fileNameStr);
/*----------------------------------------------------------------------------*/
/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* Enable the CPU Cache */
  CPU_CACHE_Enable();

  /* STM32F7xx HAL library initialization:
       - Configure the Flash ART accelerator on ITCM interface
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();
  
  /* Configure the system clock to 216 MHz */
  SystemClock_Config();
  
  /* Configure LED1  */
  BSP_LED_Init(LED1);
	BSP_LED_Off(LED1);
  
  /*##-1- Link the micro SD disk I/O driver ##################################*/
  if(FATFS_LinkDriver(&SD_Driver, SDPath) == 0)
  {
    /*##-2- Register the file system object to the FatFs module ##############*/
    if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) != FR_OK)
    {
      /* FatFs Initialization Error */
      while(1);
    }
	}
	if(checkHexFile(&MyFile, "program.hex")){
		HAL_Delay(1000);
		FLASH_If_Erase(0);
		writeFlash(&MyFile, "program.hex");		
		
		/* Test if user code is programmed starting from address "APPLICATION_ADDRESS" */
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
      /* Jump to user application */
      JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
      JumpToApplication = (pFunction) JumpAddress;
      /* Initialize user application's Stack Pointer */
      __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
      JumpToApplication();
    }
	}	
	

  
  /*##-11- Unlink the micro SD disk I/O driver ###############################*/
  FATFS_UnLinkDriver(SDPath);
  
  /* Infinite loop */
  while (1)
  {
  }
}

/*----------------------------------------------------------------------------*/
static uint8_t hex2Dec(char hexChr){
	if(hexChr > '9'){
		return (hexChr - 'A' + 10);
	}
	else{
		return (hexChr - '0');
	}
}
/*----------------------------------------------------------------------------*/
static bool checkHexFile(FIL *MyFile, char* fileNameStr){
  uint32_t bytesread;                     /* File write/read counts */
  uint8_t rHex[1024];
	
	if(f_open(MyFile, "program.hex", FA_READ) != FR_OK){
		return false;
	}
	// Check start address session
	f_read(MyFile, rHex, 17, (UINT*)&bytesread);
	if(strcmp((const char *)rHex, ":020000040800F2\r\n") != 0){
		f_close(MyFile);
		return false;
	}
	
	uint16_t addrCurrent = 0x8000;
	bool isEnd = false;
	while(!isEnd){
		// Get row and check	
		f_read(MyFile, rHex, 9, (UINT*)&bytesread);
		// Check string data error
		if((rHex[0] != ':')||(rHex[7] != '0')){
			f_close(MyFile);
			#ifdef DEBUG
				while(1);
			#endif
			f_close(MyFile);
			return false;
		}		
		uint8_t i;
		for(i = 1; i < bytesread; i++){
			if(((rHex[i] < '0')&&(rHex[i] > '9'))&&((rHex[i] < 'A')&&(rHex[i] > 'F'))){
				#ifdef DEBUG
					while(1);
				#endif
				f_close(MyFile);
				return false;
			}
		}
		
		// Get address
		uint16_t addr = hex2Dec(rHex[3])*4096 + hex2Dec(rHex[4])*256 + hex2Dec(rHex[5])*16 + hex2Dec(rHex[6]);
		
		
		// Check length
		uint8_t len;				
		if(rHex[1] > '1'){
			#ifdef DEBUG
				while(1);
			#endif
			f_close(MyFile);
			return false;
		}
		len = hex2Dec(rHex[1])*16 + hex2Dec(rHex[2]);
		
		// Sum before data
		uint16_t sumValue = 0;
		for(i = 0; i < (4); i++){
			sumValue += (hex2Dec(rHex[1 + i*2])*16 + hex2Dec(rHex[1 + i*2 + 1]));
		}
		
		// Assign record type
		char recordType = rHex[8];
		
		// Read data
		if(len > 0){
			f_read(MyFile, rHex, ((len + 2)*2), (UINT*)&bytesread);
		}
		
		// Scan and check data
		switch(recordType){			
			case '0':{		// Data
				// Check address
				if(addrCurrent != addr){
					#ifdef DEBUG
						while(1);
					#endif
					f_close(MyFile);
					return false;
				}
				// Sum data
				for(i = 0; i < (len); i++){
					sumValue += (hex2Dec(rHex[i*2])*16 + hex2Dec(rHex[i*2 + 1]));
				}
				sumValue = sumValue%256;				
				// Compare checksum
				uint8_t checkSum = (hex2Dec(rHex[len*2])*16 + hex2Dec(rHex[len*2 + 1]));
				if(checkSum != ((256 - sumValue)%256)){
					#ifdef DEBUG
						while(1);
					#endif
					f_close(MyFile);
					return false;
				}
				// Update address current
				addrCurrent += len;
				
				break;
			}
			case '1':{		// End of file
				if(strncmp((const char *)rHex, ":00000001", 11) != 0){
					f_read(MyFile, rHex, 2, (UINT*)&bytesread);
					if(strncmp((const char *)rHex, "FF", 11) != 0){
						f_close(MyFile);
						isEnd = true;
						break;
					}
				}
				
				#ifdef DEBUG
					while(1);
				#endif
				f_close(MyFile);
				return false;	
			}
			case '2':{		// Extended Segment address
				// Check extended segment address
				// Reset address current
				addrCurrent = 0;
				break;
			}
			case '3':{		// Start segment address
				
				break;
			}
			case '4':{		// Extended Linear Address
				
				break;
			}
			case '5':{		// 	Start Linear Address
				
				break;
			}
			default:{
				
				break;
			}
		}
		
	}

	return true;
}
/*----------------------------------------------------------------------------*/
static bool writeFlash(FIL *MyFile, char* fileNameStr){
	uint32_t bytesread;                     /* File write/read counts */
  uint8_t rHex[1024];
	uint8_t flashData[2048];
	uint16_t flashCount = 0;
	uint32_t flashAddrWrite = APPLICATION_ADDRESS;
	
	if(f_open(MyFile, "program.hex", FA_READ) != FR_OK){
		return false;
	}
	
	uint16_t addrCurrent = 0x8000;
	bool isEnd = false;
	while(!isEnd){
		// Get row and check	
		f_read(MyFile, rHex, 9, (UINT*)&bytesread);

		uint8_t i;
		
		// Get address
		uint16_t addr = hex2Dec(rHex[3])*4096 + hex2Dec(rHex[4])*256 + hex2Dec(rHex[5])*16 + hex2Dec(rHex[6]);
				
		// Get length
		uint8_t len;
		len = hex2Dec(rHex[1])*16 + hex2Dec(rHex[2]);
		
		// Sum before data
		uint16_t sumValue = 0;
		for(i = 0; i < (4); i++){
			sumValue += (hex2Dec(rHex[1 + i*2])*16 + hex2Dec(rHex[1 + i*2 + 1]));
		}
		
		// Assign record type
		char recordType = rHex[8];
		
		// Read data
		if(len > 0){
			f_read(MyFile, rHex, ((len + 2)*2), (UINT*)&bytesread);
		}
		
		// Scan and check data
		switch(recordType){
			case '0':{		// Data
				for(i = 0; i < len; i++){
					flashData[flashCount] = hex2Dec(rHex[i*2])*16 + hex2Dec(rHex[i*2 + 1]);
					flashCount++;
				}

				// Update address current
				addrCurrent += len;
				
				break;
			}
			case '1':{		// End of file				
				f_close(MyFile);
				
				uint16_t numWrite = (uint16_t)(flashCount/4);
				if(flashCount%4 > 0){
					numWrite += 1;
				}
				
				FLASH_If_Write(flashAddrWrite, (uint32_t *)flashData, numWrite);
				isEnd = true;
				break;
			}
			case '2':{		// Extended Segment address
				// Check extended segment address
				
				break;
			}
			case '3':{		// Start segment address
				
				break;
			}
			case '4':{		// Extended Linear Address
				
				break;
			}
			case '5':{		// 	Start Linear Address
				
				break;
			}
			default:{
				
				break;
			}
		}
		
		if(flashCount == 2048){
			flashCount = 0; 
			FLASH_If_Write(flashAddrWrite, (uint32_t *)flashData, 2048/4);
			flashAddrWrite += 2048;
			HAL_Delay(1);
		}
	}
	
	f_close(MyFile);
	return true;
}
/*----------------------------------------------------------------------------*/
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 216000000
  *            HCLK(Hz)                       = 216000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 432
  *            PLL_P                          = 2
  *            PLL_Q                          = 9
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 7
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;  
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }
  
  /* Activate the OverDrive to reach the 216 MHz Frequency */  
  ret = HAL_PWREx_EnableOverDrive();
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; 
  
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }  
}

/*----------------------------------------------------------------------------*/
#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */ 

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
