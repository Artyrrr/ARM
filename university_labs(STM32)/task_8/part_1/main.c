#include "mcu_support_package/inc/stm32f10x.h"
#include "signal.h"

_Bool flag=0;
int volatile step = 0;

void TIM3_IRQHandler(void) {
    TIM_ClearFlag(TIM3,TIM_FLAG_Update);
    step++;
}



static void init_LED(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    GPIO_InitTypeDef DefC;
    DefC.GPIO_Mode = GPIO_Mode_AF_PP;
    DefC.GPIO_Pin = GPIO_Pin_8;
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &DefC);
}

static void LED_PWM(void) {
    while(1) {
        step = 0;
        int max_step = 70;
        int period = 10000;
        while (step != max_step) {
            if (flag) TIM_SetCompare3(TIM3,(step)*period/max_step);
            else TIM_SetCompare3(TIM3,(max_step - (step))*period/max_step);
        }
        flag = !flag;
    }
}

static void init_TIM(void) {
    //создание структуры для таймера
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  //Тактирование внутреннего мультиплексора
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE); //remap ножки для таймера TIM3, у ножки pc6 первый канал
    TIM_TimeBaseInitTypeDef TIM_InitStruct; 
	
    TIM_TimeBaseStructInit (&TIM_InitStruct);
    TIM_InitStruct.TIM_Period = 10000;
    TIM_InitStruct.TIM_Prescaler = 100;
    TIM_TimeBaseInit(TIM3,&TIM_InitStruct); //настройка таймера TIM3
	
    //создание структуры для канала
    TIM_OCInitTypeDef TIM_InitStructChannel;
    TIM_OCStructInit(&TIM_InitStructChannel);
    TIM_InitStructChannel.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_InitStructChannel.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC3Init(TIM3, &TIM_InitStructChannel); //настройка ножки для ШИМа
       
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn); //разрешаем прерывания для TIM3
	
    TIM_Cmd(TIM3, ENABLE); //включаем таймер  
    TIM_SetCompare1(TIM3, 0); //начальное заполнение
}


int main(void) {
    init_LED();
    init_TIM();
    LED_PWM();   
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
