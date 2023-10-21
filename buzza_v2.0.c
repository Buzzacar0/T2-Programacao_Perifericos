#include <ucx.h>

struct sem_s *adc_mtx;
struct sem_s1 *pwm_mtx;
struct sem_s2 *print_mtx;

struct pipe_s *p1, *p2, *p3, *p4, *p5;

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


void pwm_config()
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_OCInitTypeDef TIM_OCStruct;
	GPIO_InitTypeDef GPIO_InitStruct;

	/* Enable clock for TIM4 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	
	/* TIM4 by default has clock of 84MHz
	 * Update Event (Hz) = timer_clock / ((TIM_Prescaler + 1) * (TIM_Period + 1))
	 * Update Event (Hz) = 84MHz / ((839 + 1) * (999 + 1)) = 1000 Hz
	 */
	TIM_TimeBaseInitStruct.TIM_Prescaler = 839;
	TIM_TimeBaseInitStruct.TIM_Period = 999;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_CenterAligned1;
	
	/* TIM4 initialize */
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStruct);
	/* Enable TIM4 interrupt */
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
	/* Start TIM4 */
	TIM_Cmd(TIM4, ENABLE);
	
	/* Clock for GPIOB */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	/* Alternating functions for pins */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM4);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_TIM4);

	/* Set pins */
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* set OC mode */
	TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM2;
	TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
	
	/* set TIM4 CH3, 50% duty cycle */
	TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OCStruct.TIM_Pulse = 499;
	TIM_OC3Init(TIM4, &TIM_OCStruct);
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);

	/* set TIM4 CH4, 50% duty cycle */
	TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OCStruct.TIM_Pulse = 499;
	TIM_OC4Init(TIM4, &TIM_OCStruct);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
}

/* sensors parameters */
const float F_VOLTAGE = 635.0;		// 590 ~ 700mV typical diode forward voltage
const float T_COEFF = -2.0;		// 1.8 ~ 2.2mV change per degree Celsius
const float V_RAIL = 3300.0;		// 3300mV rail voltage
const float ADC_MAX = 4095.0;		// max ADC value
const int ADC_SAMPLES = 1024;		// ADC read samples
const int REF_RESISTANCE = 2300;

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

/* application threads */
void idle(void)
{
	while (1) {
	}
}

void task_1(void)
{
	float f;
	char fval[50];
	
	while (1) {
		/* critical section: ADC is shared! */
		ucx_wait(adc_mtx);
		adc_channel(ADC_Channel_8);
		f = temperature();
		ucx_signal(adc_mtx);
		
		ftoa(f, fval, 6);
		printf("temp: %s\n", fval);

		ucx_pipe_write(p1, fval, strlen((char *)fval) + 1);

		ucx_task_delay(50);
	}
}

void task_2(void)
{
	float f;
	char fval[50];
	
	while (1) {
		/* critical section: ADC is shared! */
		ucx_wait(adc_mtx);
		adc_channel(ADC_Channel_9);
		f = luminosity();
		ucx_signal(adc_mtx);
		
		ftoa(f, fval, 6);
		printf("lux: %s\n", fval);

		ucx_pipe_write(p2, fval, strlen((char *)fval) + 1);

		ucx_task_delay(50);
	}
}

void task_5(void)
{
	char fval[64];
	char fval1[64];
	char fval2[64];
	char trans1[64];
	char trans2[64];
	float data1;
	float data2;
	float tmp;
	float luxin; 
	
// LUX 80/1000
// TEMP 0/40

	while (1) {
		
			if(ucx_pipe_size(p1) < 1){

				fval1 = ucx_pipe_read(p1, fval, ucx_pipe_size(p1));

				tmp = atof(fval1 - 1);
				
				if(tmp == 0){
					data1 = 0;
				} else if (tmp == 10){
					data1 = 250; 
				} else if (tmp == 15){
					data1 = 375;
				}else if (tmp == 20){
					data1 = 500;
				}else if (tmp == 25){
					data1 = 625;
				}else if (tmp == 30){
					data1 = 750;
				}else if (tmp == 35){
					data1 = 875;
				}else {
					data1 = 999;
				}

				ftoa(data1, trans1, 6);

				/* write pipe - write size must be less than buffer size */
				ucx_pipe_write(p3, trans1, strlen((char *)trans1) + 1);

			}

			if(ucx_pipe_size(p2) < 1){

			/* read pipe - read size must be less than buffer size */
			fval2 = ucx_pipe_read(p2, fval, ucx_pipe_size(p2));

			luxin = atof(fval2 - 1);

			if(luxin == 0){
					data2 = 0;
				} else if (luxin == 80){
					data2 = 80; 
				} else if (luxin == 160){
					data2 = 160;
				}else if (luxin == 320){
					data2 = 320;
				}else if (luxin == 640){
					data2 = 640;
				}else if (luxin == 850){
					data2 = 850;
				}else {
					data2 = 999;
				}

			ftoa(data2, trans2, 6);
			
			/* write pipe - write size must be less than buffer size */
			ucx_pipe_write(p4, trans2, strlen((char *)trans2) + 1);

		}

	}

}

void task_3(void)
{
	printf("Oi to no 3!!\n");

	float duty = 0;
	char trans1[64];
	char fval1[64];

	
	pwm_config();
	
	while (1) { 
		while (ucx_pipe_size(p3) < 1);

			fval1 = ucx_pipe_read(p3, trans1, ucx_pipe_size(p3)); 

			duty = atof(fval1 - 1);

			while (duty < 999) {
				TIM4->CCR4 = duty;
				duty++;
				_delay_ms(1);
			}

			while (duty > 0) {
				TIM4->CCR4 = duty;
				duty--;
				_delay_ms(1);
			}
			
			ucx_task_delay(20);
		
		for(;;);
	}
}

void task_4(void)
{
	printf("T4 Comeca\n");

	float duty = 0;
	char fval2[64];
	char trans2[64];

	while (ucx_pipe_size(p4) < 1); 

		fval2 = ucx_pipe_read(p4, trans2, ucx_pipe_size(p4)); 

		duty = atof(fval2 - 1);

		TIM4->CCR3 = duty;
		ucx_task_delay(50);



	for(;;);
}



/* main application entry point */
int32_t app_main(void)
{
	ucx_task_add(idle, DEFAULT_STACK_SIZE);
	ucx_task_add(task_1, DEFAULT_STACK_SIZE);
	ucx_task_add(task_2, DEFAULT_STACK_SIZE);
	ucx_task_add(task_3, DEFAULT_STACK_SIZE);
	ucx_task_add(task_4, DEFAULT_STACK_SIZE);
	ucx_task_add(task_5, DEFAULT_STACK_SIZE);

	analog_config();
	adc_config();
	pwm_config();
	
	/* ADC mutex */
	adc_mtx = ucx_semcreate(1);

	/* PWM mutex */
	pwm_mtx = ucx_semcreate(1);

	/* PRINT mutex */
	print_mtx  = ucx_semcreate(1);

	p1 = ucx_pipe_create(64);
	p2 = ucx_pipe_create(64);
	p3 = ucx_pipe_create(64);
	p4 = ucx_pipe_create(64);

	// start UCX/OS, non-preemptive mode
	return 1;
}
