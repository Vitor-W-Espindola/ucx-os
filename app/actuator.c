#include <ucx.h>
#include <ieee754.h>
#include <math.h>

/* synchronization */
struct sem_s *adc_mtx;
struct sem_s *pwm_mtx;
struct sem_s *luminosity_sem_adc, *luminosity_sem_pwm, *temperature_sem_adc, *temperature_sem_pwm;

/* ADC library */
void analog_config();
void adc_config(void);
void adc_channel(uint8_t channel);
uint16_t adc_read();

/* PWM library */
void pwm_config();

/* sensors parameters */
const float F_VOLTAGE = 606.0;		// 590 ~ 700mV typical diode forward voltage
const float T_COEFF = -2.0;		// 1.8 ~ 2.2mV change per degree Celsius
const float V_RAIL = 3300.0;		// 3300mV rail voltage
const float ADC_MAX = 4095.0;		// max ADC value
const int ADC_SAMPLES = 1024;		// ADC read samples
const int REF_RESISTANCE = 4656;

/* timing parameters */
const int capture_delay_us = 5;

void pwm_config()
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_OCInitTypeDef TIM_OCStruct;
	GPIO_InitTypeDef GPIO_InitStruct;

	/* Enable clock for TIM5 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	
	/* TIM4 (je ne sais pas about TIM5) by default has clock of 84MHz
	 * Update Event (Hz) = timer_clock / ((TIM_Prescaler + 1) * (TIM_Period + 1))
	 * Update Event (Hz) = 84MHz / ((839 + 1) * (999 + 1)) = 1000 Hz
	 */
	TIM_TimeBaseInitStruct.TIM_Prescaler = 839;
	TIM_TimeBaseInitStruct.TIM_Period = 999;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_CenterAligned1;
	
	/* TIM5 initialize */
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseInitStruct);
	/* Enable TIM5 interrupt */
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);
	/* Start TIM5 */
	TIM_Cmd(TIM5, ENABLE);
	
	/* Clock for GPIOB */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* Alternating functions for pins */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM5);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM5);

	/* Set pins */
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* set OC mode */
	TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM2;
	TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
	
	/* set TIM5 CH1, 50% duty cycle */
	TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OCStruct.TIM_Pulse = 499;
	TIM_OC1Init(TIM5, &TIM_OCStruct);
	TIM_OC1PreloadConfig(TIM5, TIM_OCPreload_Enable);

	/* set TIM5 CH2, 50% duty cycle */
	TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OCStruct.TIM_Pulse = 499;
	TIM_OC2Init(TIM5, &TIM_OCStruct);
	TIM_OC2PreloadConfig(TIM5, TIM_OCPreload_Enable);
}

void analog_config()
{
	/* GPIOC Peripheral clock enable. */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	/* Init GPIOB for ADC input */
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void adc_config(void)
{
	/* Enable clock for ADC1 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* Init ADC1 */
	ADC_InitTypeDef ADC_InitStruct;
	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStruct.ADC_ExternalTrigConv = DISABLE;
	ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStruct.ADC_NbrOfConversion = 1;
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStruct.ADC_ScanConvMode = DISABLE;
	ADC_Init(ADC1, &ADC_InitStruct);
	ADC_Cmd(ADC1, ENABLE);
}

void adc_channel(uint8_t channel)
{
	/* select ADC channel */
	ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_84Cycles);
}

uint16_t adc_read()
{
	/* Start the conversion */
	ADC_SoftwareStartConv(ADC1);
	/* Wait until conversion completion */
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	/* Get the conversion value */
	return ADC_GetConversionValue(ADC1);
}

/* sensor aquisition functions */
float temperature()
{
	float temp = 0.0;
	float voltage;
	
	for (int i = 0; i < ADC_SAMPLES; i++) {
		voltage = adc_read() * (V_RAIL / ADC_MAX);
		temp += ((voltage - F_VOLTAGE) / T_COEFF);
	}
	
	return (temp / ADC_SAMPLES);
}

float luminosity()
{
	float voltage, lux = 0.0, rldr;
	
	for (int i = 0; i < ADC_SAMPLES; i++) {
		voltage = adc_read() * (V_RAIL / ADC_MAX);
		rldr = (REF_RESISTANCE * (V_RAIL - voltage)) / voltage;
		lux += 500 / (rldr / 650);
	}
	
	return (lux / ADC_SAMPLES);
}

