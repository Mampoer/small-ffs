#include "hardware.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>

#define UART_IT_PE                       ((uint32_t)(UART_CR1_REG_INDEX << 28 | USART_CR1_PEIE))
#define UART_IT_TXE                      ((uint32_t)(UART_CR1_REG_INDEX << 28 | USART_CR1_TXEIE))
#define UART_IT_TC                       ((uint32_t)(UART_CR1_REG_INDEX << 28 | USART_CR1_TCIE))
#define UART_IT_RXNE                     ((uint32_t)(UART_CR1_REG_INDEX << 28 | USART_CR1_RXNEIE))
#define UART_IT_IDLE                     ((uint32_t)(UART_CR1_REG_INDEX << 28 | USART_CR1_IDLEIE))

#define UART_IT_LBD                      ((uint32_t)(UART_CR2_REG_INDEX << 28 | USART_CR2_LBDIE))

#define UART_IT_CTS                      ((uint32_t)(UART_CR3_REG_INDEX << 28 | USART_CR3_CTSIE))
#define UART_IT_ERR                      ((uint32_t)(UART_CR3_REG_INDEX << 28 | USART_CR3_EIE))


#define UART_CR1_REG_INDEX               1    
#define UART_CR2_REG_INDEX               2    
#define UART_CR3_REG_INDEX               3    

#define UART_DIV_SAMPLING16(_PCLK_, _BAUD_)         (((_PCLK_)*25)/(4*(_BAUD_)))
#define UART_DIVMANT_SAMPLING16(_PCLK_, _BAUD_)     (UART_DIV_SAMPLING16((_PCLK_), (_BAUD_))/100)
#define UART_DIVFRAQ_SAMPLING16(_PCLK_, _BAUD_)     (((UART_DIV_SAMPLING16((_PCLK_), (_BAUD_)) - (UART_DIVMANT_SAMPLING16((_PCLK_), (_BAUD_)) * 100)) * 16 + 50) / 100)
#define UART_BRR_SAMPLING16(_PCLK_, _BAUD_)         ((UART_DIVMANT_SAMPLING16((_PCLK_), (_BAUD_)) << 4)|(UART_DIVFRAQ_SAMPLING16((_PCLK_), (_BAUD_)) & 0x0F))


#define UART_IT_MASK  ((uint32_t) USART_CR1_PEIE | USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_RXNEIE | \
                                  USART_CR1_IDLEIE | USART_CR2_LBDIE | USART_CR3_CTSIE | USART_CR3_EIE )

#define __UART_ENABLE_IT(__HANDLE__, __INTERRUPT__)   ((((__INTERRUPT__) >> 28) == UART_CR1_REG_INDEX)? ((__HANDLE__)->CR1 |= ((__INTERRUPT__) & UART_IT_MASK)): \
                                                           (((__INTERRUPT__) >> 28) == UART_CR2_REG_INDEX)? ((__HANDLE__)->CR2 |=  ((__INTERRUPT__) & UART_IT_MASK)): \
                                                           ((__HANDLE__)->CR3 |= ((__INTERRUPT__) & UART_IT_MASK)))
                                                           
#define __UART_DISABLE_IT(__HANDLE__, __INTERRUPT__)  ((((__INTERRUPT__) >> 28) == UART_CR1_REG_INDEX)? ((__HANDLE__)->CR1 &= ~((__INTERRUPT__) & UART_IT_MASK)): \
                                                           (((__INTERRUPT__) >> 28) == UART_CR2_REG_INDEX)? ((__HANDLE__)->CR2 &= ~((__INTERRUPT__) & UART_IT_MASK)): \
                                                           ((__HANDLE__)->CR3 &= ~ ((__INTERRUPT__) & UART_IT_MASK)))


/* Buf mask */
#define __BUF_MASK(size) (size-1)
/* Check buf is full or not */
#define __BUF_IS_FULL(head, tail, size) ((tail&__BUF_MASK(size))==((head+1)&__BUF_MASK(size)))
/* Check buf will be full in next receiving or not */
#define __BUF_WILL_FULL(head, tail, size) ((tail&__BUF_MASK(size))==((head+2)&__BUF_MASK(size)))
/* Check buf is empty */
#define __BUF_IS_EMPTY(head, tail, size) ((head&__BUF_MASK(size))==(tail&__BUF_MASK(size)))
/* Reset buf */
#define __BUF_RESET(bufidx)  (bufidx=0)
#define __BUF_INCR(bufidx,size)  (bufidx=(bufidx+1)&__BUF_MASK(size))

/************************** PRIVATE TYPES *************************/

