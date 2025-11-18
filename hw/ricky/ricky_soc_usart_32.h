#ifndef RICKIY_SOC_USART_H
#define RICKIY_SOC_USART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"


#define USART_SR   0x00
#define USART_DR   0x04
#define USART_BRR  0x08
#define USART_CR1  0x0C
#define USART_CR2  0x10
#define USART_CR3  0x14
#define USART_GTPR 0x18

/*
 * NB: The reset value mentioned in "24.6.1 Status register" seems bogus.
 * Looking at "Table 98 USART register map and reset values", it seems it
 * should be 0xc0, and that's how real hardware behaves.
 */
#define USART_SR_RESET (USART_SR_TXE | USART_SR_TC)

#define USART_SR_TXE  (1 << 7)
#define USART_SR_TC   (1 << 6)
#define USART_SR_RXNE (1 << 5)

#define USART_CR1_UE     (1 << 13)
#define USART_CR1_TXEIE  (1 << 7)
#define USART_CR1_TCEIE  (1 << 6)
#define USART_CR1_RXNEIE (1 << 5)
#define USART_CR1_TE     (1 << 3)
#define USART_CR1_RE     (1 << 2)


#define TYPE_RICKY_SOC_USART "ricky-soc-usart"

OBJECT_DECLARE_SIMPLE_TYPE(RickySocUsartState, RICKY_SOC_USART)


struct RickySocUsartState {
    /* private */
    SysBusDevice partend_obj;

    /* public */
    MemoryRegion mmio;

    uint32_t usart_sr;      //状态寄存器 Status Register
    uint32_t usart_dr;      //数据寄存器 Data Register
    uint32_t usart_brr;     //波特率寄存器 Baud Rate Register 
    uint32_t usart_cr1;     //控制寄存器1
    uint32_t usart_cr2;     //控制寄存器2
    uint32_t usart_cr3;     //控制寄存器3
    uint32_t usart_gtpr;    //预分频寄存器 Guard Time and Prescaler Register

    CharBackend chr;
    qemu_irq irq;
};




#endif