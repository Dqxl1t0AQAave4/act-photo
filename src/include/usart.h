#pragma once

/*
 * #30 <bug>
 *
 *     Race encountered on `iobuf_state`.
 *
 *     Split it on `ibuf_state` and `obuf_state`.
 *     Merge macro state constants (IBUF_NEMPTY, OBUF_NEMPTY
 *     -> IOBUF_NEMPTY).
 */

// Requires IBUF_SIZE and OBUF_SIZE be defined
// Requires BAUD_RATE, FOSC be defined

#include "common.h"




/* Disables RXCIE, executes code, enables RXCIE */
#define NO_RX_COMPLETE(code) \
    UCSRB &= ~(1 << RXCIE); /* Disable RX Complete interrupt */ \
    code \
    UCSRB |= (1 << RXCIE); /* Enable RX Complete interrupt */

/* Disables UDRIE, executes code, enables UDRIE */
#define NO_DATA_REG_EMPTY(code) \
    UCSRB &= ~(1 << UDRIE); /* Disable Data Reg. Empty interrupt */ \
    code \
    UCSRB |= (1 << UDRIE); /* Enable Data Reg. Empty interrupt */




/*
 * Marked `volatile` to prevent data caching
 * and other effects as the data is to be accessed
 * from both the interrupt and user code.
 */

volatile byte ibuf[IBUF_SIZE];
volatile byte obuf[OBUF_SIZE];


#define IOBUF_NEMPTY 0x1
#define IOBUF_NFULL  0x2


__tiny volatile byte ibuf_state  = IOBUF_NFULL;
__tiny volatile byte obuf_state  = IOBUF_NFULL;

__tiny volatile byte ibuf_head   = 0;
__tiny volatile byte ibuf_tail   = 0;
__tiny volatile byte obuf_head   = 0;
__tiny volatile byte obuf_tail   = 0;





/* Accessed from the user code; USART interrupts must be disabled manually */
/*
 * Reads a byte from input buffer.
 *
 * Returns true if byte was actually read, false otherwise
 */
inline bool _iread(byte &out)
{
    if (ibuf_state & IOBUF_NEMPTY) /* not empty */
    {
        byte h = ibuf_head;
        out = ibuf[h++]; /* read byte */
        if (h == IBUF_SIZE) h = 0;
        if (h == ibuf_tail)
        {
            ibuf_state &= ~IOBUF_NEMPTY /* mark empty */;
        }
        ibuf_state |= IOBUF_NFULL /* mark not full */;
        ibuf_head = h;
        return true;
    }
    return false;
}





/* Accessed from the interrupt */
/*
 * Writes the byte to input buffer.
 */
#define _iwrite(in) \
    if (ibuf_state & IOBUF_NFULL) /* not full */ \
    { \
        byte t = ibuf_tail; \
        ibuf[t++] = in; /* write byte */ \
        if (t == IBUF_SIZE) t = 0; \
        if (t == ibuf_head) ibuf_state &= ~IOBUF_NFULL /* make full */; \
        ibuf_state |= IOBUF_NEMPTY /* mark not empty */; \
        ibuf_tail = t; \
    }





/* Accessed from the interrupt */
/*
 * Reads a byte from output buffer.
 *
 * Enables further USART interrupts on success.
 */
#define _oread(out) \
    if (obuf_state & IOBUF_NEMPTY) /* not empty */ \
    { \
        byte h = obuf_head; \
        out = obuf[h++]; /* read byte */ \
        if (h == OBUF_SIZE) h = 0; \
        if (h == obuf_tail) obuf_state &= ~IOBUF_NEMPTY; /* mark empty */ \
        obuf_state |= IOBUF_NFULL; /* mark not full */ \
        obuf_head = h; \
        UCSRB |= (1 << UDRIE); /* Enable Data Reg. Empty interrupt */ \
    }





/* Accessed from the user code; USART interrupts must be disabled manually */
/*
 * Writes the byte to output buffer.
 *
 * Returns true if byte was actually written, false otherwise
 */
inline bool _owrite(const byte &in)
{
    if (obuf_state & IOBUF_NFULL) /* not full */
    {
        byte t = obuf_tail;
        obuf[t++] = in; /* write byte */
        if (t == OBUF_SIZE) t = 0;
        if (t == obuf_head) obuf_state &= ~IOBUF_NFULL /* make full */;
        obuf_state |= IOBUF_NEMPTY; /* mark not empty */
        obuf_tail = t;
        return true;
    }
    return false;
}





inline byte isize()
{
    NO_RX_COMPLETE(
        byte result = 0;
        if (ibuf_state & IOBUF_NEMPTY) /* not empty */
        {
            /* Warnings are unreasonable for this block since
               interrupts are disabled and no concurrent
               access of variables occur. */
            if (ibuf_head < ibuf_tail)
            {
                result = (ibuf_tail - ibuf_head);
            }
            else
            {
                result = IBUF_SIZE - (ibuf_head - ibuf_tail);
            }
        }
    )
    return result;
}





inline bool transmit(const byte &in)
{
    NO_DATA_REG_EMPTY(
        bool result = _owrite(in);
    );
    return result;
}

inline bool receive(byte &out)
{
    NO_RX_COMPLETE(
        bool result = _iread(out);
    );
    return result;
}





#pragma vector=USART_RXC_vect
__interrupt void usart_rxc_interrupt_handler()
{
    NO_RX_COMPLETE(
        /* Allow nested interrupts */
        __enable_interrupt();
        
        /* Must read the data anyway
           in order to suppress unnecessary interrupts */
        byte udr = UDR; /* Reads from UDR */
        _iwrite(udr); /* Reads from udr */
    )
}

#pragma vector=USART_UDRE_vect
__interrupt void usart_udre_interrupt_handler()
{
    /* Must do this manually since UDRIE must
       be cleared if output buffer become empty
       in order to suppress unnecessary series of interrupts */
    UCSRB &= ~(1 << UDRIE); /* Disable Data Reg. Empty interrupt */
    /* Allow nested interrupts */
    __enable_interrupt();
    
    _oread(UDR); /* Writes to UDR */
}





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
