#include "hardware.h"
#include "application.h"

#include <stdio.h>
#include <stdbool.h>

//-------------------------------------------------------------------------------------
void DEBUG_printf( const char *ptr, ... );

//-------------------------------------------------------------------------------------
#define debug_printf(...)       DEBUG_printf(__VA_ARGS__)

//-------------------------------------------------------------------------------------
void UART_Init(USART_TypeDef *Instance, uint32_t baud);

//-------------------------------------------------------------------------------------
void UART_MspInit(USART_TypeDef *Instance)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  if(Instance==UART4)
  {
    /* Peripheral clock enable */
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_UART4EN);
  
    /**UART4 GPIO Configuration    
    PC10     ------> UART4_TX
    PC11     ------> UART4_RX 
    */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Peripheral interrupt init*/
    NVIC_SetPriority(UART4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(UART4_IRQn);
  }
  else if(Instance==USART1)
  {
    /* Peripheral clock enable */
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_USART1EN);
  
    /**USART1 GPIO Configuration    
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX 
    */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Peripheral interrupt init*/
    NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(USART1_IRQn);
  }
}

//-------------------------------------------------------------------------------------
uint8_t spi_read_byte(void)
{  
  uint8_t byte  = 0;
//  uint8_t count = 0;
  __IO uint32_t tmpreg;
  
  /* Check if the SPI is already enabled */ 
  if((SPI3->CR1 &SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */    
    SPI3->CR1 |=  SPI_CR1_SPE;
  }
  
  *(__IO uint8_t *)&SPI3->DR = byte;

  /* Wait until RXNE flag is reset */
  while( (SPI3->SR & SPI_SR_RXNE) != SPI_SR_RXNE )
  {
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
      taskYIELD();
  }
    
  byte = *(__IO uint8_t*)&SPI3->DR;
    
  return byte;
}

//-------------------------------------------------------------------------------------
void spi_send_byte(uint8_t dat)
{
  uint8_t dummy;
  
  /* Check if the SPI is already enabled */ 
  if((SPI3->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    SPI3->CR1 |=  SPI_CR1_SPE;
  }
  
  *(__IO uint8_t*)&SPI3->DR = dat; // just write - fucking fifo keeps txe ready
    
  /* Wait until RXNE flag is reset */
  while( (SPI3->SR & SPI_SR_RXNE) != SPI_SR_RXNE )  // use in to control byte out time
  {
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
      taskYIELD();
  }
  
  dummy = *(__IO uint8_t*)&SPI3->DR;
  dummy = dummy;
}

//-------------------------------------------------------------------------------------
void spi_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  
  /* Peripheral clock enable */
  RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
  
  /**SPI3 GPIO Configuration    
  PB3     ------> SPI3_SCK
  PB4     ------> SPI3_MISO
  PB5     ------> SPI3_MOSI 
  */
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_5;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  
  /* Disable the selected SPI peripheral */
  SPI3->CR1 &= (~SPI_CR1_SPE);
  
  /*---------------------------- SPIx CR1 & CR2 Configuration ------------------------*/
  /* Configure : SPI Mode, Communication Mode, Clock polarity and phase, NSS management,
  Communication speed, First bit, CRC calculation state, CRC Length */
  SPI3->CR1 = ((SPI_CR1_MSTR | SPI_CR1_SSI) | (SPI_CR1_SSM & SPI_CR1_SSM) | SPI_CR1_BR_0 );
  
  /* Configure : NSS management */
  /* Configure : Rx Fifo Threshold */
  SPI3->CR2 = (((SPI_CR1_SSM >> 16) & SPI_CR2_SSOE) );
  
  /* Configure : CRC Polynomial */
  SPI3->CRCPR = 10;
  
  /* Activate the SPI mode (Make sure that I2SMOD bit in I2SCFGR register is reset) */
  SPI3->I2SCFGR &= (uint16_t)(~SPI_I2SCFGR_I2SMOD);
}

//-------------------------------------------------------------------------------------
void init_hw(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  
  SystemCoreClockUpdate();
//  SysTick_Config(SystemCoreClock/1000); // 1 ms tick
  NVIC_SetPriority(SysTick_IRQn,0); // highest priority
  NVIC_SetPriorityGrouping(0);
  
  /* GPIO Ports Clock Enable */
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
  RCC->APB2ENR |= RCC_APB2ENR_IOPDEN;
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; // enable for eth rmii setting to have effect
  
  MODIFY_REG(AFIO->MAPR, AFIO_MAPR_SWJ_CFG, AFIO_MAPR_SWJ_CFG_JTAGDISABLE);

  /*Configure GPIO pins : PC13 PC0 PC2 PC12 OWI_L? RELAY R485 UART5_TX? */
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13|GPIO_Pin_0|GPIO_Pin_2|GPIO_Pin_12;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC9 KP_C1 */
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  /*Configure GPIO pins : PA4 PA5 PA6 LED_G LED_R LED_Y*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 PB8 F_WP F_HOLD ETH_N_RST*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB9 485_TX_RX*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB14 BUZ*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* TIM1 No remapping pins */
  /*!< No remap (ETR/PA12, CH1/PA8, CH2/PA9, CH3/PA10, CH4/PA11, BKIN/PB12, CH1N/PB13, CH2N/PB14, CH3N/PB15) */
  //AFIO->MAPR = ~AFIO_MAPR_TIM1_REMAP;
  
  /*Configure GPIO pin : PB15 KP_R4*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PC6 PC7 PC8 KP_R3 KP_R2*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA12 PA15 BL_EN OWI*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_15;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 KP_C2 KP_C3*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_11;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PD2 SPI3_CS*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOD, &GPIO_InitStruct);

//  /*Configure GPIO pins : PB4 PB5 */
//  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_5;
//  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
//  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
//  GPIO_Init(GPIOB, &GPIO_InitStruct);
  
//  BUZZER_HIGH();
  
  FLASH_CS_HIGH();
  FLASH_WP_HIGH();
  FLASH_HLD_HIGH();
  
  spi_init();
  
  // set listen direction
  RS485_READ_DIRECTION();

  UART_Init(USART1, 230400);
  UART_Init(UART4, 57600);
}

//-------------------------------------------------------------------------------------
void Flash_WP(bool wp)
{
  if (wp)
    FLASH_WP_LOW();
  else
    FLASH_WP_HIGH();
}

//-------------------------------------------------------------------------------------
void Flash_Hold(bool wp)
{
  if (wp)
    FLASH_HLD_LOW();
  else
    FLASH_HLD_HIGH();
}

//-------------------------------------------------------------------------------------
void EWSR(void)
{
  FLASH_CS_LOW();
  spi_send_byte(0x50);		/* enable writing to the status register */
  FLASH_CS_HIGH();
}

//-------------------------------------------------------------------------------------
void WRSR(uint8_t byte)
{
  FLASH_CS_LOW();
  spi_send_byte(0x01);		/* select write to status register */
  spi_send_byte(byte);		/* data that will change the status of BPx 
                              or BPL (only bits 2,3,4,5,7 can be written) */
  FLASH_CS_HIGH();
}

//-------------------------------------------------------------------------------------
void init_flash_hw(void)
{
  FLASH_CS_HIGH();
  
  Flash_WP(false);
  Flash_Hold(false);
  EWSR();
  WRSR(0x00);
}

//-------------------------------------------------------------------------------------
void Select_Flash(void)
{
  FLASH_CS_LOW();
}

//-------------------------------------------------------------------------------------
void Deselect_Flash(void)
{
  FLASH_CS_HIGH();
//  SF1_CS_SET = SF1_CS;
//  release_spi();
}

//-------------------------------------------------------------------------------------
void Write_Data_Page(uint8_t *transmit_data, uint16_t len)
{
  while (len)
  {
    spi_send_byte(*transmit_data++); // just write - fucking fifo keeps txe ready
    len--;
  }    
}

//-------------------------------------------------------------------------------------
void Read_Data_Page(uint8_t *received_data, uint16_t len)
{
  while (len)
  {
    *received_data++ = spi_read_byte();
    len--;
  }
}

//-------------------------------------------------------------------------------------
long Tim2RunCounter = 0;

//-------------------------------------------------------------------------------------
void vConfigureTimerForRunTimeStats( void )
{
  uint32_t tmpcr1 = 0;
  
	/* init variables */
	Tim2RunCounter=100;
	
	/*Init RCC clock Timer */
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

  /* Set the Time Base configuration */
  tmpcr1 = TIM2->CR1;

  /* Select the Counter Mode */
  tmpcr1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS);
  tmpcr1 |= 0;

  /* Set the clock division */
  tmpcr1 &= ~TIM_CR1_CKD;
  tmpcr1 |= (uint32_t)0x0000; // div0

  TIM2->CR1 = tmpcr1;

  /* Set the Autoreload value */
  TIM2->ARR = (uint32_t)99;             // 100 x 1MHz = 10KHz (100us)

  /* Set the Prescaler value */
  TIM2->PSC = (uint32_t)(SystemCoreClock/1000000) - 1;       // 1 MHz

  /* Set the Repetition Counter value */
//  TIM2->RCR = 0;

  /* Generate an update event to reload the Prescaler 
     and the repetition counter(only for TIM1 and TIM8) value immediatly */
  TIM2->EGR = TIM_EGR_UG;

	/* NVIC - Enable and set Interrupt to TIM2 */
  NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_EnableIRQ(TIM2_IRQn);

 /* TIM2 interrupt Enable */
  TIM2->DIER |= TIM_DIER_UIE;

  /* TIM2 - counter enable */
  TIM2->CR1 |= TIM_CR1_CEN;
}

//-------------------------------------------------------------------------------------
void TIM2_IRQHandler(void)
{
  if((TIM2->SR & TIM_SR_UIF) == TIM_SR_UIF)
  {
    if((TIM2->DIER & TIM_DIER_UIE) == TIM_DIER_UIE)
    {
      TIM2->SR = ~TIM_DIER_UIE;
      /* Clear TIM2 pending interrupt bit */
      Tim2RunCounter += 1 ;
    }
  }
}

//-------------------------------------------------------------------------------------
unsigned ValueTimerForRunTimeStats(void){
	return Tim2RunCounter ;
}

//-------------------------------------------------------------------------------------
void tx_done_callback(USART_TypeDef *Instance)
{
  //RS485_READ_DIRECTION();
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
