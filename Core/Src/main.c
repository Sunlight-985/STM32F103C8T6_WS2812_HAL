/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include <stdint.h>  // 使用uint8_t需包含的头文件
#include <stdlib.h>  // abs函数的头文件
#include <string.h>  // memset函数的标准头文件
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// 针对32位无符号整数的别名
typedef uint32_t   u32;///32
typedef uint16_t   u16;///16
typedef uint8_t     u8;///8
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SCROLL_SPEED 100  //滚动速度（ms），值越小越快

// 宏定义颜色索引（对应8种颜色，便于调用）
#define COLOR_BLACK    0//// 黑色
#define COLOR_RED      1//// 红色
#define COLOR_GREEN    2//// 绿色
#define COLOR_BLUE     3//// 蓝色
#define COLOR_YELLOW   4//// 黄色
#define COLOR_PURPLE   5//// 紫色
#define COLOR_CYAN     6//// 青色
#define COLOR_WHITE    7//// 白色
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

char running_mode_str[20];  //显示模式字符串的存放数组

// 定义WS2812的GPIO端口/引脚全局变量，可在初始化函数中修改
GPIO_TypeDef *RGB_GPIO_PORT = GPIOB;    // 默认端口：GPIOB
uint16_t RGB_GPIO_PIN = GPIO_PIN_9;     // 默认引脚：PIN9

uint8_t target_leds[] = {0, 2, 5};  // 需要点亮的灯珠索引
uint8_t arr_len = sizeof(target_leds)/sizeof(target_leds[0]); // 数组长度（3）
uint8_t total_led = 8; // 假设总共有8个灯珠

uint8_t rainbow_running_flag = 0;  // 0=停止，1=运行
uint8_t rainbow_running_mode = 0;    //共有五种显示模式，0-4

// 定义8组RGB颜色数组（uint8_t类型，取值0x00~0xFF）
uint8_t rgb_color_array[8][3] = {
	{0x00, 0x00, 0x00},  //// 黑色
	{0xFF, 0x00, 0x00},  //// 红色
	{0x00, 0xFF, 0x00},  //// 绿色
	{0x00, 0x00, 0xFF},  //// 蓝色
	{0xFF, 0xFF, 0x00},  //// 黄色
	{0xFF, 0x00, 0xFF},  //// 紫色
	{0x00, 0xFF, 0xFF},  //// 青色
	{0xFF, 0xFF, 0xFF}   //// 白色
};

// 定义“灯珠索引+颜色”的结构体
typedef struct {
	uint8_t idx;    // 灯珠索引
	uint8_t red;    // 红色值
	uint8_t green;  // 绿色值
	uint8_t blue;   // 蓝色值
} LED_Color;

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void delay_nus(u32 nus)  //72MHz晶振下的us级精确延时
{
	uint32_t Delay = nus * 10;
	do
  {
		__NOP();
   }
	while(Delay --);

}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//向WS2812写入1个字节（8位），严格匹配时序
void RGB_WriteByte(uint8_t in_data)
{	
	uint8_t n = 0;
	uint8_t y = 0,z = 0;
	n = in_data;
	for(y = 0;y < 8;y++)
	{
		z = ((n<<y)&0x80);
		if(z)
		{
			HAL_GPIO_WritePin(RGB_GPIO_PORT,RGB_GPIO_PIN,GPIO_PIN_SET);
			delay_nus(2);
			HAL_GPIO_WritePin(RGB_GPIO_PORT,RGB_GPIO_PIN,GPIO_PIN_RESET);
			__nop();  // 1码

		}
		else
		{    
			HAL_GPIO_WritePin(RGB_GPIO_PORT,RGB_GPIO_PIN,GPIO_PIN_SET);
			__nop();    //
			HAL_GPIO_WritePin(RGB_GPIO_PORT,RGB_GPIO_PIN,GPIO_PIN_RESET);
			delay_nus(2);  // 0码 
		}
	}
}



