
#include "main.hpp"
#include "stm32f469xx.h"
#include "FreeRTOS.h"
#include "task.h"


int main()
{
    xTaskCreate(blink, "blink", configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
    vTaskStartScheduler();
    return 0;
}

void blink(void *pvParameters)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOKEN;

    GPIOG->MODER &= ~(3 << (6 * 2));
    GPIOG->MODER |=  1 << (6 * 2);

    GPIOD->MODER &= ~(3 << (4 * 2) | 3 << (5 * 2));
    GPIOD->MODER |=  1 << (4 * 2) | 1 << (5 * 2);

    GPIOK->MODER &= ~(3 << (3 * 2));
    GPIOK->MODER |=  1 << (3 * 2);

    while (true)
    {
        GPIOG->BSRR = GPIO_BSRR_BR6;   // ON
        GPIOD->BSRR = GPIO_BSRR_BR4 | GPIO_BSRR_BR5;
        GPIOK->BSRR = GPIO_BSRR_BR3;
        vTaskDelay(pdMS_TO_TICKS(500));

        GPIOG->BSRR = GPIO_BSRR_BS6;   // OFF
        GPIOD->BSRR = GPIO_BSRR_BS4 | GPIO_BSRR_BS5;
        GPIOK->BSRR = GPIO_BSRR_BS3;
        vTaskDelay(pdMS_TO_TICKS(500));

    }
    vTaskDelete(nullptr);
}