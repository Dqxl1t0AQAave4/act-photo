#pragma once

typedef unsigned char byte;

// Requires IBUFSIZE and OBUFSIZE be defined
// iobuf_state must be initialized

byte ibuf[IBUF_SIZE];
byte obuf[OBUF_SIZE];

#define IBUF_NEMPTY 0x1
#define IBUF_NFULL  0x2
#define OBUF_NEMPTY 0x4
#define OBUF_NFULL  0x8

/*
 * Since the variable to be accessed very often,
 * it seems necessary to declare it as __regvar.
 */
__regvar __no_init
volatile byte iobuf_state @ 15;

__tiny volatile byte ibuf_head   = 0;
__tiny volatile byte ibuf_tail   = 0;
__tiny volatile byte obuf_head   = 0;
__tiny volatile byte obuf_tail   = 0;

/* Accessed by the user code; USART interrupts must be disabled manually */
/*
 * Reads a byte from input buffer.
 *
 * Returns true if byte was actually read, false otherwise
 */
inline bool iread(byte &out)
{
    if (iobuf_state & IBUF_NEMPTY) /* not empty */
    {
        byte h = ibuf_head;
        out = ibuf[h++]; /* read byte */
        if (h == IBUF_SIZE) h = 0;
        if (h == ibuf_tail)
        {
            iobuf_state &= ~IBUF_NEMPTY /* mark empty */;
        }
        iobuf_state |= IBUF_NFULL /* mark not full */;
        ibuf_head = h;
        return true;
    }
    return false;
}

/* Accessed in the interruption code */
/*
 * Writes the byte to output buffer.
 */
#define iwrite(in) \
    if (iobuf_state & IBUF_NFULL) /* not full */ \
    { \
        byte t = ibuf_tail; \
        ibuf[t++] = in; /* write byte */ \
        if (t == IBUF_SIZE) t = 0; \
        if (t == ibuf_head) iobuf_state &= ~IBUF_NFULL /* make full */; \
        iobuf_state |= IBUF_NEMPTY /* mark not empty */; \
        ibuf_tail = t; \
    }

/* Accessed in the interruption code */
/*
 * Reads a byte from output buffer.
 *
 * Disables further USART interrupts on failure
 */
#define oread(out) \
    if (iobuf_state & OBUF_NEMPTY) /* not empty */ \
    { \
        byte h = obuf_head; \
        out = obuf[h++]; /* read byte */ \
        if (h == OBUF_SIZE) h = 0; \
        if (h == obuf_tail) iobuf_state &= ~OBUF_NEMPTY; /* mark empty */ \
        iobuf_state |= OBUF_NFULL; /* mark not full */ \
        obuf_head = h; \
    } \
    else \
    { \
        UCSRB &= ~(1 << UDRIE); /* Unset Data Reg. Empty interrupt */ \
    }


/* Accessed by the user code; USART interrupts must be disabled manually */
/*
 * Writes the byte to output buffer.
 */
inline void owrite(const byte &in)
{
    if (iobuf_state & OBUF_NFULL) /* not full */
    {
        byte t = obuf_tail;
        obuf[t++] = in; /* write byte */
        if (t == OBUF_SIZE) t = 0;
        if (t == obuf_head) iobuf_state &= ~OBUF_NFULL /* make full */;
        iobuf_state |= OBUF_NEMPTY; /* mark not empty */ \
        obuf_tail = t;
    }
}

inline byte isize()
{
    UCSRB &= ~(1 << RXCIE); /* Unset RX Complete interrupt */
    byte result = 0;
    if (iobuf_state & IBUF_NEMPTY) /* not empty */
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
    UCSRB |= (1 << RXCIE); /* Set RX Complete interrupt */
    return result;
}

inline void transmit(const byte &in)
{
    UCSRB &= ~(1 << UDRIE); /* Unset Data Reg. Empty interrupt */
    owrite(in);
    UCSRB |= (1 << UDRIE); /* Set Data Reg. Empty interrupt */
}

inline bool receive(byte &out)
{
    UCSRB &= ~(1 << RXCIE); /* Unset RX Complete interrupt */
    bool result = iread(out);
    UCSRB |= (1 << RXCIE); /* Set RX Complete interrupt */
    return result;
}

#pragma vector=USART_RXC_vect
__interrupt void usart_rxc_interrupt_handler()
{
    iwrite(UDR);
}

#pragma vector=USART_UDRE_vect
__interrupt void usart_udre_interrupt_handler()
{
    /* Must read the data anyway
       in order to suppress unnecessary interrupts */
    byte udr = UDR;
    oread(udr);
}

/* Disable interrupts (see ATmega8A datasheet) */
inline __monitor void usart_init()
{
    const unsigned int ubrr = 1843200/16/9600-1;
    /* Set baud rate */
    UBRRH = (unsigned char) (ubrr>>8);
    UBRRL = (unsigned char) ubrr;
    /* Enable receiver and transmitter */
    UCSRB = (1<<RXEN)|(1<<TXEN);
    /* Set frame format: 8data, 1stop bit */
    UCSRC = (1<<URSEL)|(3<<UCSZ0);
     /* Set RX Complete interrupt */
    UCSRB |= (1 << RXCIE);
}