/** @brief UART Ring buffer structure */
typedef struct
{
  __IO uint32_t tx_head; /*!< UART Tx ring buffer head index */
  __IO uint32_t tx_tail; /*!< UART Tx ring buffer tail index */
  __IO uint32_t rx_head; /*!< UART Rx ring buffer head index */
  __IO uint32_t rx_tail; /*!< UART Rx ring buffer tail index */
  __IO uint8_t  *tx;     /*!< UART Tx data ring buffer */
  __IO uint8_t  *rx;     /*!< UART Rx data ring buffer */
       uint32_t tx_size;
       uint32_t rx_size;
} UART_RING_BUFFER_T;

/************************** PRIVATE VARIABLES *************************/

static uint8_t rx1_buf[0x100];
static uint8_t tx1_buf[0x400];
//static uint8_t rx2_buf[0x40];
//static uint8_t tx2_buf[0x40];
//static uint8_t rx3_buf[0x40];
//static uint8_t tx3_buf[0x40];
static uint8_t rx4_buf[0x40];
static uint8_t tx4_buf[0x40];

// UART Ring buffer
static UART_RING_BUFFER_T rb1 = { 0, 0, 0, 0, tx1_buf, rx1_buf, sizeof(tx1_buf), sizeof(rx1_buf)};
//static UART_RING_BUFFER_T rb2 = { 0, 0, 0, 0, tx2_buf, rx2_buf, sizeof(tx2_buf), sizeof(rx2_buf)};
//static UART_RING_BUFFER_T rb3 = { 0, 0, 0, 0, tx3_buf, rx3_buf, sizeof(tx3_buf), sizeof(rx3_buf)};
static UART_RING_BUFFER_T rb4 = { 0, 0, 0, 0, tx4_buf, rx4_buf, sizeof(tx4_buf), sizeof(rx4_buf)};

void UART_MspInit(USART_TypeDef *Instance);

UART_RING_BUFFER_T *get_buffer(USART_TypeDef *Instance)
{
  if ( Instance == USART1 ) return (&rb1);
//  if ( Instance == USART2 ) return (&rb2);
//  if ( Instance == USART3 ) return (&rb3);
  if ( Instance == UART4 )  return (&rb4);
  return NULL;
}

#define UART_RX_DATA(Instance)                *(__IO uint8_t*)&Instance->DR & 0xFF
#define UART_TX_DATA(Instance)                *(__IO uint8_t*)&Instance->DR

void UART_Receive_IT(USART_TypeDef *Instance)
{
  UART_RING_BUFFER_T *rb = get_buffer(Instance);
  
  uint8_t c = (uint8_t)(UART_RX_DATA(Instance));
  
  if ( rb )
  {
    if (!__BUF_IS_FULL(rb->rx_head,rb->rx_tail,rb->rx_size))
    {
      rb->rx[rb->rx_head] = c;
      __BUF_INCR(rb->rx_head,rb->rx_size);
    }
  }
}

void UART_Transmit_IT(USART_TypeDef *Instance)
{
  UART_RING_BUFFER_T *rb = get_buffer(Instance);
  
  if ( rb )
  {
    if (!__BUF_IS_EMPTY(rb->tx_head,rb->tx_tail,rb->tx_size))
    {
      UART_TX_DATA(Instance) = rb->tx[rb->tx_tail];
      __BUF_INCR(rb->tx_tail,rb->tx_size);
      return;
    }
  }
  
  __UART_DISABLE_IT(Instance, UART_IT_TXE);
}

void UART_EndTransmit_IT(USART_TypeDef *Instance)
{
  tx_done_callback(Instance);
  __UART_DISABLE_IT(Instance, UART_IT_TC);
}

#define __UART_DISABLE(__HANDLE__)              ((__HANDLE__)->CR1 &=  ~USART_CR1_UE)


/** @defgroup UART_Word_Length   UART Word Length
  * @{
  */
#define UART_WORDLENGTH_8B                  ((uint32_t)0x00000000)
#define UART_WORDLENGTH_9B                  ((uint32_t)USART_CR1_M)
/**
  * @}
  */

/** @defgroup UART_Stop_Bits   UART Number of Stop Bits
  * @{
  */
#define UART_STOPBITS_1                     ((uint32_t)0x00000000)
#define UART_STOPBITS_2                     ((uint32_t)USART_CR2_STOP_1)
/**
  * @}
  */ 

/** @defgroup UART_Parity  UART Parity
  * @{
  */ 
#define UART_PARITY_NONE                    ((uint32_t)0x00000000)
#define UART_PARITY_EVEN                    ((uint32_t)USART_CR1_PCE)
#define UART_PARITY_ODD                     ((uint32_t)(USART_CR1_PCE | USART_CR1_PS)) 
/**
  * @}
  */ 

/** @defgroup UART_Hardware_Flow_Control UART Hardware Flow Control
  * @{
  */ 