/* shared data */
char data_luminosity[64];
char data_temperature[64];

/* application threads */
void task_head(void)
{
    while (1) {	
	/* Luminosity Reading and Writing */
        ucx_sem_wait(luminosity_sem_adc);
        
        ucx_sem_signal(luminosity_sem_pwm);

	/* Temperature Reading and Writing */
        ucx_sem_wait(temperature_sem_adc);
        
        ucx_sem_signal(temperature_sem_pwm);
    }
}

/* ADC - Temperature */
void task_temperature_adc(void)
{
    float f;
    char fval[50];

    while (1) {
        /* critical section: ADC is shared! */
        ucx_sem_wait(adc_mtx);
        adc_channel(ADC_Channel_8);
        f = temperature();
        ucx_sem_signal(adc_mtx);

        ftoa(f, data_temperature, 6);
        ucx_sem_signal(temperature_sem_adc); 

        ftoa(f, fval, 6);
        printf("temp: %s\n", fval);

        ucx_task_delay(capture_delay_us);
    }
}

/* ADC - Luminosity */
void task_luminosity_adc(void)
{
    float f;
    char fval[50];

    while (1) {
        /* critical section: ADC is shared! */
        ucx_sem_wait(adc_mtx);
        adc_channel(ADC_Channel_9);
        f = luminosity();
        ucx_sem_signal(adc_mtx);

        ftoa(f, data_luminosity, 6);
        ucx_sem_signal(luminosity_sem_adc); 

        ftoa(f, fval, 6);
        printf("lux: %s\n", fval);

        ucx_task_delay(capture_delay_us);
    }
}

/* PWM - Temperature LED */
void task_temperature_pwm(void)
{
    char temperature_val[50];
    float temperature = 0.0;

    while (1) {
        /* Waiting for data from head task */
        ucx_sem_wait(temperature_sem_pwm);
        temperature = (atof(data_temperature) / 40) * 1000;

        ftoa(temperature, temperature_val, 6);
        printf("temperature: %s\n", temperature_val);

        ucx_sem_wait(pwm_mtx);
        if (temperature <= 0) TIM5->CCR1 = 0;
        else if (temperature >= 999) TIM5->CCR1 = 999;
        else TIM5->CCR1 = temperature;
        ucx_sem_signal(pwm_mtx);
    }
}

/* PWM - Luminosity LED */
void task_luminosity_pwm(void)
{
    char luminosity_val[50];
    float luminosity = 0.0;

    while (1) {
        /* Waiting for data from head task */
        ucx_sem_wait(luminosity_sem_pwm);
        luminosity = (1 - (atof(data_luminosity) / 100)) * 1000;

        ftoa(luminosity, luminosity_val, 6);
        printf("luminosity: %s\n", luminosity_val);

        ucx_sem_wait(pwm_mtx);
        if (luminosity <= 100) TIM5->CCR2 = 0;
        else if (luminosity >= 900) TIM5->CCR2 = 999;
        else TIM5->CCR2 = luminosity;
        ucx_sem_signal(pwm_mtx);
    }
}

/* main application entry point */
int32_t app_main(void)
{
    analog_config();
    adc_config();
    pwm_config();

    adc_mtx = ucx_sem_create(5, 1);
    pwm_mtx = ucx_sem_create(5, 1);

    luminosity_sem_adc = ucx_sem_create(10, 0); 
    luminosity_sem_pwm = ucx_sem_create(10, 0);
    temperature_sem_adc = ucx_sem_create(10, 0);
    temperature_sem_pwm = ucx_sem_create(10, 0);

    ucx_task_add(task_head, DEFAULT_STACK_SIZE);
    ucx_task_add(task_temperature_adc, DEFAULT_STACK_SIZE);
    ucx_task_add(task_luminosity_adc, DEFAULT_STACK_SIZE);
    ucx_task_add(task_temperature_pwm, DEFAULT_STACK_SIZE);
    ucx_task_add(task_luminosity_pwm, DEFAULT_STACK_SIZE);

    return 1;
}
