/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#include "fs.h"
#include "fs_port.h"
#include <stdio.h>
#include "shell.h"
#include "comm.h"
#include "cbpf.h"
#include "ccbpf.h"
#include "parser.h"
#include <stdint.h>
#include <stdlib.h>


int __io_putchar(int ch)
{
    if (ch == '\n') {
        uint8_t cr = '\r';
        HAL_UART_Transmit(&huart1, &cr, 1, HAL_MAX_DELAY);
    }

    uint8_t c = ch;
    HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return ch;
}

int __io_getchar(void)
{
    uint8_t c;
    HAL_UART_Receive(&huart1, &c, 1, HAL_MAX_DELAY);
    return c;
}

void native_init_frontend(void)
{
    native_decl_register("ntohl",      1, 1);
    native_decl_register("ntohs",      2, 1);
    native_decl_register("print",      3, 1);
    native_decl_register("print_str",  4, 1);
    native_decl_register("map_lookup", 5, 2);
    native_decl_register("map_update", 6, 3);
    native_decl_register("now_ms",     7, 0);
    native_decl_register("migrate",    8, 0);
}

uint32_t native_printf(struct ccbpf_program *p,
                       uint32_t a0,
                       uint32_t a1,
                       uint32_t a2,
                       uint32_t a3)
{
    printf("stm32: %u\r\n", a0);
    return 0;
}

uint32_t native_print_str(struct ccbpf_program *p,
                          uint32_t a0,
                          uint32_t a1,
                          uint32_t a2,
                          uint32_t a3)
{
    if (a0 >= (uint32_t)p->string_count) return 0;
    printf("%s", p->strings[a0]);
    return 0;
}

void native_register_all(void)
{
    native_register(3, 1, native_printf);
    native_register(4, 1, native_print_str);
}

static void pass_packet(uint8_t *pkt, uint32_t len)
{
    printf("pass\r\n");
}

static void drop_packet(uint8_t *pkt, uint32_t len)
{
    printf("drop\r\n");
}

struct udp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t checksum;
};

static void make_udp_packet(uint8_t *buf, uint32_t *len, int type)
{
    struct udp_hdr *uh = (struct udp_hdr *)buf;
    uint32_t payload_len = 32;
    uint32_t udp_len = 8 + payload_len;

    uh->sport = 10000;
    uh->dport = 20000;
    uh->len   = udp_len;
    uh->checksum = 0;

    for (uint32_t i = 8; i < udp_len; i++)
        buf[i] = rand() & 0xFF;

    switch (type) {
        case 0:
            break;
        case 1:
            uh->len = 9999;
            break;
        case 2:
            uh->sport = 12345;
            uh->dport = 54321;
            break;
    }

    *len = udp_len;
}

void test_bpf_on_mcu(void)
{
    uint8_t pkt[40];
    uint32_t len;

     {
        int type = rand() % 3;
        make_udp_packet(pkt, &len, type);
        uint32_t ret = hook_run("hook_udp_input", pkt, len);

        if (ret)
            pass_packet(pkt, len);
        else
            drop_packet(pkt, len);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

    ccbpf_system_init();
    native_register_all();
    hook_register("hook_udp_input");

    comm_init_uart(&huart1);

    struct superblock sb;

    fs_port_init();

    if (fs_port_mount(&sb) != 0) {
        printf("FS mount after format failed!\r\n");
    }

    printf("FS mounted OK!\r\n");
    printf("Starting shell...\r\n");

    HAL_Delay(100);

    while (1) {
        shell_main();
        test_bpf_on_mcu();
    }
}

/* USER CODE END 0 */
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
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
