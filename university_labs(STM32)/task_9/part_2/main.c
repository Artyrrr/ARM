#include "mcu_support_package/inc/stm32f10x.h"
#include <stdio.h>
#include "signal.h" 
#include "stdbool.h"

int a;

static void init_USART(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 | RCC_APB1Periph_USART2,ENABLE); 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA,ENABLE); 
    
    GPIO_InitTypeDef DefA; 
    DefA.GPIO_Mode = GPIO_Mode_IPD; 
    DefA.GPIO_Pin = GPIO_Pin_0; 
    DefA.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_Init(GPIOA,&DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_IN_FLOATING; 
    DefA.GPIO_Pin = GPIO_Pin_3; 
    GPIO_Init(GPIOA,&DefA);
    
    DefA.GPIO_Mode = GPIO_Mode_AF_PP; 
    DefA.GPIO_Pin = GPIO_Pin_2; 
    GPIO_Init(GPIOA,&DefA); 
    
    USART_InitTypeDef USART_struct; 
    USART_struct.USART_BaudRate = 9600; 
    USART_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
    USART_struct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
    USART_struct.USART_Parity = USART_Parity_No; 
    USART_struct.USART_StopBits = USART_StopBits_1; 
    USART_struct.USART_WordLength = USART_WordLength_8b; 
    USART_Init(USART1,&USART_struct); //настройка режима работы USART 
    
    USART_Cmd(USART1,ENABLE); 
    
    printf("haha");
    
    scanf("%i", &a );
        
    printf("A is equal to %i", a);
}	

int main(void) {
    init_USART();
    while(1) {  
    }; 
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
