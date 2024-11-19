/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "model.h"
#include "model_data.h"
#include "ai_platform.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define F_OUT "output.bin" //file in cui vengono salvati gli output
#define F_REPORT "output.txt" //file di report
#define F_INPUT "input.bin" //file contenente il vettore di input
#define QUANTIZZAZIONE 0 //0 se no (input in float32) ed 1 se si (input in int8)
#if QUANTIZZAZIONE
typedef ai_i8 IN_TYPE;
#else
typedef ai_float IN_TYPE;
#endif
#define STANDARDIZZAZIONE 1 //1 se si o 0 se no
//se standardizzazione è pari a 1 settare con gli appositi valori
#define MEDIA 		522.0
#define VARIANZA	814.0
#define OUT_EXP 	3999.0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd1;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
FIL file_out;
FIL file_in;
FIL f_report;
char buf[100];
ai_float max_val = 0;
ai_float min_val = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
int aiRun(const void*, void*);
int aiInit(void);
void acquire_and_process_data(IN_TYPE*, int, IN_TYPE*);
FRESULT read_from_sd(long int, ai_float*, int);
void write_to_sd(ai_float);
int net_run(IN_TYPE*, int, IN_TYPE*, IN_TYPE*);
void w_report(char*, int);
IN_TYPE* recast(ai_float*, int);
ai_float dequan(IN_TYPE*);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static ai_handle network = AI_HANDLE_NULL;

/* Global c-array to handle the activations buffer */
AI_ALIGNED(32)
static ai_u8 activations[AI_MODEL_DATA_ACTIVATIONS_SIZE];

/* Array to store the data of the input tensor */
	AI_ALIGNED(32)
static IN_TYPE in_data[AI_MODEL_IN_1_SIZE];


/* c-array to store the data of the output tensor */
AI_ALIGNED(32)
static IN_TYPE out_data[AI_MODEL_OUT_1_SIZE];


