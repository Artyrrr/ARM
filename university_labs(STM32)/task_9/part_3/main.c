#include "mcu_support_package/inc/stm32f10x.h"
#include "signal.h" 

static uint8_t buffer = 0; //буфер приёмника
static uint8_t message[5]; //сообщение-запрос
static uint8_t reply[4] = {0xAA,0xCC,0,0}; //сообщение-ответ
static uint8_t count = 0; //счетчик, который соответствует количеству полученных правильных запросов
static uint16_t freq = 0; //частота динамика

void USART2_IRQHandler(void) {
    static uint8_t count_rep = 1; //счетчик элементов ответа
    if(USART_GetITStatus(USART2, USART_IT_TC) == SET) { //прерывание окончании передачи
        USART_SendData(USART2, reply[count_rep]);
        count_rep++;
        if (count_rep > 3) {
            count_rep = 1;
            USART_ITConfig(USART2, USART_IT_TC, DISABLE); //после передачи - выключение прерывания по передаче
        }
    } else {
        buffer = USART_ReceiveData(USART2);
    }
}

void SysTick_Handler(void) { //для динамика
    static uint8_t a = 1; //для инверсии включения динамика
    static uint8_t k = 1; //счетчик прерываний
    static uint8_t f = 0; //коэффицент (когда подавать напряжение на динамик)
    f=10000/freq;
    if(k == f) {
        if (a==1) {
            GPIO_SetBits(GPIOA, GPIO_Pin_6);
        } else {
            GPIO_ResetBits(GPIOA, GPIO_Pin_6);
        }
        a=~a;
        k=0;
    }
    k++;
    if (k >= 254) {
        k = 1; //обновление счетчика, чтобы избежать переполнения
    }
}

void ProcessingReply(void) { //функция обработки ответа
    reply[2]=count;
    reply[3]=(0xAA+0xCC+count)%256;
    if (count >= 254) {
        count = 0; //обнуление счетчика для избежания переполнения
    }
}

void ProcessingRequest(void) { //функция обработки запроса
    static uint8_t prev_buf = 0; //предыдущее значение буфера 
    static int chsum = 0; //теоретическая контрольная сумма
    if (prev_buf != buffer) { //если значение буфера изменилось
        prev_buf = buffer;
        for(int i=0; i<4; i++) {
            message[i]=message[i+1];
        }
        message[4] = buffer;
        chsum=(message[0]+message[1]+message[2]+message[3])%256;
        if(message[0]==0xBB&&message[1]==0xCC&&message[4]==chsum) {
            count++; //пополняем счетчик, который соответствует количеству полученных правильных запросов
            if(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == SET) {
                ProcessingReply (); //обработка ответа
                USART_SendData(USART2, reply[0]);
                USART_ITConfig(USART2, USART_IT_TC, ENABLE); //т.к. отправилась только 1/4, запускается прерывание по передаче
            }
            freq=(message[3]<<8) + message[2]; //считывание частоты
        }
    }
}

void init_USART(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); //USART2 на выводах РА2(tx) и РА3(rx), динамик PA6
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); //запитали USART2
    
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode = GPIO_Mode_Out_PP; //динамик
    DefA.GPIO_Pin = GPIO_Pin_6;
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_AF_PP; //РА2(tx)
    DefA.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOA, &DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_IN_FLOATING; //РА3(rx)
    DefA.GPIO_Pin = GPIO_Pin_3;
    GPIO_Init(GPIOA, &DefA);
    
    USART_InitTypeDef USART_struct;
    USART_struct.USART_BaudRate = 57600;
    USART_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
    USART_struct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_struct.USART_Parity = USART_Parity_No;
    USART_struct.USART_StopBits = USART_StopBits_1;
    USART_struct.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &USART_struct); //настройка режима работы USART 
    
    USART_Cmd(USART2, ENABLE);
    
    __disable_irq();
    SysTick_Config(SystemCoreClock/10000);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); //прерывание при фактическом приёме данных
    NVIC_EnableIRQ(USART2_IRQn);
    __enable_irq();
}

int main(void) {
    init_USART();
    while(1) {
        ProcessingRequest();
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
