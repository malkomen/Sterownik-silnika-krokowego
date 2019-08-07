#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include "stm32f10x_usart.h"


#define PRESS 0
#define CHANNEL_SPEED 0


typedef unsigned int frequency_Hz;

typedef enum {LEFT = 0, RIGHT = !LEFT} dir_t;
typedef enum {AUTO = 0, MANUAL = !AUTO} stat_t;





char txtbuf[1024];

void delay_ms(int d)
{
	d = d * 5500;
	while(d)
	{
		d--;
	}
}

void delay_us(int d)
{
	d = d * 5.5;
	while(d)
	{
		d--;
	}
}

void uartPutS(char* str)
{
	while(*str){
		  USART_SendData(USART1, *str++);

		  while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET){
		  }
	}
}
/*
void printk(const char *__fmt, ...){
	va_list __ap;
	va_start(__ap, __fmt);
	vsniprintf(txtbuf, 1024, __fmt, __ap);
	uartPutS(txtbuf);
	va_end(__ap);
}
*/
static void initDriverEngine()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	//Engine step, Engine dir, Engine enable
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOB, GPIO_Pin_2); //dir
	GPIO_SetBits(GPIOB, GPIO_Pin_6); //enable
}

/*
 *
 * brief: konfiguracja panala sterujacego
 */
static void initControlPanel()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO| RCC_APB2Periph_ADC1, ENABLE);

	//kontrolki: praca ciag³a w lewo, praca ciag³a w prawo, praca 5000 krokow w lewo, praca 5000 krokow w prawo
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//konfiguracja wejsc: ADC_Speed,
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//konfiguracja przetwornika ADC, kana³ 1
	ADC_InitTypeDef ADC_InitStructure;
	// Jeden przetwornik, pracujacy niezaleznie
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	// Pomiar dwóch kanalów, wlacz opcje skanowania
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	// Wy³acz pomiar w trybie ciaglym
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	// Nie bedzie wyzwalania zewnetrznego
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	// Dane wyrownane do prawej - znaczacych bedzie 12 mlodszych bitow
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	// Jeden kana³
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	// Inicjuj przetwornik
	ADC_Init(ADC1, &ADC_InitStructure);
	// Grupa regularna, czas probkowania 28,5 cykla czyli???
	//ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
	//ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

	// Wlacz ADC1
	ADC_Cmd(ADC1, ENABLE);

	// Resetuj rejestry kalibracyjne
	ADC_ResetCalibration(ADC1);
	// Czekaj, az skonczy resetowac
	while(ADC_GetResetCalibrationStatus(ADC1));

	// Start kalibracji ADC1
	ADC_StartCalibration(ADC1);
	// Czekaj na zakonczenie kalibracji ADC1
	while(ADC_GetCalibrationStatus(ADC1));

	// Start przetwarzania
	//ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*
 *
 * brief: konfiguracja terminana znakowego
 */
static void initConsole()
{
  	// Konfiguracja PA9 jako Tx
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1, ENABLE);
	GPIO_InitTypeDef  GPIO_InitStructure;
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitTypeDef USART_InitStructure;
	// Konfiguracja USART
  	USART_InitStructure.USART_BaudRate = 9600;
  	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  	USART_InitStructure.USART_StopBits = USART_StopBits_1;
  	USART_InitStructure.USART_Parity = USART_Parity_No;
  	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  	USART_InitStructure.USART_Mode =  USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	//USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);

}

/*
 *
 * brief: odczyt pomiaru z ADC
 */
static int adcRead(uint8_t channel)
{

	ADC_RegularChannelConfig(ADC1, channel, 2, ADC_SampleTime_239Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) != SET);
	ADC_SoftwareStartConvCmd(ADC1, DISABLE);

	return ADC_GetConversionValue(ADC1);

}

/*
 *
 * brief: odczyt stanu przycisku SW1 praca ci¹g³a w lewo
 */
uint8_t SW1_constLeftStat()
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);
}

/*
 *
 * brief: odczyt stanu przycisku SW1 praca ci¹g³a w prawo
 */
uint8_t SW2_constRightStat()
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7);
}

/*
 *
 * brief: odczyt stanu przycisku SW3 praca 5000 kroków w lewo
 */
uint8_t SW3_tempLeftStat()
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
}

/*
 *
 * brief: odczyt stanu przycisku SW4 praca 5000 kroków w prawo
 */
uint8_t SW4_tempRightStat()
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5);
}

/*
 *
 * brief: Ustawienie kierunku pracy silnika
 */
static void setEngineDir(uint8_t dir)
{
	switch (dir)
	{
		case RIGHT:
			GPIO_SetBits(GPIOB, GPIO_Pin_2);
			break;
		case LEFT:
			GPIO_ResetBits(GPIOB, GPIO_Pin_2);
			break;
	}
}

/*
 *
 * brief: Za³aczenie/wy³aczenie pr¹du na cewki silnika
 */
static void setEngineStatus(uint8_t status)
{
	switch (status)
	{
		case ENABLE:
			GPIO_ResetBits(GPIOB, GPIO_Pin_6);
			break;
		case DISABLE:
			GPIO_SetBits(GPIOB, GPIO_Pin_6);
			break;
	}
}

/*
 *
 * brief: Jeden krok silnika (parametr prêdkosc 0-100)
 */
static void engineStep(uint16_t speed)
{
	if (speed > 100) speed = 100;

	speed = (110 - speed)*100;

	GPIO_SetBits(GPIOB, GPIO_Pin_1);
	delay_us(speed);
	GPIO_ResetBits(GPIOB, GPIO_Pin_1);
	delay_us(speed);
}



/*
 *
 * brief: Uchwyt sterowania silnikiem (wywo³ywany cyklicznie)
 */
static void handlerEngine()
{
	int speed = (adcRead(CHANNEL_SPEED)/40);

	if(SW1_constLeftStat() == RESET)
	{
		setEngineDir(LEFT);
		setEngineStatus(ENABLE);
		while(SW1_constLeftStat() == RESET)
		{
			engineStep(speed);
		}
		setEngineStatus(DISABLE);
	}

	if(SW2_constRightStat() == RESET)
	{
		setEngineDir(RIGHT);
		setEngineStatus(ENABLE);
		while(SW2_constRightStat() == RESET)
		{
			engineStep(speed);
		}
		setEngineStatus(DISABLE);
	}

	if(SW3_tempLeftStat() == RESET)
	{
		uint16_t licznik = 0;
		setEngineDir(LEFT);
		setEngineStatus(ENABLE);
		while(licznik < 5000)
		{
			engineStep(speed);
			licznik++;
		}
		setEngineStatus(DISABLE);
	}
	if(SW4_tempRightStat() == RESET)
	{
		uint16_t licznik = 0;
		setEngineDir(RIGHT);
		setEngineStatus(ENABLE);
		while(licznik < 5000)
		{
			engineStep(speed);
			licznik++;
		}
		setEngineStatus(DISABLE);
	}

}

void handlerTemp()
{
	int i = 0;

	for(i=0;i<5;i++){};
}

/*
 *
 *  brief: Funkcja g³ówna
 */
int main(void)
{
	//initConsole();
	initDriverEngine();
	initControlPanel();


    while(1)
    {
    	handlerEngine();

    	handlerTemp();

       	delay_ms(100);

    }
}
