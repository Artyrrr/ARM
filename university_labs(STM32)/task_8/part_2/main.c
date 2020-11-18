#include "mcu_support_package/inc/stm32f10x.h"
#include "signal.h"
#include "math.h"

const double pi = 3.14;
static volatile int16_t sinus[1200]; //массив для позиций функции синуса
volatile uint32_t sin_pos[8] = {0, 0, 0, 0, 0, 0, 0, 0}; //шаги перемещения по массиву синуса
const uint32_t freq[8] = {1046, 1175, 1244, 1318, 1397, 1568, 1760, 1975}; //частота нот третьей октавы
_Bool butt_on[8] = {0, 0, 0, 0, 0, 0, 0, 0}; //если определённая кнопка нажата - соответствующий элемент меняется с нуля на единицу
const uint32_t correction = 42; //поправочный коэффициент для управления частотой, увеличивая - уменьшаем частоту
 
void TIM3_IRQHandler(void) {
    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
}
 
void SysTick_Handler(void) {
    __disable_irq();
    int8_t num_butt_on = 0; //количество одновременно нажатых клавиш
    int32_t filling = 0; //отвечает за заполнение
    for (int8_t i = 0; i <= 7; i++) {
        if (sin_pos[i] >= (1200 * correction)) //обнуление текущее положение синуса, если он вышел за границы массива   
            sin_pos[i] = 0;
    }
 
    for (int8_t i = 0; i <= 7; i++) {
        if (butt_on[i]) { //если кнопка нажата, в заполнение добавляется текущее значение синуса соответствующей частоты
            filling += sinus[sin_pos[i] / correction]; 
            num_butt_on++;
        }
        sin_pos[i] += freq[i]; //в шаг добавляется значение частоты соответствующего синуса
    }
    filling = filling / num_butt_on; //нормирование амплитуды синуса
    TIM_SetCompare1(TIM3, (150 * (100 + filling))/100); //заполнение задаётся от центра по синусоидальной функции
    __enable_irq();
}
 

static void init_periph(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
 
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode = GPIO_Mode_IPD;
    DefA.GPIO_Pin = (GPIO_Pin_1) | (GPIO_Pin_2) | (GPIO_Pin_3) | (GPIO_Pin_4) | (GPIO_Pin_5) | (GPIO_Pin_6) | (GPIO_Pin_7);
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &DefA); //настройка клавиш
    
    GPIO_InitTypeDef DefC;
    DefC.GPIO_Mode = GPIO_Mode_AF_PP;
    DefC.GPIO_Pin = GPIO_Pin_6;
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &DefC); //динамик
    
    DefC.GPIO_Mode = GPIO_Mode_IPD;
    DefC.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOC, &DefC);
}

static void init_sin(void) {
    //заполнение массива синуса значениями одного периода
    for (int32_t i = 0; i < 1200; i++) sinus[i] = 100 * sin(i * pi / 600); 
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE); //remap ножки 
    TIM_TimeBaseInitTypeDef TIM_InitStruct;
    
    TIM_TimeBaseStructInit(&TIM_InitStruct);
    TIM_InitStruct.TIM_Prescaler = 1; 
    TIM_InitStruct.TIM_Period = 300; 
    TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Down;
    TIM_TimeBaseInit(TIM3, &TIM_InitStruct); //настройка таймера TIM3
     
    //создание структуры для канала
    TIM_OCInitTypeDef TIM_InitStructChannel;
    TIM_OCStructInit(&TIM_InitStructChannel);
    TIM_InitStructChannel.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_InitStructChannel.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC1Init(TIM3, &TIM_InitStructChannel); //настройка ножки для ШИМа
    
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn); //разрешаем прерывания для TIM3
    
    TIM_Cmd(TIM3, ENABLE); //включаем таймер 
    TIM_SetCompare1(TIM3, TIM_InitStruct.TIM_Period / 2); //начальное заполнение
    __disable_irq();
    SysTick_Config((SystemCoreClock / 51100) - 1); //настройка SysTick
    __enable_irq();
   
    while (1) {
        for (uint32_t cols = 1; cols <= 7; cols++) { //проход по кнопкам порта A
            if ((GPIOA -> IDR & (1 << cols)))
                butt_on[cols - 1] = 1; //если кнопка нажата - записать в глобальный массив
            else butt_on[cols - 1] = 0;
        }
 
        if ((GPIOC ->IDR & (1 << 4))) butt_on[7] = 1; //кнопка порта C
        else butt_on[7] = 0;
    }
}

int main(void) {
    init_periph();
    init_sin();
    return 0;
}
 
// В Project->Options->Linker, Scatter File выбран файл stack_protection.sct
// он обеспечивает падение в HardFault при переполнении стека
// Из-за этого может выдаваться ложное предупреждение "AppData\Local\Temp\p2830-2(34): warning: #1-D: last line of file ends without a newline"
#ifdef USE_FULL_ASSERT
 
// эта функция вызывается, если assert_param обнаружил ошибку
void assert_failed(uint8_t * file, uint32_t line) {
    /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
 
    (void) file;
    (void) line;
 
    __disable_irq();
    while (1) {
        // это ассемблерная инструкция "отладчик, стой тут"
        // если вы попали сюда, значит вы ошиблись в параметрах вызова функции из SPL.
        // Смотрите в call stack, чтобы найти ее
        __BKPT(0xAB);
    }
}
 
#endif