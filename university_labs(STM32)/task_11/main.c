#include "mcu_support_package/inc/stm32f10x.h"

#include <app_cfg.h>

#include <ucos_ii.h>

#include <cpu.h>
#include <lib_def.h>
#include <lib_mem.h>
#include <lib_str.h>
#include "stdbool.h"
#include <bsp.h>

#define APP_ASSERT( statement ) do { if(! (statement) ) { __disable_irq(); while(1){ __BKPT(0xAB); if(0) break;} }  } while(0)

/***************************************************************************************************
                                 Стеки для тасков
 ***************************************************************************************************/

static OS_STK App_TaskStartStack[APP_TASK_START_STACK_SIZE];

static OS_STK App_ButtonStack[APP_TASK_BUTTON_STACK_SIZE];

static OS_STK App_TaskLed8Stack[APP_TASK_LED8_STACK_SIZE];

static OS_STK App_TaskScanKeyboardStack[APP_TASK_KEYBOARD_STACK_SIZE];

static OS_STK App_TaskPlaySoundStack[APP_TASK_SOUND_STACK_SIZE];

static OS_STK App_TaskSendButtonNumStack[APP_TASK_SEND_BUTTON_NUM_STACK_SIZE];

/***************************************************************************************************
                                 Таски - объявления
 ***************************************************************************************************/

static void App_TaskStart( void * p_arg );

static void App_TaskButton( void * p_arg );// сканирование кнопки PA0

static void App_TaskLed8( void * p_arg ); // зажигание светодиода PC8 по нажатию PA0

static void App_TaskScanKeyboard( void * p_arg ); // сканирование клавиатуры

static void App_TaskPlaySound( void * p_arg ); // играть звук согласно нажатой кнопке

static void App_TaskSendButtonNum( void * p_arg ); // отправлять номер нажатой клавиши
//на клавиатуре по USART
OS_EVENT * semaphore;// указатель на семафор
OS_EVENT * queue;// указатель на очередь
#define QUEUE_SIZE 128
static void * queueStorage[ QUEUE_SIZE ]; // массив, в котором будут лежать сообщения

int main(void) {

    // запрет прерываний и инициализация ОС 
    BSP_IntDisAll();
    OSInit();

    // создание стартового таска - в нем создаются все остальные
    // почему не создавать всё сразу в main'e?
    // возможно, вы хотите создавать их в зависимости от каких-нибудь условий,
    // для которых уже должна работать ОС
    
    INT8U res = OSTaskCreate( 
                    App_TaskStart,     // указатель на таск         
                    NULL,              // параметр вызова (без параметра)
                    &App_TaskStartStack[APP_TASK_START_STACK_SIZE - 1], // указатель на стек
                    APP_TASK_START_PRIO // приоритет
                );


    
    APP_ASSERT( res == OS_ERR_NONE );

    // запуск многозадачности
    OSStart();

    // до сюда выполнение доходить не должно
    return 0;

}

/***************************************************************************************************
                                 Таски
 ***************************************************************************************************/