#define UART_HWCONTROL_NONE                  ((uint32_t)0x00000000)
#define UART_HWCONTROL_RTS                   ((uint32_t)USART_CR3_RTSE)
#define UART_HWCONTROL_CTS                   ((uint32_t)USART_CR3_CTSE)
#define UART_HWCONTROL_RTS_CTS               ((uint32_t)(USART_CR3_RTSE | USART_CR3_CTSE))
/**
  * @}
  */

/** @defgroup UART_Mode UART Transfer Mode
  * @{
  */ 
#define UART_MODE_RX                        ((uint32_t)USART_CR1_RE)
#define UART_MODE_TX                        ((uint32_t)USART_CR1_TE)
#define UART_MODE_TX_RX                     ((uint32_t)(USART_CR1_TE |USART_CR1_RE))

#define UART_BRR_SAMPLING16(_PCLK_, _BAUD_)         ((UART_DIVMANT_SAMPLING16((_PCLK_), (_BAUD_)) << 4)|(UART_DIVFRAQ_SAMPLING16((_PCLK_), (_BAUD_)) & 0x0F))

const uint8_t aAPBAHBPrescTable[16]       = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL))) 

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
  /* Get HCLK source and Compute PCLK1 frequency ---------------------------*/
  return (SystemCoreClock >> aAPBAHBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE1)>> POSITION_VAL(RCC_CFGR_PPRE1)]);
}    

uint32_t HAL_RCC_GetPCLK2Freq(void)
{
  /* Get HCLK source and Compute PCLK2 frequency ---------------------------*/
  return (SystemCoreClock >> aAPBAHBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE2)>> POSITION_VAL(RCC_CFGR_PPRE2)]);
} 

static void UART_SetConfig(USART_TypeDef *Instance, uint32_t BaudRate)
{
  uint32_t tmpreg = 0x00;
  
  /*------- UART-associated USART registers setting : CR2 Configuration ------*/
  /* Configure the UART Stop Bits: Set STOP[13:12] bits according 
   * to huart->Init.StopBits value */
  MODIFY_REG(Instance->CR2, USART_CR2_STOP, UART_STOPBITS_1);

  /*------- UART-associated USART registers setting : CR1 Configuration ------*/
  /* Configure the UART Word Length, Parity and mode: 
     Set the M bits according to huart->Init.WordLength value 
     Set PCE and PS bits according to huart->Init.Parity value
     Set TE and RE bits according to huart->Init.Mode value */
  tmpreg = (uint32_t)UART_WORDLENGTH_8B | UART_PARITY_NONE | UART_MODE_TX_RX ;
  MODIFY_REG(Instance->CR1, 
             (uint32_t)(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS | USART_CR1_TE | USART_CR1_RE), 
             tmpreg);
  
  /*------- UART-associated USART registers setting : CR3 Configuration ------*/
  /* Configure the UART HFC: Set CTSE and RTSE bits according to huart->Init.HwFlowCtl value */
  MODIFY_REG(Instance->CR3, (USART_CR3_RTSE | USART_CR3_CTSE), UART_HWCONTROL_NONE);
  
  /*------- UART-associated USART registers setting : BRR Configuration ------*/
  if(Instance == USART1)
  {
    Instance->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK2Freq(), BaudRate);
  }
  else
  {
    Instance->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK1Freq(), BaudRate);
  }
}

#define __UART_ENABLE(__HANDLE__)               ((__HANDLE__)->CR1 |=  USART_CR1_UE)


void UART_Init(USART_TypeDef *Instance, uint32_t baud)
{
  UART_MspInit(Instance);
  
  /* Disable the Peripheral */
  __UART_DISABLE(Instance);
  
  /* Set the UART Communication parameters */
  UART_SetConfig(Instance, baud);
  
  /* In asynchronous mode, the following bits must be kept cleared: 
  - LINEN and CLKEN bits in the USART_CR2 register,
  - SCEN, HDSEL and IREN  bits in the USART_CR3 register.*/
  Instance->CR2 &= ~(USART_CR2_LINEN | USART_CR2_CLKEN); 
  Instance->CR3 &= ~(USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN); 
    
    /* Enable the UART Parity Error Interrupt */
  __UART_ENABLE_IT(Instance, UART_IT_PE);

  /* Enable the UART Error Interrupt: (Frame error, noise error, overrun error) */
  __UART_ENABLE_IT(Instance, UART_IT_ERR);

  /* Enable the UART Data Register not empty Interrupt */
  __UART_ENABLE_IT(Instance, UART_IT_RXNE);

  /* Enable the Peripheral */
  __UART_ENABLE(Instance);
}

#define __UART_GET_FLAG(__HANDLE__, __FLAG__) (((__HANDLE__)->SR & (__FLAG__)) == (__FLAG__))   

