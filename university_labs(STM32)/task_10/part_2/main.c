#include "mcu_support_package/inc/stm32f10x.h"
 
volatile int32_t ADC_values[11];
static volatile int count=0;
static float x_values[11]= {0};
static float y_values[11]= {0};
static float z_values[11]= {0};

int volatile full=0; //если 1 - набрано 10 значений, можно взять медиану

//функция задержки
void delay(uint32_t i) {
    for (volatile uint32_t j=0; j<i; j++);
}

static void init_periph(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
           
    GPIO_InitTypeDef DefC;
    DefC.GPIO_Mode = GPIO_Mode_Out_PP;
    DefC.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC,&DefC);
    
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode =  GPIO_Mode_AIN;
    DefA.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&DefA);
}

static void init_ADC_DMA(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    
    
    DMA_DeInit(DMA1_Channel1);
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_PeripheralBaseAddr=(uint32_t)&ADC1->DR; //откуда берём
    DMA_InitStructure.DMA_MemoryBaseAddr=(uint32_t)&ADC_values[0]; //куда кладём
    DMA_InitStructure.DMA_DIR=DMA_DIR_PeripheralSRC; //периферии -> память
    DMA_InitStructure.DMA_BufferSize = 3; 
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; //адрес переферии не инкрементировать
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; //инкрементировать указатели на данные в памяти соответственно
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word; //размер результата преобразования 2 байта
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word; //размер ячейки в памяти 2 байта
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; // зацикливаем работу DMA
    DMA_InitStructure.DMA_Priority = DMA_Priority_High; //приоритет для канала DMA
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable; //используем ли передачу память->память
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    
    DMA_Cmd(DMA1_Channel1, ENABLE);
    /*Разрешение прерываний*/
    DMA_ITConfig(DMA1_Channel1, DMA1_IT_TC1, ENABLE);
    
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    ADC_InitTypeDef ADC_InitStructure;
    ADC_StructInit(&ADC_InitStructure);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;  
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 3;
    ADC_Init ( ADC1, &ADC_InitStructure);
    
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5); 
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_28Cycles5); 
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_28Cycles5); 
    
    ADC_Cmd (ADC1,ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    
           
    //Калибровка АЦП
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
    
    ADC_SoftwareStartConvCmd(ADC1,ENABLE);
}

void DMA1_Channel1_IRQHandler() {
    DMA_ClearITPendingBit(DMA1_IT_TC1);
    x_values[count]=ADC_values[0];
    y_values[count]=ADC_values[1];
    z_values[count]=ADC_values[2];
    count++;
    if (count==11) {
        count=0;
        full=1;
    }
    ADC_SoftwareStartConvCmd(ADC1,ENABLE);
} 
 
float Median(float * arr, int size) {
    int i, j;
    int z = 0;
    for (i=0; (i < size) && (z != size/2); i++) {
        z=0;
        for (j=0; j< size; j++) {
            if (arr[i] >= arr[j] && j!=i) {
                z++;
            }
        }
    }		
    return arr[i-1];
}

void Accelerometer() {
    float sens=0.91;
    
    float axis_x; //ускорения по осям
    float axis_y;
    float axis_z;
    
    float axis_x_calibration;
    float axis_y_calibration;
    float axis_z_calibration;
    
    float axis_x_max = 0.342; //sin(20)
    float axis_y_max = 0.342;
    
    float axis_x_min = -0.342;
    float axis_y_min = -0.342;
    
    delay(30000);
    
    while(!full) {;}
    
    delay(30000);
    
    axis_x_calibration = (Median(x_values, 11))*3.3f/4095;
    axis_y_calibration = (Median(y_values, 11))*3.3f/4095;
    axis_z_calibration = (Median(z_values, 11))*3.3f/4095; 
       
    while(1) {
        axis_x = ((Median(x_values, 11))*3.3f/4095 - axis_x_calibration)/sens;
        axis_y = ((Median(y_values, 11))*3.3f/4095 - axis_y_calibration)/sens;
        axis_z = ((Median(z_values, 11))*3.3f/4095 - axis_z_calibration)/sens;
        
        if (axis_x > axis_x_max) GPIO_SetBits(GPIOC,GPIO_Pin_2);    
        else GPIO_ResetBits(GPIOC,GPIO_Pin_2);
        
        if (axis_x < axis_x_min) GPIO_SetBits(GPIOC,GPIO_Pin_0); 
        else GPIO_ResetBits(GPIOC,GPIO_Pin_0);
        
        if (axis_y > axis_y_max) GPIO_SetBits(GPIOC,GPIO_Pin_3);    
        else GPIO_ResetBits(GPIOC,GPIO_Pin_3);
        
        if (axis_y < axis_y_min) GPIO_SetBits(GPIOC,GPIO_Pin_1); 
        else GPIO_ResetBits(GPIOC,GPIO_Pin_1);            
    }
}

int main(void) {
    init_periph();
    init_ADC_DMA();
    Accelerometer();
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