/*  指定单个灯珠设置颜色（其余灯珠默认黑色） */
void RGB_ColorSet(uint8_t led_idx, uint8_t total_led, uint8_t t_red, uint8_t t_green, uint8_t t_blue)
{
	// 边界检查：防止灯珠索引越界
	if(led_idx >= total_led) return;
	// 步骤1：逐灯珠写入数据（WS2812需遍历所有灯珠）
	for(uint8_t i=0; i<total_led; i++)
	{
		if(i == led_idx)
		{
			// 目标灯珠：写入指定颜色（GRB顺序）
			RGB_WriteByte(t_green);  // 绿
			RGB_WriteByte(t_red);    // 红
			RGB_WriteByte(t_blue);   // 蓝
		}
		else
		{
			// 非目标灯珠：写入黑色（默认熄灭）
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
		}
	}
	// 步骤2：发送WS2812复位信号（必需，低电平≥280us）
	HAL_GPIO_WritePin(RGB_GPIO_PORT, RGB_GPIO_PIN, GPIO_PIN_RESET);
	delay_nus(300);  // 300us > 280us，确保复位成功
}

/*同时点亮的灯：指定多个灯珠索引，同时设置颜色（其余熄灭）*/
void RGB_MultiSameColorSet(uint8_t *led_idx_arr, uint8_t arr_len, uint8_t total_led,
                           uint8_t t_red, uint8_t t_green, uint8_t t_blue)
{
	// 遍历所有灯珠
	for(uint8_t i=0; i<total_led; i++)
	{
		uint8_t is_target = 0; // 标记当前灯珠是否是目标灯珠
		// 检查当前灯珠i是否在“需要点亮的索引数组”中
		for(uint8_t j=0; j<arr_len; j++)
		{
			if(i == led_idx_arr[j])
			{
					is_target = 1;
					break;
			}
		}

		if(is_target)
		{
			// 目标灯珠：写入指定颜色（WS2812是GRB顺序）
			RGB_WriteByte(t_green);
			RGB_WriteByte(t_red);
			RGB_WriteByte(t_blue);
		}
		else
		{
			// 非目标灯珠：写入黑色（熄灭）
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
		}
	}

	// 发送WS2812复位信号（必须）
	HAL_GPIO_WritePin(RGB_GPIO_PORT, RGB_GPIO_PIN, GPIO_PIN_RESET);
	delay_nus(300); // 复位时间≥280us
}


// 多灯、不同颜色的函数
void RGB_MultiDiffColorSet(LED_Color *led_color_arr, uint8_t arr_len, uint8_t total_led)
{
	if(rainbow_running_flag==1)
	{
		for(uint8_t i=0; i<total_led; i++)
		{
			uint8_t is_target = 0;
			uint8_t r=0, g=0, b=0;
			// 查找当前灯珠i对应的颜色
			for(uint8_t j=0; j<arr_len; j++)
			{
				if(i == led_color_arr[j].idx)
				{
					is_target = 1;
					r = led_color_arr[j].red;
					g = led_color_arr[j].green;
					b = led_color_arr[j].blue;
					break;
				}
			}
			// 写入颜色（目标灯珠用指定色，否则黑色）
			RGB_WriteByte(is_target ? g : 0);
			RGB_WriteByte(is_target ? r : 0);
			RGB_WriteByte(is_target ? b : 0);
		}
		// 发送复位信号
		HAL_GPIO_WritePin(RGB_GPIO_PORT, RGB_GPIO_PIN, GPIO_PIN_RESET);
		delay_nus(300);
	}
}

// 渐变滚动：核心用多灯异色函数（调用前面的RGB_MultiDiffColorSet）
void RGB_Scroll_Gradient(void)
{
	uint8_t center_led = 0; // 最亮的中心灯珠
	LED_Color led_color[total_led]; // 存储所有灯珠的颜色

	while(1)
	{
		if(rainbow_running_flag==1)
		{
				// 遍历所有灯珠，计算亮度（中心灯珠最亮，距离越远越暗）
			for(uint8_t i=0; i<total_led; i++)
			{
				led_color[i].idx = i;
				// 计算当前灯珠与中心灯珠的距离（绝对值）
				uint8_t distance = abs(i - center_led);
				// 亮度随距离衰减（距离≤3则亮，否则熄灭）
				uint8_t brightness = (distance <= 3) ? (255 - distance*35) : 0;
				// 设置渐变红色（可换成绿色/蓝色）
				led_color[i].red = 0;
				led_color[i].green = brightness;
				led_color[i].blue = 0;
			}
			// 写入渐变颜色
			RGB_MultiDiffColorSet(led_color, total_led, total_led);
			// 延时
			HAL_Delay(SCROLL_SPEED);
			// 中心灯珠偏移，循环
			center_led = (center_led + 1) % total_led;
		}
		else break;
	}
}