// стартовый таск
static void App_TaskStart(void * p_arg) {
    // это чтобы убрать warning о неиспользуемом аргументе
    (void)p_arg;

    //  Фактически - только настройка RCC - 72 МГц от ФАПЧ  (Initialize BSP functions)
    BSP_Init();

    // настройка СисТика
    OS_CPU_SysTickInit();

    // таск для сбора статистики - если он нужен                            
#if (OS_TASK_STAT_EN > 0)
    OSStatInit();
#endif

    semaphore = OSSemCreate(1); // создание семофора
    queue = OSQCreate( queueStorage, QUEUE_SIZE ); // создание очереди 

    BSP_IntInit();//для прерываний

    // дальше создаются пользовательские таски
    
    INT8U res;

    res = OSTaskCreate( 
              App_TaskButton, // указатель на функцию       
              NULL,            // параметр - без параметра              
              &App_ButtonStack[APP_TASK_BUTTON_STACK_SIZE - 1], // указатель на массив для стека  
              APP_TASK_BUTTON_PRIO // приоритет
          );
                
    APP_ASSERT( res == OS_ERR_NONE );

    res = OSTaskCreate( 
              App_TaskLed8,
              NULL,
              &App_TaskLed8Stack[APP_TASK_LED8_STACK_SIZE - 1], 
              APP_TASK_LED8_PRIO 
          );
                
    APP_ASSERT( res == OS_ERR_NONE );

    res = OSTaskCreate(  
              App_TaskScanKeyboard, // указатель на функцию 
              NULL, // параметр - без параметра 
              &App_TaskScanKeyboardStack[APP_TASK_KEYBOARD_STACK_SIZE - 1], // указатель на массив для стека 
              APP_TASK_SCAN_KEYBOARD_PRIO // приоритет
          );
					 
    APP_ASSERT(res == OS_ERR_NONE);
		
    res = OSTaskCreate(
              App_TaskPlaySound, // указатель на функцию 
              NULL, // параметр - без параметра 
              &App_TaskPlaySoundStack[APP_TASK_SOUND_STACK_SIZE - 1], // указатель на массив для стека 
              APP_TASK_PLAY_SOUND_PRIO // приоритет
          );

    APP_ASSERT( res == OS_ERR_NONE );
		
    res = OSTaskCreate(
              App_TaskSendButtonNum, // указатель на функцию 
              NULL, // параметр - без параметра 
              &App_TaskSendButtonNumStack[APP_TASK_SEND_BUTTON_NUM_STACK_SIZE - 1], // указатель на массив для стека 
              APP_TASK_SEND_BUTTON_NUM_PRIO // приоритет
          );

    APP_ASSERT( res == OS_ERR_NONE );

    // этот таск больше не нужен 
    OSTaskDel (OS_PRIO_SELF);
					 
}

static bool is_button_on = false; // запоминает состояние кнопки PA0

// этот таск зажигает светодиод по нажатию кнопки
static void App_TaskLed8( void * p_arg ) {
    // это чтобы убрать warning о неиспользуемом аргументе
    (void)p_arg;

    // настройка портов светодиодов и кнопки - делается при первом вызове таска
    // подаем тактирование
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC, ENABLE );
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );

    // делаем РС.0 - floating input
    GPIO_InitTypeDef gpioA;
    gpioA.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpioA.GPIO_Pin = GPIO_Pin_0;
    gpioA.GPIO_Speed = GPIO_Speed_10MHz;

    GPIO_Init( GPIOA, &gpioA );
    // делаем РС.8 - выходом пуш-пулл
    GPIO_InitTypeDef gpioC;
    gpioC.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioC.GPIO_Pin = GPIO_Pin_8;
    gpioC.GPIO_Speed = GPIO_Speed_10MHz;

    GPIO_Init( GPIOC, &gpioC );

    // таск никогда не должен завершаться через закрывающую скобку
    // поэтому внутри или удаление или бесконечный цикл
    while (1) {
        if (is_button_on == true)
            GPIOC -> ODR |= 1 << 8; // зажечь светодиод при нажатой кнопке
        else GPIOC -> ODR &= ~(1 << 8); // иначе погасить
        // засыпаем на 5 миллисекунд,чтобы поток кнопки захватил управление
        OSTimeDlyHMSM(0, 0, 0, 5);
    }
}

// все что делает этот таск - сканирует кнопку
static void App_TaskButton( void * p_arg ) {
    // это чтобы убрать warning о неиспользуемом аргументе
    (void)p_arg;

    // настройка портов - делается при первом вызове таска
    // да, в таске для кнопки порты тоже настраиваются - но ведь другой таск может
    // быть и не активен!

    // подаем тактирование
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC, ENABLE );
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );

    // делаем РС.0 - floating input
    GPIO_InitTypeDef gpioA;
    gpioA.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpioA.GPIO_Pin = GPIO_Pin_0;
    gpioA.GPIO_Speed = GPIO_Speed_10MHz;

    GPIO_Init( GPIOA, &gpioA );
    // делаем РС.8 - выходом пуш-пулл
    GPIO_InitTypeDef gpioC;
    gpioC.GPIO_Mode = GPIO_Mode_Out_PP;
    gpioC.GPIO_Pin = GPIO_Pin_8;
    gpioC.GPIO_Speed = GPIO_Speed_10MHz;

    GPIO_Init(GPIOC, &gpioC);

    // таск никогда не должен завершаться через закрывающую скобку
    // поэтому внутри или удаление или бесконечный цикл
    while (1) {
        //если кнопка нажата
        if(((GPIOA->IDR)&1))
            is_button_on = true;
        else is_button_on = false;
        // засыпаем на 5 миллисекунд, чтобы дать время поменять состояние светодиода
        OSTimeDlyHMSM(0, 0, 0, 5);
    }
}