/* Array of pointer to manage the model's input/output tensors */
static ai_buffer *ai_input;
static ai_buffer *ai_output;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  //creazione della rete e dei buffer di input ed output
   if (aiInit() != 0)
     	  {
     		  Error_Handler();
     	  }
   FRESULT er = FR_OK;


   //montaggio della scheda SD
   er = f_mount(&SDFatFS, (TCHAR const*)SDPath, 0);
   if(er != FR_OK)
   {
    	  	   	Error_Handler();
   }

   strcpy (buf, "rete analizzata: ");
   w_report (buf, strlen(buf));
   int lung_stringa = strlen(AI_MODEL_ORIGIN_MODEL_NAME);
   w_report(AI_MODEL_ORIGIN_MODEL_NAME, lung_stringa);

   er = f_open(&file_in, F_INPUT, FA_READ);
   if(er != FR_OK)
   {
   	Error_Handler();
   }
   long int f_lung = f_size(&file_in); //salvo la lunghezza del vettore di input
   er = f_close(&file_in);
   if(er != FR_OK)
   {
   	Error_Handler();
   }

   /*definisco il numero di chunk da processare e creo il vettore prec
   per il passaggi da un chunk all'altro*/

   long int n_float = f_lung / sizeof(ai_float);
   int max_chunk = n_float / 10000;
   int c_corrente = 0;
   ai_float* ingresso;
   IN_TYPE* val;
   ai_float prec[AI_MODEL_IN_1_SIZE - 1];
   for(int k = 0; k < AI_MODEL_IN_1_SIZE - 1; k++)
 	  prec[k] = 0;

   sprintf(buf, "\nnumero di valori in ingresso: %ld\n", n_float);
   w_report(buf, strlen(buf));

   HAL_GPIO_WritePin(GPIOE, LED1_Pin|DCDC_2_EN_Pin, GPIO_PIN_SET); //accende un led che indica che il chip sta processando gli input

   strcpy (buf, "\ntempo di run di ogni inferenza:\n");
   lung_stringa = strlen(buf);
   w_report(buf, lung_stringa);

   while(c_corrente < max_chunk)
   {
 	  /*allocazione memoria e riempimento del vettore di imput di un determinato chunk*/
 	  ingresso = malloc(sizeof(ai_float) * (10000 + AI_MODEL_IN_1_SIZE - 1));
 	  for(int k = 0; k < AI_MODEL_IN_1_SIZE - 1; k++)
 	  		  ingresso[k] = prec[k];

 	  er = read_from_sd(10000, ingresso, c_corrente);
 	  if(er != FR_OK)
 	  {
    	    	  Error_Handler();
 	  }
 	  /*riempimento vettore prec per il chunk successivo*/
 	  for(int k = 0; k < AI_MODEL_IN_1_SIZE - 1; k++)
 	  	  		  prec[k] = ingresso[10000 + k];

 	  /*standardizzazione dell'imput e quantizzazione, se necessaria*/
 	  if (STANDARDIZZAZIONE)
 	  {
 		  for(int i = 0; i < 10000 + AI_MODEL_IN_1_SIZE - 1; i++)
 			  ingresso[i] = (ingresso[i] - MEDIA) / VARIANZA;
 	  }

 	  if (QUANTIZZAZIONE)
 	  {
 		  val = recast(ingresso, 10000 + AI_MODEL_IN_1_SIZE - 1);
 		  if(val == NULL)
 			  Error_Handler();
 		  free (ingresso);
 	  }
 	  else
 	  {
 		  val = ingresso;
 	  }

 	  /*run del chnk corrente*/
 	  for(int j = 0; j < 10000; j++)
 	  {
 		  if (net_run(in_data, j, val, out_data) != 0)
 			  Error_Handler();

 	  }
 	  free(val);
 	  c_corrente ++;
   }
 /*operazioni effettuate per l'ultimo chunk*/
   if((c_corrente == max_chunk)||(max_chunk == 0))
   {
 	  n_float = n_float - (max_chunk * 10000) + AI_MODEL_IN_1_SIZE - 1;
 	  ingresso = malloc(sizeof(ai_float) * n_float);
 	  for(int k = 0; k < AI_MODEL_IN_1_SIZE - 1; k++)
 	  		  ingresso[k] = prec[k];

 	  er = read_from_sd(n_float - AI_MODEL_IN_1_SIZE - 1, ingresso, c_corrente);

 	  if (STANDARDIZZAZIONE)
 	  {
 	  		  for(int k = 0; k < sizeof(ingresso); k++)
 	  			  ingresso[k] = (ingresso[k] - MEDIA) / VARIANZA;
 	  }
 	  if (QUANTIZZAZIONE)
 	  {
 	  	  val = recast(ingresso, n_float);
 	  	  if(val == NULL)
 	  		  Error_Handler();
 	  	  free (ingresso);
 	  }
 	  else
 	  {
 		  val = ingresso;
 	  }

 	  if(er != FR_OK)
 	  {
 		  		  Error_Handler();
 	  }

 	  for(int j = 0; j < n_float - AI_MODEL_IN_1_SIZE - 1; j++)
 	  {
 		  if (net_run(in_data, j, val, out_data) != 0)
 			  Error_Handler();
 	  }
 	  free(val);
   }

   strcpy (buf,"\nfine\n");
   lung_stringa = strlen(buf);
   w_report(buf, lung_stringa);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 30;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SDMMC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDMMC1_SD_Init(void)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd1.Init.ClockDiv = 0;
  hsd1.Init.Transceiver = SDMMC_TRANSCEIVER_DISABLE;
  /* USER CODE BEGIN SDMMC1_Init 2 */

  /* USER CODE END SDMMC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 120 - 1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LED1_Pin|DCDC_2_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LED2_Pin|WIFI_WAKEUP_Pin|CS_DH_Pin|EX_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_RST_GPIO_Port, BLE_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, WIFI_RST_Pin|SPI2_MISO_p2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, CS_WIFI_Pin|C_EN_Pin|CS_ADWB_Pin|STSAFE_RESET_Pin
                          |WIFI_BOOT0_Pin|CS_DHC_Pin|SEL3_4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, BLE_SPI_CS_Pin|SEL1_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI2_MOSI_p2_Pin|PB11_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BOOT0_PE0_Pin BLE_TEST8_Pin */
  GPIO_InitStruct.Pin = BOOT0_PE0_Pin|BLE_TEST8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PB9_Pin PB8_Pin PB14_Pin CHRGB0_Pin */
  GPIO_InitStruct.Pin = PB9_Pin|PB8_Pin|PB14_Pin|CHRGB0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT0_PE0H3_Pin */
  GPIO_InitStruct.Pin = BOOT0_PE0H3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT0_PE0H3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI3_MISO_Pin SPI3_MOSI_Pin SPI3_CLK_Pin */
  GPIO_InitStruct.Pin = SPI3_MISO_Pin|SPI3_MOSI_Pin|SPI3_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI2_MISO_Pin SPI2_CLK_Pin */
  GPIO_InitStruct.Pin = SPI2_MISO_Pin|SPI2_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : BLE_TEST9_Pin WIFI_DRDY_Pin INT1_DHC_Pin INT_STT_Pin
                           INT1_ADWB_Pin */
  GPIO_InitStruct.Pin = BLE_TEST9_Pin|WIFI_DRDY_Pin|INT1_DHC_Pin|INT_STT_Pin
                          |INT1_ADWB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : USART2_RX_Pin USART2_RTS_Pin USART2_TX_Pin */
  GPIO_InitStruct.Pin = USART2_RX_Pin|USART2_RTS_Pin|USART2_TX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : EX_PWM_Pin */
  GPIO_InitStruct.Pin = EX_PWM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(EX_PWM_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OTG_FS_DP_Pin OTG_FS_DM_Pin */
  GPIO_InitStruct.Pin = OTG_FS_DP_Pin|OTG_FS_DM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SAI1_SCK_A_Pin SAI1_MCLK_A_Pin SAI1_FS_A_DFSDM_D3_Pin SAI1_SD_A_Pin
                           SAI1_SD_B_Pin */
  GPIO_InitStruct.Pin = SAI1_SCK_A_Pin|SAI1_MCLK_A_Pin|SAI1_FS_A_DFSDM_D3_Pin|SAI1_SD_A_Pin
                          |SAI1_SD_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_SAI1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_Pin DCDC_2_EN_Pin */
  GPIO_InitStruct.Pin = LED1_Pin|DCDC_2_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin WIFI_WAKEUP_Pin CS_DH_Pin EX_RESET_Pin */
  GPIO_InitStruct.Pin = LED2_Pin|WIFI_WAKEUP_Pin|CS_DH_Pin|EX_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PA10_Pin PA9_Pin PA0_Pin DAC1_OUT1_Pin
                           PA1_Pin */
  GPIO_InitStruct.Pin = PA10_Pin|PA9_Pin|PA0_Pin|DAC1_OUT1_Pin
                          |PA1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DFSDM1_DATIN5_Pin DFSDM1_D7_Pin */
  GPIO_InitStruct.Pin = DFSDM1_DATIN5_Pin|DFSDM1_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_DFSDM1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PG12_Pin PG10_Pin PG9_Pin */
  GPIO_InitStruct.Pin = PG12_Pin|PG10_Pin|PG9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : BLE_RST_Pin */
  GPIO_InitStruct.Pin = BLE_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLE_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : WIFI_RST_Pin SPI2_MISO_p2_Pin */
  GPIO_InitStruct.Pin = WIFI_RST_Pin|SPI2_MISO_p2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : I2C2_SMBA_Pin I2C2_SDA_Pin I2C2_SDAF0_Pin */
  GPIO_InitStruct.Pin = I2C2_SMBA_Pin|I2C2_SDA_Pin|I2C2_SDAF0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : CS_WIFI_Pin C_EN_Pin CS_ADWB_Pin STSAFE_RESET_Pin
                           WIFI_BOOT0_Pin CS_DHC_Pin SEL3_4_Pin */
  GPIO_InitStruct.Pin = CS_WIFI_Pin|C_EN_Pin|CS_ADWB_Pin|STSAFE_RESET_Pin
                          |WIFI_BOOT0_Pin|CS_DHC_Pin|SEL3_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : I2C3_SDA_Pin I2C3_SCL_Pin */
  GPIO_InitStruct.Pin = I2C3_SDA_Pin|I2C3_SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_SEL_Pin */
  GPIO_InitStruct.Pin = SW_SEL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM5;
  HAL_GPIO_Init(SW_SEL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : INT2_DHC_Pin PGOOD_Pin INT_M_Pin */
  GPIO_InitStruct.Pin = INT2_DHC_Pin|PGOOD_Pin|INT_M_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_MISO_Pin SPI1_MOSI_Pin SPI1_CLK_Pin */
  GPIO_InitStruct.Pin = SPI1_MISO_Pin|SPI1_MOSI_Pin|SPI1_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : BLE_SPI_CS_Pin SEL1_2_Pin */
  GPIO_InitStruct.Pin = BLE_SPI_CS_Pin|SEL1_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : INT_HTS_Pin BLE_INT_Pin */
  GPIO_InitStruct.Pin = INT_HTS_Pin|BLE_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : I2C4_SCL_Pin I2C4_SDA_Pin */
  GPIO_InitStruct.Pin = I2C4_SCL_Pin|I2C4_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : ADC1_IN1_Pin ADC1_IN2_Pin uC_ADC_BATT_Pin */
  GPIO_InitStruct.Pin = ADC1_IN1_Pin|ADC1_IN2_Pin|uC_ADC_BATT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : INT2_ADWB_Pin SD_DETECT_Pin */
  GPIO_InitStruct.Pin = INT2_ADWB_Pin|SD_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : CHRG_Pin */
  GPIO_InitStruct.Pin = CHRG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CHRG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_PWR_Pin */
  GPIO_InitStruct.Pin = BUTTON_PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_PWR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART3_RX_Pin USART3_TX_Pin */
  GPIO_InitStruct.Pin = USART3_RX_Pin|USART3_TX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI2_MOSI_Pin */
  GPIO_InitStruct.Pin = SPI2_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(SPI2_MOSI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART3_RTS_Pin USART3_CTS_Pin */
  GPIO_InitStruct.Pin = USART3_RTS_Pin|USART3_CTS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DFSDM1_CKOUT_Pin */
  GPIO_InitStruct.Pin = DFSDM1_CKOUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_DFSDM1;
  HAL_GPIO_Init(DFSDM1_CKOUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI2_MOSI_p2_Pin PB11_Pin */
  GPIO_InitStruct.Pin = SPI2_MOSI_p2_Pin|PB11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : INT2_DH_Pin */
  GPIO_InitStruct.Pin = INT2_DH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(INT2_DH_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EX_ADC_Pin */
  GPIO_InitStruct.Pin = EX_ADC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(EX_ADC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PE12_Pin */
  GPIO_InitStruct.Pin = PE12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PE12_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

int aiRun(const void *in_data, void *out_data) {

	ai_i32 n_batch;
	ai_error err;
/*riempimento buffer di input ed output*/
	ai_input[0].data = AI_HANDLE_PTR(in_data);
	ai_output[0].data = AI_HANDLE_PTR(out_data);
/*run*/
	n_batch = ai_model_run(network, &ai_input[0], &ai_output[0]);
	if (n_batch != 1) {
		err = ai_model_get_error(network);
		Error_Handler();
	};

	return 0;
}

int aiInit(void) {
  ai_error err;

  /* Create and initialize the c-model */
  const ai_handle acts[] = { activations };
  err = ai_model_create_and_init(&network, acts, NULL);
  if (err.type != AI_ERROR_NONE) {
	  Error_Handler();
  }

  /* Reteive pointers to the model's input/output tensors */
  ai_input = ai_model_inputs_get(network, NULL);
  ai_output = ai_model_outputs_get(network, NULL);

  return 0;
}

void acquire_and_process_data(IN_TYPE* in_data, int j, IN_TYPE* val)
{
		for (int i = 0; i < AI_MODEL_IN_1_SIZE; i++)
			in_data[i] = val[i+j];
}

int net_run(IN_TYPE* in_data, int j, IN_TYPE* val,IN_TYPE* out_data)
{
	ai_float risultato;
	uint16_t et;

	acquire_and_process_data(in_data, j, val);

	HAL_TIM_Base_Start(&htim1);
	et = __HAL_TIM_GET_COUNTER(&htim1);

	aiRun(in_data, out_data);

	et = __HAL_TIM_GET_COUNTER(&htim1) - et;
    sprintf(buf, "finestra %d : %uus\n", j,  et);
    int lunghezza_stringa = strlen(buf);
    w_report(buf, lunghezza_stringa);

    if (QUANTIZZAZIONE)
    	risultato = dequan(out_data);
    else
    	risultato = out_data[0];
    if (STANDARDIZZAZIONE){
    write_to_sd(risultato * 3999.0);}
    else{
    write_to_sd(risultato);}

	return 0;
}
/*funzione che legge dalla scheda sd con un offset appropriato*/
FRESULT read_from_sd(long int f_to_r, ai_float* valori_letti, int chunk)
{
	FRESULT er;
	er = f_open(&file_in, F_INPUT , FA_READ);
	if(er != FR_OK)
	{
		Error_Handler();
	}
	int offset = chunk * 10000 *sizeof(ai_float);
	er = f_lseek(&file_in, offset);
	if(er != FR_OK)
	{
		Error_Handler();
	}

	int br;

	er = f_read(&file_in, valori_letti + AI_MODEL_IN_1_SIZE - 1, sizeof(ai_float) * f_to_r, (void*) &br);
	if((er != FR_OK)||br != (sizeof(ai_float) * f_to_r))
	{
	   	Error_Handler();
	}
	er = f_close(&file_in);
	if(er != FR_OK)
	{
		Error_Handler();
	}
	return er;
}
/*scrittura del risultato in SD*/
void write_to_sd (ai_float num)
{
	FRESULT er;
	int bw;
	er = f_open(&file_out, F_OUT, FA_WRITE | FA_OPEN_APPEND);
	if(er != FR_OK)
	{
		 Error_Handler();
	}
	er = f_write(&file_out, &num, sizeof(num), (void *)&bw);
	if((er != FR_OK)||(bw == 0))
	{
		Error_Handler();
	}
	er = f_close(&file_out);
	if(er != FR_OK)
	{
		 Error_Handler();
	}

}
void w_report(char* stringa, int l_str)
{
	FRESULT er;
	int bw;
	er = f_open(&f_report, F_REPORT, FA_WRITE | FA_OPEN_APPEND);
	if(er != FR_OK)
	{
		Error_Handler();
	}
	er = f_write(&f_report, stringa, l_str, (void *)&bw);
	if((er != FR_OK)||(bw != l_str))
	{
		Error_Handler();
	}
	er = f_close(&f_report);
	if((er != FR_OK)||(bw == 0))
	{
		Error_Handler();
	}
}
/*quantizzazione lineare degli input, in fase di test sono state usate delle reti in cui
gli input dovevano essere quatizzati*/
IN_TYPE* recast(ai_float* v_float,int lungh)
{
	if (lungh == 0 || v_float == NULL)
	        return NULL;  // Gestione degli errori

	ai_float range;
	max_val = v_float[0];
	min_val = v_float[0];

	for (int i = 0; i < lungh; i++)
	{
		if (v_float[i] > max_val)
			max_val = v_float[i];
		if (v_float[i] < min_val)
		    min_val = v_float[i];
	}
	range = max_val - min_val;
	if (range == 0)
	{
        range = 1;  // Evita divisione per zero
    }

	IN_TYPE* result = (IN_TYPE*)malloc(lungh * sizeof(IN_TYPE));
	if (result == NULL)
	        return NULL;  // Gestione errori in caso di allocazione fallita
    // Normalizza e converte i valori in int8_t
    for (int i = 0; i < lungh; i++) {
        ai_float normalized = (v_float[i] - min_val) / range;  // Normalizza tra 0 e 1
        result[i] = (IN_TYPE) (roundf(normalized * 255.0) - 128);  // Scala a -128 ... 127
    }

    return result;
}
/*dequantizzazione*/
ai_float dequan(IN_TYPE* output)
{
	ai_float range = max_val - min_val;
	if (range == 0)
	{
	        range = 1;  // Evita moltiplicazione per zero
	}
	ai_float risultato = output[0];
	risultato = ((risultato + 128) * (max_val - min_val) / 255) + min_val;
	return risultato;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	  /* User can add his own implementation to report the HAL error return state */
	  __disable_irq();
	  w_report("errore", strlen("errore"));
	  HAL_GPIO_WritePin(GPIOE, LED1_Pin|DCDC_2_EN_Pin, GPIO_PIN_RESET);
	  while (1)
	  {
	  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
