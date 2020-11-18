#include "mcu_support_package/inc/stm32f10x.h"

#define ASSIGNMENT_PART_1_BUTTON_LED    1
#define ASSIGNMENT_PART_2_LED_BLINKING  2
#define ASSIGNMENT_PART_3_THREE_BUTTONS 3


#if ASSIGNMENT_CURRENT_PART == ASSIGNMENT_PART_1_BUTTON_LED

int main(void)
{
    return 0;
}

#elif ASSIGNMENT_CURRENT_PART == ASSIGNMENT_PART_2_LED_BLINKING

int main(void)
{
    return 0;
}

#elif ASSIGNMENT_CURRENT_PART == ASSIGNMENT_PART_3_THREE_BUTTONS

int main(void)
{
    return 0;
}

#else

    #error "You should define ASSIGNMENT_CURRENT_PART to enable some part of the assignment"

#endif
