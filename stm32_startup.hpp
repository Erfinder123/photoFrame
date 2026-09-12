
#ifndef STM32_STARTUP_H
#define STM32_STARTUP_H


#define DEFAULT_ISR(handler) void handler() __attribute__((weak, alias("Default_Handler")))

extern uint8_t _sstack[];
extern uint8_t _ldata[];
extern uint8_t _sdata[];
extern uint8_t _edata[];
extern uint8_t _sbss[];
extern uint8_t _ebss[];


extern "C"
{
	void _init();

    void SystemInit();
    //void __libc_init_array();

    void Default_Handler();

    void Reset_Handler();
    DEFAULT_ISR(NMI_Handler);
    DEFAULT_ISR(HardFault_Handler);
    DEFAULT_ISR(MemManage_Handler);
    DEFAULT_ISR(BusFault_Handler);
    DEFAULT_ISR(UsageFault_Handler);
    DEFAULT_ISR(SVC_Handler);
    DEFAULT_ISR(DebugMon_Handler);
    DEFAULT_ISR(PendSV_Handler);
    DEFAULT_ISR(SysTick_Handler);

    DEFAULT_ISR(WWDG_IRQHandler);
    DEFAULT_ISR(PVD_IRQHandler);
    DEFAULT_ISR(TAMP_STAMP_IRQHandler);
    DEFAULT_ISR(RTC_WKUP_IRQHandler);
    DEFAULT_ISR(FLASH_IRQHandler);
    DEFAULT_ISR(RCC_IRQHandler);
    DEFAULT_ISR(EXTI0_IRQHandler);
    DEFAULT_ISR(EXTI1_IRQHandler);
    DEFAULT_ISR(EXTI2_IRQHandler);
    DEFAULT_ISR(EXTI3_IRQHandler);
    DEFAULT_ISR(EXTI4_IRQHandler);
    DEFAULT_ISR(DMA1_Stream0_IRQHandler);
    DEFAULT_ISR(DMA1_Stream1_IRQHandler);
    DEFAULT_ISR(DMA1_Stream2_IRQHandler);
    DEFAULT_ISR(DMA1_Stream3_IRQHandler);
    DEFAULT_ISR(DMA1_Stream4_IRQHandler);
    DEFAULT_ISR(DMA1_Stream5_IRQHandler);
    DEFAULT_ISR(DMA1_Stream6_IRQHandler);
    DEFAULT_ISR(ADC_IRQHandler);
    DEFAULT_ISR(CAN1_TX_IRQHandler);
    DEFAULT_ISR(CAN1_RX0_IRQHandler);
    DEFAULT_ISR(CAN1_RX1_IRQHandler);
    DEFAULT_ISR(CAN1_SCE_IRQHandler);
    DEFAULT_ISR(EXTI9_5_IRQHandler);
    DEFAULT_ISR(TIM1_BRK_TIM9_IRQHandler);
    DEFAULT_ISR(TIM1_UP_TIM10_IRQHandler);
    DEFAULT_ISR(TIM1_TRG_COM_TIM11_IRQHandler);
    DEFAULT_ISR(TIM1_CC_IRQHandler);
    DEFAULT_ISR(TIM2_IRQHandler);
    DEFAULT_ISR(TIM3_IRQHandler);
    DEFAULT_ISR(TIM4_IRQHandler);
    DEFAULT_ISR(I2C1_EV_IRQHandler);
    DEFAULT_ISR(I2C1_ER_IRQHandler);
    DEFAULT_ISR(I2C2_EV_IRQHandler);
    DEFAULT_ISR(I2C2_ER_IRQHandler);
    DEFAULT_ISR(SPI1_IRQHandler);
    DEFAULT_ISR(SPI2_IRQHandler);
    DEFAULT_ISR(USART1_IRQHandler);
    DEFAULT_ISR(USART2_IRQHandler);
    DEFAULT_ISR(USART3_IRQHandler);
    DEFAULT_ISR(EXTI15_10_IRQHandler);
    DEFAULT_ISR(RTC_Alarm_IRQHandler);
    DEFAULT_ISR(OTG_FS_WKUP_IRQHandler);
    DEFAULT_ISR(TIM8_BRK_TIM12_IRQHandler);
    DEFAULT_ISR(TIM8_UP_TIM13_IRQHandler);
    DEFAULT_ISR(TIM8_TRG_COM_TIM14_IRQHandler);
    DEFAULT_ISR(TIM8_CC_IRQHandler);
    DEFAULT_ISR(DMA1_Stream7_IRQHandler);
    DEFAULT_ISR(FMC_IRQHandler);
    DEFAULT_ISR(SDIO_IRQHandler);
    DEFAULT_ISR(TIM5_IRQHandler);
    DEFAULT_ISR(SPI3_IRQHandler);
    DEFAULT_ISR(UART4_IRQHandler);
    DEFAULT_ISR(UART5_IRQHandler);
    DEFAULT_ISR(TIM6_DAC_IRQHandler);
    DEFAULT_ISR(TIM7_IRQHandler);
    DEFAULT_ISR(DMA2_Stream0_IRQHandler);
    DEFAULT_ISR(DMA2_Stream1_IRQHandler);
    DEFAULT_ISR(DMA2_Stream2_IRQHandler);
    DEFAULT_ISR(DMA2_Stream3_IRQHandler);
    DEFAULT_ISR(DMA2_Stream4_IRQHandler);
    DEFAULT_ISR(ETH_IRQHandler);
    DEFAULT_ISR(ETH_WKUP_IRQHandler);
    DEFAULT_ISR(CAN2_TX_IRQHandler);
    DEFAULT_ISR(CAN2_RX0_IRQHandler);
    DEFAULT_ISR(CAN2_RX1_IRQHandler);
    DEFAULT_ISR(CAN2_SCE_IRQHandler);
    DEFAULT_ISR(OTG_FS_IRQHandler);
    DEFAULT_ISR(DMA2_Stream5_IRQHandler);
    DEFAULT_ISR(DMA2_Stream6_IRQHandler);
    DEFAULT_ISR(DMA2_Stream7_IRQHandler);
    DEFAULT_ISR(USART6_IRQHandler);
    DEFAULT_ISR(I2C3_EV_IRQHandler);
    DEFAULT_ISR(I2C3_ER_IRQHandler);
    DEFAULT_ISR(OTG_HS_EP1_OUT_IRQHandler);
    DEFAULT_ISR(OTG_HS_EP1_IN_IRQHandler);
    DEFAULT_ISR(OTG_HS_WKUP_IRQHandler);
    DEFAULT_ISR(OTG_HS_IRQHandler);
    DEFAULT_ISR(DCMI_IRQHandler);
    DEFAULT_ISR(CRYP_IRQHandler);
    DEFAULT_ISR(HASH_RNG_IRQHandler);
    DEFAULT_ISR(FPU_IRQHandler);
    DEFAULT_ISR(UART7_IRQHandler);
    DEFAULT_ISR(UART8_IRQHandler);
    DEFAULT_ISR(SPI4_IRQHandler);
    DEFAULT_ISR(SPI5_IRQHandler);
    DEFAULT_ISR(SPI6_IRQHandler);
    DEFAULT_ISR(SAI1_IRQHandler);
    DEFAULT_ISR(LTDC_IRQHandler);
    DEFAULT_ISR(LTDC_ER_IRQHandler);
    DEFAULT_ISR(DMA2D_IRQHandler);
}


#endif //STM32_STARTUP_H