// HSV转RGB函数（用于生成渐变色）
void HSVtoRGB(uint16_t hue, uint8_t sat, uint8_t val, uint8_t *r, uint8_t *g, uint8_t *b) 
{
	if (sat == 0) 
	{           // 饱和度为0 → 灰度色
		*r = val;  
		*g = val;
		*b = val;
		return;
	}
	uint8_t region = hue / 60;       // 色相分区（0-5）
	uint8_t remainder = hue % 60;     // 分区内偏移
	uint8_t p = (val * (255 - sat)) / 255;
	uint8_t q = (val * (255 - (sat * remainder) / 60)) / 255;
	uint8_t t = (val * (255 - (sat * (60 - remainder)) / 60)) / 255;
	// 按分区赋值RGB
	switch (region) 
	{
		case 0: *r = val; *g = t; *b = p; break;
		case 1: *r = q; *g = val; *b = p; break;
		case 2: *r = p; *g = val; *b = t; break;
		case 3: *r = p; *g = q; *b = val; break;
		case 4: *r = t; *g = p; *b = val; break;
		default: *r = val; *g = p; *b = q; break;
  }
}


// 彩虹渐变滚动核心函数
void RGB_RainbowScroll(void) 
{
	uint8_t center_led = 0;       // 滚动中心灯珠
	uint16_t base_hue = 0;        // 基础色相（控制整体彩虹偏移）
	LED_Color led_color[total_led];  // 所有灯珠的颜色配置

	while(1) 
	{
		if(rainbow_running_flag==1)
		{
			for(uint8_t i = 0; i < total_led; i++) 
			{
				led_color[i].idx = i;  // 绑定灯珠索引
				// 计算亮度（中心灯珠最亮，距离越远越暗）
				uint8_t distance = abs((int)i - (int)center_led);  // 距离绝对值
				uint8_t brightness = (distance <= 3) ? (255 - distance * 35) : 0;  // 亮度衰减

				// 计算当前灯珠的色相（彩虹效果核心）
				// 每个灯珠偏移20°色相，叠加基础色相实现整体颜色滚动
				uint16_t current_hue = (base_hue + i * 20) % 360;
				// HSV转RGB（饱和度255=纯色，brightness=明度）
				HSVtoRGB(current_hue, 255, brightness, 
													 &led_color[i].red, 
													 &led_color[i].green, 
													 &led_color[i].blue);
			}
			// 更新所有灯珠显示
			RGB_MultiDiffColorSet(led_color, total_led, total_led);
			// 控制滚动速度和颜色偏移
			HAL_Delay(SCROLL_SPEED);          // 滚动延时
			center_led = (center_led + 1) % total_led; // 中心灯珠移动
			base_hue = (base_hue + 5) % 360;  // 整体色相偏移（颜色变化速度）
		}
		else break;
	}
}