static uint8_t button_num = 0;

static void App_TaskScanKeyboard(void * p_arg) {

    (void) p_arg;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    //строки в режиме input pull up
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode = GPIO_Mode_IPU;
    DefA.GPIO_Pin = (GPIO_Pin_1) | (GPIO_Pin_2) | (GPIO_Pin_3) | (GPIO_Pin_4);
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &DefA); // строки клавиатуры

    GPIO_InitTypeDef DefC;
    DefC.GPIO_Mode = GPIO_Mode_Out_OD;
    DefC.GPIO_Pin = (GPIO_Pin_1) | (GPIO_Pin_2) | (GPIO_Pin_3);
    DefC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &DefC); // столбцы клавиатуры
    GPIO_SetBits(GPIOC, (GPIO_Pin_1) | (GPIO_Pin_2) | (GPIO_Pin_3)); // установим столбцы в 1

    while (1) {
        INT8U err;
        OSSemPend(semaphore, 0, &err);
        APP_ASSERT(err == OS_ERR_NONE);
        bool is_button_on = false;
        static bool is_prev_button_on = false;
        for (uint8_t cols = 1; cols < 4; cols++) {
            GPIO_ResetBits(GPIOC, 1 << cols); // ставим 0 по очереди по столбцам
            OSTimeDlyHMSM( 0, 0, 0, 4 ); // подождать 4 миллисекунды
            for (uint8_t rows = 1; rows < 5; rows++) { // по очереди проверяем строки
                if (!(GPIOA->IDR&(1 << rows))) {
                    button_num = (cols-1)+3*(rows-1) + 1; // запоминает номер нажатой кнопки
                    is_button_on = true;
                    break;
                }
            }
            GPIO_SetBits(GPIOC, 1 << cols);
        }
        if ((is_button_on == false)&&(is_prev_button_on == false)) button_num = 0;
        is_prev_button_on = is_button_on;
        OSSemPost(semaphore); // освобождение семафора
        OSTimeDlyHMSM( 0, 0, 0, 10 );
    }
}

static void TIM3_IRQHandler() {
    TIM_ClearFlag(TIM3, TIM_IT_Update);
    GPIOB-> ODR ^= 1 << 5;
}

static void App_TaskPlaySound(void * p_arg) {

    (void)p_arg;

    static const uint16_t freq[12] = {523, 587, 659, 698, 784, 880, 987, 880, 784, 698, 659, 587}; //частота: ДО, РЕ, МИ, ФА, СОЛЬ, ЛЯ, СИ, ЛЯ, СОЛЬ, ФА, МИ, РЕ

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE );
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_InitStruct; 
    TIM_TimeBaseStructInit(&TIM_InitStruct); 
    TIM_InitStruct.TIM_Period = 100; 
    TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Down; 
    TIM_TimeBaseInit(TIM3,&TIM_InitStruct);

    TIM_ITConfig(TIM3,TIM_IT_Update, ENABLE);//настройка прерываний TIM3
    TIM_Cmd(TIM3,ENABLE);//запуск таймера 
    BSP_IntVectSet(BSP_INT_ID_TIM3,TIM3_IRQHandler);//разрешаем прерывания 

    //динамик в режиме push-pull
    GPIO_InitTypeDef DefB; // настройка динамика
    DefB.GPIO_Mode = GPIO_Mode_Out_PP;
    DefB.GPIO_Pin = GPIO_Pin_5;
    DefB.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &DefB);

    while(1) {
        INT8U err;
        OSSemPend(semaphore, 0, &err);
        APP_ASSERT(err == OS_ERR_NONE);
        //если кнопка нажата			
        if(button_num) {
            TIM_InitStruct.TIM_Prescaler = (72000000 / (200*freq[button_num - 1]))-1;
            TIM_TimeBaseInit(TIM3,&TIM_InitStruct); 
            NVIC_EnableIRQ(TIM3_IRQn);//разрешение прерываний таймера TIM3
            //если ничего не нажато
        } else { 
            NVIC_DisableIRQ(TIM3_IRQn);
            GPIO_ResetBits(GPIOB,GPIO_Pin_5);
        }
        OSSemPost(semaphore);
        OSTimeDlyHMSM(0, 0, 0, 10);
    }
}

