#pragma once


// Requires BAUD_RATE, FOSC be defined


/* generate and register interrupt handlers */

#define REG_USART_RXC_HANDLER
#define REG_USART_UDRE_HANDLER


/* use default I/O BUF_SIZEs and types */


#include <act-common/usart.h>


/* Disable interrupts (see ATmega8A datasheet) */
inline __monitor void usart_init()
{
    const int ubrr = FOSC/16/BAUD_RATE-1;
    /* Set baud rate */
    UBRRH = (unsigned char) (ubrr>>8);
    UBRRL = (unsigned char) ubrr;
    /* Enable receiver and transmitter */
    UCSRB = (1<<RXEN)|(1<<TXEN);
    /* Set frame format: 8data, 1stop bit, parity odd bit */
    UCSRC = (1<<URSEL)|(3<<UCSZ0)|(3<<UPM0);
     /* Enable RX Complete and Data Reg. Empty interrupt */
    UCSRB |= (1<<RXCIE)|(1<<UDRIE);
}
