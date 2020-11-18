#include "mcu_support_package/inc/stm32f10x.h"
#include "signal.h"

static uint8_t set_but = 0; //индикатор нажатия кнопки
static uint8_t buf = 0; //буфер приемника

void USART1_IRQHandler(void) {
    if(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == SET) { //прерывание по окончании регистра передачи
        USART_SendData(USART1, set_but);
    }
    if(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) { //прерывание при приёме данных
        buf = USART_ReceiveData(USART1);
    } else {
        buf = 0;
    }
}

void SysTick_Handler(void) {
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)!=0) { //если зафиксировано нажатие
        set_but=1;
    } else {
        set_but=0;
    }
    if (buf != 0) { //если приняли данные
        GPIO_SetBits(GPIOC, GPIO_Pin_8);
    } else {
        GPIO_ResetBits(GPIOC, GPIO_Pin_8);
    }
}

void init_USART(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); //USART1 на выводах РА9(tx) и РА10(rx), кнопка PA0
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); //PC8 - светодиод
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); //запитали USART1
    
    GPIO_InitTypeDef DefA;
    DefA.GPIO_Mode = GPIO_Mode_Out_PP;
    DefA.GPIO_Pin = GPIO_Pin_8;
    DefA.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_AF_PP; //РА2(tx)
    DefA.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOA, &DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_IN_FLOATING; //РА3(rx)
    DefA.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &DefA); 
    
    USART_InitTypeDef USART_struct;
    USART_struct.USART_BaudRate = 9600;
    USART_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
    USART_struct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_struct.USART_Parity = USART_Parity_No;
    USART_struct.USART_StopBits = USART_StopBits_1;
    USART_struct.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART_struct); //настройка режима работы USART 
    
    USART_Cmd(USART1, ENABLE);
    
    __disable_irq();
    SysTick_Config(200);
    USART_ITConfig(USART1, USART_IT_TXE, ENABLE); //прерывание по окончании регистра передачи
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); //прерывание при приёме данных
    NVIC_EnableIRQ(USART1_IRQn);
    __enable_irq();
}

int main(void) {
    init_USART();
    while(1);
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