volatile char tx_buffer[16];
static int8_t counter = 0;

static void USART1_IRQHandler(void) { 
    if(USART_GetITStatus(USART1, USART_IT_TC) == SET) { //прерывание по передаче
        USART_SendData(USART1,tx_buffer[counter++]);
    }
    if (counter > 15) {
        counter = 0;
        USART_ITConfig(USART1, USART_IT_TC, DISABLE);
    }
}

static void App_TaskSendButtonNum(void * p_arg) {

    (void)p_arg;

    char value;//значение кнопки в ASCII 

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef DefB;
    DefB.GPIO_Mode = GPIO_Mode_IN_FLOATING;	
    DefB.GPIO_Pin = GPIO_Pin_7; //РА7(rx)	

    DefB.GPIO_Mode = GPIO_Mode_AF_PP; 
    DefB.GPIO_Pin = GPIO_Pin_6; ////РА6(tx) 
	
    DefB.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB,&DefB);

    USART_InitTypeDef USART_struct; 
    USART_struct.USART_BaudRate = 57600; 
    USART_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
    USART_struct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
    USART_struct.USART_Parity = USART_Parity_No; 
    USART_struct.USART_StopBits = USART_StopBits_1; 
    USART_struct.USART_WordLength = USART_WordLength_8b; 
    USART_Init(USART1,&USART_struct);//настройка режима работы USART 

    USART_ITConfig(USART1,USART_IT_TC,DISABLE); 
    USART_Cmd(USART1, ENABLE);
    BSP_IntVectSet(BSP_INT_ID_USART1,USART1_IRQHandler);//разрешаем прерывания 
    NVIC_EnableIRQ(USART1_IRQn);//разрешение прерываний USART1 
    while(1) {
        INT8U err;
        OSSemPend( semaphore,0,&err);
        APP_ASSERT(err == OS_ERR_NONE); 
        static char prev_15th_symb = 0;
        if(button_num) {
            // перевод кнопки в ASCII
            if(button_num==12) value = 35;// #
            if(button_num==11) value = 48;// 0
            if(button_num==10) value = 42;// *
            else if(button_num<10) value = button_num + 0x30;
            tx_buffer[0] = 'p';
            tx_buffer[1] = 'r';
            tx_buffer[2] = 'e';
            tx_buffer[3] = 's';
            tx_buffer[4] = 's';
            tx_buffer[5] = 'e';
            tx_buffer[6] = 'd';
            tx_buffer[7] = ' ';
            tx_buffer[8] = 'b';
            tx_buffer[9] = 'u';
            tx_buffer[10] = 't';
            tx_buffer[11] = 't';
            tx_buffer[12] = 'o';
            tx_buffer[13] = 'n';
            tx_buffer[14] = ' ';
            tx_buffer[15] = value; // место в сообщении, где должен стоять номер кнопки
            if(prev_15th_symb != tx_buffer[15]) {
                prev_15th_symb = tx_buffer[15];
                USART_ITConfig(USART1, USART_IT_TC, ENABLE);
            }
        } 
        if(!button_num) {//если ничего не нажато
            tx_buffer[0] = 'n';
            tx_buffer[1] = 'o';
            tx_buffer[2] = 't';
            tx_buffer[3] = 'h';
            tx_buffer[4] = 'i';
            tx_buffer[5] = 'n';
            tx_buffer[6] = 'g';
            tx_buffer[7] = ' ';
            tx_buffer[8] = 'p';
            tx_buffer[9] = 'r';
            tx_buffer[10] = 'e';
            tx_buffer[11] = 's';
            tx_buffer[12] = 's';
            tx_buffer[13] = 'e';
            tx_buffer[14] = 'd';
            tx_buffer[15] = ' ';
            if(prev_15th_symb != tx_buffer[15]) {
                prev_15th_symb = tx_buffer[15];
                USART_ITConfig(USART1, USART_IT_TC, ENABLE);
            }
        }
        OSSemPost(semaphore);
        OSTimeDlyHMSM( 0, 0, 0, 15 );
    } 
}

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
