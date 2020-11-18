#include "mcu_support_package/inc/stm32f10x.h"

static int32_t freq;
static uint16_t f = 0;
static uint16_t counter=0;

void SysTick_Handler(void) {
    counter++;
    f=50000/freq;
    if(counter >= f) {
        GPIOC->ODR ^= (1<<6);
        counter=0;
    } 
}
    
static void init_periph(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    //динамик
    GPIO_InitTypeDef DefC;
    GPIO_StructInit(&DefC);
    DefC.GPIO_Mode = GPIO_Mode_Out_PP;
    DefC.GPIO_Pin = GPIO_Pin_6;
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &DefC);
    
    GPIO_InitTypeDef Def_COMP_Trig;
    GPIO_StructInit(&Def_COMP_Trig);
    Def_COMP_Trig.GPIO_Mode = GPIO_Mode_Out_PP;
    Def_COMP_Trig.GPIO_Pin = GPIO_Pin_1;
    Def_COMP_Trig.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &Def_COMP_Trig);
    
    GPIO_InitTypeDef Def_DAC_OUT;
    GPIO_StructInit(&Def_DAC_OUT);
    Def_DAC_OUT.GPIO_Mode =  GPIO_Mode_AIN;
    Def_DAC_OUT.GPIO_Pin = GPIO_Pin_4;
    Def_DAC_OUT.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &Def_DAC_OUT);
}

static void init_ADC(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2ENR_ADC1EN, ENABLE);
    
    //Настройка АЦП
    ADC_InitTypeDef ADC_InitStructure;
    ADC_StructInit(&ADC_InitStructure);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init (ADC1, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 1, ADC_SampleTime_239Cycles5);
    
    // Включаем АЦП
    ADC_Cmd(ADC1, ENABLE);
    // Сбрасываем калибровку
    ADC_ResetCalibration(ADC1);
    // Ждем окончания сброса
    while( ADC_GetResetCalibrationStatus(ADC1)) {;}
    // Запускаем калибровку
    ADC_StartCalibration(ADC1);
    // Ждем окончания калибровки
    while( ADC_GetCalibrationStatus(ADC1)) {;}
    
    SysTick_Config (SystemCoreClock/100000-1);
    __enable_irq();
}

static void Rangefinder(void) {
    uint16_t buffer[10];
    int temp;
    int final_value=0;
   
    while(1) {
        for(int i=0; i<10; i++) {
            ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    
            while((ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC))!=SET) {;}
	  
            ADC_ClearITPendingBit(ADC1, ADC_IT_EOC); 
            buffer[i]=ADC_GetConversionValue(ADC1);
            
            GPIO_ResetBits(GPIOA, GPIO_Pin_1);
            ADC_SoftwareStartConvCmd(ADC1, ENABLE);
            final_value+=buffer[i];      
        }
	  
        final_value = final_value/10;
        final_value = (uint32_t)(((double)final_value-23.11)/4.98);
        temp = final_value%5;
        final_value -= temp;
    
        if(final_value <= 10) {
            freq = 1000;
        }
        if(final_value >= 100) {
            freq = 10;
        }
        //сопоставление расстояния и частоты
        if((final_value <= 100)&&(final_value >= 10)) {
            freq = -11*final_value+1110;
        }
    }
}

int main(void) {
    init_periph();
    init_ADC();
    Rangefinder();
    return 0; 
}

// В Project->Options->Linker, Scatter File выбран файл stack_protection.sct
// он обеспечивает падение в HardFault при переполнении стека
// Из-за этого может выдаваться ложное предупреждение "AppData\Local\Temp\p2830-2(34): warning:  #1-D: last line of file ends without a newline"

#ifdef USE_FULL_ASSERT
// эта функция вызывается, если assert_param обнаружил ошибку
void assert_failed(uint8_t * file, uint32_t line) {
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    (void)file;
    (void)line;
    __disable_irq();
    while(1) {
        // это ассемблерная инструкция "отладчик, стой тут"
        // если вы попали сюда, значит вы ошиблись в параметрах вызова функции из SPL.
        // Смотрите в call stack, чтобы найти ее
        __BKPT(0xAB);
    }
}
#endif