#define __UART_GET_IT_SOURCE(__HANDLE__, __IT__) (((((__IT__) >> 28) == UART_CR1_REG_INDEX)? (__HANDLE__)->CR1:(((((uint32_t)(__IT__)) >> 28) == UART_CR2_REG_INDEX)? \
                                                      (__HANDLE__)->CR2 : (__HANDLE__)->CR3)) & (((uint32_t)(__IT__)) & UART_IT_MASK))

#define __UART_CLEAR_NEFLAG(__HANDLE__) __UART_CLEAR_PEFLAG(__HANDLE__)
#define __UART_CLEAR_OREFLAG(__HANDLE__) __UART_CLEAR_PEFLAG(__HANDLE__)

#define UNUSED(x) ((void)(x))

#define __UART_CLEAR_PEFLAG(__HANDLE__) \
do{                                     \
  __IO uint32_t tmpreg;                 \
  tmpreg = (__HANDLE__)->SR;  \
  tmpreg = (__HANDLE__)->DR;  \
  UNUSED(tmpreg);                       \
}while(0)

void COMM_UART_IRQHandler(USART_TypeDef *Instance)
{
  uint32_t tmp_flag = 0, tmp_it_source = 0;

  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_PE);
  tmp_it_source = __UART_GET_IT_SOURCE(Instance, UART_IT_PE);  
  /* UART parity error interrupt occurred ------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    __UART_CLEAR_PEFLAG(Instance);
    
    //huart->ErrorCode |= HAL_UART_ERROR_PE;
  }
  
  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_FE);
  tmp_it_source = __UART_GET_IT_SOURCE(Instance, UART_IT_ERR);
  /* UART frame error interrupt occurred -------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    __UART_CLEAR_PEFLAG(Instance);
    
    //huart->ErrorCode |= HAL_UART_ERROR_FE;
  }
  
  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_NE);
  /* UART noise error interrupt occurred -------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    __UART_CLEAR_NEFLAG(Instance);
    
    //huart->ErrorCode |= HAL_UART_ERROR_NE;
  }
  
  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_ORE);
  /* UART Over-Run interrupt occurred ----------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    __UART_CLEAR_OREFLAG(Instance);
    
    //huart->ErrorCode |= HAL_UART_ERROR_ORE;
  }
  
  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_RXNE);
  tmp_it_source = __UART_GET_IT_SOURCE(Instance, UART_IT_RXNE);
  /* UART in mode Receiver ---------------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    UART_Receive_IT(Instance);
  }
  
  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_TXE);
  tmp_it_source = __UART_GET_IT_SOURCE(Instance, UART_IT_TXE);
  /* UART in mode Transmitter ------------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    UART_Transmit_IT(Instance);
  }

  tmp_flag = __UART_GET_FLAG(Instance, USART_SR_TC);
  tmp_it_source = __UART_GET_IT_SOURCE(Instance, UART_IT_TC);
  /* UART in mode Transmitter end --------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    UART_EndTransmit_IT(Instance);
  }  

//  if(huart->ErrorCode != HAL_UART_ERROR_NONE)
//  {
//    /* Set the UART state ready to be able to start again the process */
//    huart->State = HAL_UART_STATE_READY;
//    
//    HAL_UART_ErrorCallback(huart);
//  }  
}

int uart_getchar(USART_TypeDef *Instance)
{
  int ret;
  UART_RING_BUFFER_T *rb = get_buffer(Instance);
  
  if ( rb )
  {
    if (!(__BUF_IS_EMPTY(rb->rx_head, rb->rx_tail, rb->rx_size)))
    {
      /* Read data from ring buffer into user buffer */
      ret = (int)rb->rx[rb->rx_tail];

      /* Update tail pointer */
      __BUF_INCR(rb->rx_tail,rb->rx_size);
      
      return ret;
    }
  }

  return -1;
}

int uart_putchar( USART_TypeDef *Instance, int c )
{
  UART_RING_BUFFER_T *rb = get_buffer(Instance);
  
  if (rb)
  {
    /* Loop until transmit run buffer is full or until n_bytes
     expires */
    while (__BUF_IS_FULL(rb->tx_head, rb->tx_tail, rb->tx_size))
    {
      if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
        taskYIELD();
    }

    __UART_DISABLE_IT(Instance, UART_IT_TXE);
   
    rb->tx[rb->tx_head] = (uint8_t)c;
    /* Increment head pointer */
    __BUF_INCR(rb->tx_head,rb->tx_size);

    __UART_ENABLE_IT(Instance, UART_IT_TXE);
    __UART_ENABLE_IT(Instance, UART_IT_TC);
  }

  return c;
}

void USART1_IRQHandler(void)
{
  COMM_UART_IRQHandler(USART1);
}

void USART2_IRQHandler(void)
{
  COMM_UART_IRQHandler(USART2);
}

void USART3_IRQHandler(void)
{
  COMM_UART_IRQHandler(USART3);
}

void UART4_IRQHandler(void)
{
  COMM_UART_IRQHandler(UART4);
}