void ALL_LED_Turnoff(void)
{
		// 熄灭所有灯珠
	for(uint8_t i=0; i<total_led; i++)
	{
			// 锟斤拷目锟斤拷锟斤拷椋盒达拷锟斤拷色锟斤拷熄锟斤拷
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
			RGB_WriteByte(0x00);
	}
		// 发送复位信号
	HAL_GPIO_WritePin(RGB_GPIO_PORT, RGB_GPIO_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);  // 延时
	
	HAL_Delay(20);
}	

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	
	LED_Color leds1[] = {
	{0,0xFF, 0xFF, 0x00},  // 0位置 黄色
	{2,0x00, 0x00, 0xFF},  // 2位置 蓝色
	{4,0x88, 0xFF, 0x88},  // 4位置 白色
	{6,0xFF, 0x00 , 0xFF}   // 6位置 紫色
	};
	
		LED_Color leds2[] = {
	{1,0x00, 0xFF, 0x00},  // 1位置 绿色
	{3,0xFF, 0x00, 0x00},   //3位置 红色
	{5,0xAA, 0xAA, 0xFF},   //5位置  
	{7,0x00, 0xFF , 0xFF}  // 7位置 青色

	};

	HAL_UART_Transmit(&huart1,(uint8_t *)&"LEDs Display!\r\n",strlen("ALL LED OFF!\r\n"),10); 
	sprintf(running_mode_str,"Display Mode = %d\r\n",rainbow_running_mode);
	HAL_UART_Transmit(&huart1,(uint8_t *)running_mode_str,strlen(running_mode_str),10); 
	
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		if(rainbow_running_flag == 1)    
		{

			if(rainbow_running_mode == 0)
			{
				RGB_GPIO_PORT = GPIOB;
				RGB_GPIO_PIN = GPIO_PIN_8;
				RGB_RainbowScroll();
			}
			else if(rainbow_running_mode == 1)
			{
				RGB_GPIO_PORT = GPIOB;
				RGB_GPIO_PIN = GPIO_PIN_9;
				RGB_Scroll_Gradient();
			}
			else if(rainbow_running_mode == 2)
			{
				RGB_GPIO_PORT = GPIOB;
				RGB_GPIO_PIN = GPIO_PIN_9;
				
				RGB_RainbowScroll();
			}
			else if(rainbow_running_mode == 3)
			{
				RGB_GPIO_PORT = GPIOB;
				RGB_GPIO_PIN = GPIO_PIN_8;
				RGB_Scroll_Gradient();
			}
			else if(rainbow_running_mode ==4)
			{

				RGB_GPIO_PORT = GPIOB;
				RGB_GPIO_PIN = GPIO_PIN_9;
				HAL_Delay(300);
				RGB_MultiDiffColorSet(leds1, 4, 8); // 0、2、4、6号灯亮，颜色不同
				HAL_Delay(300);
				RGB_MultiDiffColorSet(leds2, 4, 8); // 1、3、5、7号灯亮，颜色不同				
			}
		}
		else
		{
			RGB_GPIO_PORT = GPIOB;
			RGB_GPIO_PIN = GPIO_PIN_8;
			ALL_LED_Turnoff();
			HAL_Delay(200);
			RGB_GPIO_PORT = GPIOB;
			RGB_GPIO_PIN = GPIO_PIN_9;
			ALL_LED_Turnoff();
		}
		
		HAL_Delay(200);
		
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

/* USER CODE BEGIN 4 */
// 外部GPIO按键中断回调函数
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == KEY1_Pin)  // KEY1_PIN表示 开始
    {
        if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)  //确定是KEY1按键按下
        {
           rainbow_running_flag = 1;  // 显示标志位置1
					HAL_UART_Transmit(&huart1,(uint8_t *)&"Display ON!\r\n",strlen("Display ON!\r\n"),10); 
        }
    }
		
		if(GPIO_Pin == KEY2_Pin) // KEY2_PIN表示 停止
    {
        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)  //确定是KEY2按键按下
        {
           rainbow_running_flag = 0;  // 显示标志位置0
					HAL_UART_Transmit(&huart1,(uint8_t *)&"ALL LEDs OFF!\r\n",strlen("ALL LEDs OFF!\r\n"),10); 
        }
    }
		
		if(GPIO_Pin == KEY3_Pin)  // KEY3_PIN表示 模式切换
    {
        if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)  // 确定是KEY3按键按下
        {
           rainbow_running_mode += 1;  // 五种显示模式
					if(rainbow_running_mode>4)
						rainbow_running_mode = 0;
					sprintf(running_mode_str,"Display Mode = %d\r\n",rainbow_running_mode);
					HAL_UART_Transmit(&huart1,(uint8_t *)running_mode_str,strlen(running_mode_str),10); 
        }
    }
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
