#include "mcu_support_package/inc/stm32f10x.h"
//выполнено основное и одно дополнительное задание (кратковременное и длительное нажатие)

volatile static int delay = 0;

//Обработчик прерываний
void SysTick_Handler(void) {
    delay++;
}

//Функция задержки. Отсчёт в микросекундах
void Delay_Func(int time) {
    int start = delay;
    while ((delay - start) < time);
}

//Функция подачи звука на динамик
void Sound(int rows, int cols, int *freq) {
    int countdown = delay;
    while(!(GPIOA->IDR&(1<<rows))) { //пока кнопка нажата, инвертируется ножка динамика
        GPIOB->ODR ^= 1<<10;
        Delay_Func(1000000/(2*freq[(cols-1)+3*(rows-1)])); 
        while(((delay - countdown) >= 600000)&&(!(GPIOA->IDR&(1<<rows)))) { //задержка ~3 секунды, после которой устанавливается другая частота
            GPIOB->ODR ^= 1<<10;
            Delay_Func(1000000/(3*freq[(cols-1)+3*(rows-1)])); //подача более высокой частоты (задержка больше)
        }
    }
}

//Функция сканирования клавиатуры
void Scan(int *freq) {
    for(int cols = 1; cols < 4; cols++) { 
        GPIO_ResetBits(GPIOC, 1<<cols);
        for(volatile int i = 0; i < 10000; i++); //пауза между сканированиями 
        for(int rows = 1; rows < 5; rows++) {
            Sound(rows, cols, freq); //вызов и передача параметров строк, столбов и элементов массива в функцию Sound
        }
        GPIO_SetBits(GPIOC, 1<<cols);
    }
}

int main(void) {
    __disable_irq();
    SysTick_Config((SystemCoreClock / 1000000) + 1);
    __enable_irq();
        
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    //столбы в режиме output open drain
    GPIO_InitTypeDef DefC;
    DefC.GPIO_Mode = GPIO_Mode_Out_OD;
    DefC.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC,&DefC);
    GPIO_SetBits(GPIOC,(GPIO_Pin_1) | (GPIO_Pin_2) | (GPIO_Pin_3)); //set cols to 1
    
    //строки в режиме input pull up
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode = GPIO_Mode_IPU;
    DefA.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&DefA);

    //динамик в режиме push-pull
    GPIO_InitTypeDef DefB;
    DefB.GPIO_Mode = GPIO_Mode_Out_PP;
    DefB.GPIO_Pin = GPIO_Pin_10;
    DefB.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB,&DefB);

    /*int freq[12] = {698,784,830,932,1046,1108,1244,1396,784,698,659,587};//частота: ДО, РЕ, МИ, ФА, СОЛЬ, ЛЯ, СИ, ЛЯ, СОЛЬ, ФА, МИ, РЕ
    //1Фа+,2Со,3Со+,4Ля+,5Д, 6Д+, 7Р+,8Фа++  для коробейников*/ 
    int freq[12] = {523,587,659,698,784,880,987,880,784,698,659,587};//частота: ДО, РЕ, МИ, ФА, СОЛЬ, ЛЯ, СИ, ЛЯ, СОЛЬ, ФА, МИ, РЕ
    while(1) {
        Scan(freq);
    }
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
