#define DLIB

#include <act-common/common.h>

#define BAUD_RATE    4800
#define FOSC         1000000
#define TC2_PRESCALE 1

#include <act-photo/usart.h>
#include <act-photo/pwm.h>

inline __monitor void init()
{
    /* USART initialization */
    usart_init();
    
    /* Pin configuration */
    PORTB = (1<<PORTB0)|(1<<PORTB1)|(1<<PORTB2)|
            (0<<PORTB3)|(1<<PORTB4)|(1<<PORTB5)|
            (1<<PORTB6)|(1<<PORTB7);
    DDRB = (0<<DDB0)|(0<<DDB1)|(0<<DDB2)|
           (1<<DDB3)|(0<<DDB4)|(0<<DDB5)|
           (0<<DDB6)|(0<<DDB7);
    PORTC = (0<<PORTC0)|(0<<PORTC1)|(1<<PORTC2)|
            (1<<PORTC3)|(1<<PORTC4)|(1<<PORTC5)|
            (1<<PORTC6);
    DDRC = (0<<DDC0)|(0<<DDC1)|(0<<DDC2)|
           (0<<DDC3)|(0<<DDC4)|(0<<DDC5)|
           (0<<DDC6);
    PORTD = (0<<PORTD0)|(0<<PORTD1)|(1<<PORTD2)|
            (1<<PORTD3)|(1<<PORTD4)|(1<<PORTD5)|
            (1<<PORTD6)|(1<<PORTD7);
    DDRD = (0<<DDD0)|(1<<DDD1)|(0<<DDD2)|
           (0<<DDD3)|(0<<DDD4)|(0<<DDD5)|
           (0<<DDD6)|(0<<DDD7);
}


/* Program entry point */


int main()
{
    
    /* Initialization here */
    
    
    init();
    
    __enable_interrupt();
    
    
    /* Program Loop */
    
    
    transmit(0x8);
    for(byte pb = 0; pb <= 7; pb++) // 80 sec
    {
        transmit(pb);
        for (byte t = 0; t < 100; t++) // 10 sec
        {
            PORTB = ((!(t & 1)) ? ~(1 << pb) : 0xff);
            __delay_cycles(100000); // 1MHz=1.000.000Hz => wait 0.1 sec
        }
    }
    
    transmit(0x9);
    for(byte pc = 0; pc <= 6; pc++) // 70 sec
    {
        transmit(pc);
        for (byte t = 0; t < 100; t++) // 10 sec
        {
            PORTC = ((!(t & 1)) ? ~(1 << pc) : 0xff);
            __delay_cycles(100000); // 1MHz=1.000.000Hz => wait 0.1 sec
        }
    }
    
    transmit(0xA);
    for(byte pd = 0; pd <= 7; pd++) // 80 sec
    {
        transmit(pd);
        for (byte t = 0; t < 100; t++) // 10 sec
        {
            PORTD = ((!(t & 1)) ? ~(1 << pd) : 0xff);
            __delay_cycles(100000); // 1MHz=1.000.000Hz => wait 0.1 sec
        }
    }
    
    transmit(0xB);
    
    init_tc2_wg();
    
    for (byte t = 0; t < 255; t++) // 255 sec
    {
        OCR2 = t;
        __delay_cycles(1000000); // 1MHz=1.000.000Hz => wait 1 sec
    }
    
    transmit(0xC);
    transmit(0xD);
    transmit(0xE);
    transmit(0xF);
    
    return 0;
}
