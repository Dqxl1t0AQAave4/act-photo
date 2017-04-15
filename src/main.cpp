#include "common.h"

#define IBUF_SIZE    16
#define OBUF_SIZE    32
#define BAUD_RATE    4800
#define FOSC         1000000
#define TC2_PRESCALE 1

#include "usart.h"
#include "pwm.h"


#define REMUX(c) ADMUX = (3<<REFS0)|(1<<ADLAR)|(c<<MUX0)


inline __monitor void init()
{
    /* USART initialization */
    usart_init();
    /* Timer/Counter2 initialization */
    init_tc2_wg();
    /* ADC initialization (1/8 prescaling,
       1st channel, internal Vref, left alignment) */
    REMUX(0);
    ADCSR = (1<<ADEN)|(3<<ADPS0);
    
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
    
    
/* Common variables */


sbyte kp = 1,       /* proportional factor */
      ki = -7,      /* memory factor       */
      ks = 0        /* scale factor        */
      ;

bool  pause = false;/* USART async/sync transmit operation */
bool  sync  = true; /* USART async/sync transmit operation */

byte  adc1, adc2;   /* ADC 1st and 2nd channel */

int   err = 0;      /* current error */

    
/* Message Loop variables */


byte command;
bool command_present = false;


/* Input messages */


#define COMMAND_SET_COEFS  byte(0xFC)
#define COMMAND_SET_SYNC   byte(0xFD)
#define COMMAND_PAUSE      byte(0xFE)
#define _COMMAND_SET_COEFS byte(~0xFC)
#define _COMMAND_SET_SYNC  byte(~0xFD)
#define _COMMAND_PAUSE     byte(~0xFE)


/* Output messages */


#define COMMAND_SEND_DATA  byte(0xFF)
#define _COMMAND_SEND_DATA byte(~0xFF)


void answer(byte *data, byte size)
{
    if (!sync)
    {
        for (int i = 0; i < size; i++)
        {
            transmit(data[i]);
        }
    }
    else
    {
        for (int i = 0; i < size; i++)
        {
            while (!transmit(data[i])) // wait in sync mode
              ;
        }
    }
}


#define ANSWER(arr,size,cmd, ncmd) \
  arr[0] = cmd; arr[1] = cmd; arr[size-1] = ncmd; arr[size-2] = ncmd; answer(arr, size);


/* Command processor */


void process_command()
{
    byte args[5];
    byte answ[4];
    switch(command)
    {
    case COMMAND_SET_COEFS:
        if (isize() < 5) return; // S-kpki-00ks-St-St
        for (int i = 0; i < 5; i++) receive(args[i]);
        // validate data packet
        if ((args[0] != COMMAND_SET_COEFS)  ||
            (args[3] != _COMMAND_SET_COEFS) ||
            (args[4] != _COMMAND_SET_COEFS)) break;
        // unpack data
        kp = ((args[1] >> 4) & 0x7); if ((args[1] >> 7) == 1) kp = -kp;
        ki = (args[1] & 0x7); if(((args[1] >> 3) & 0x1) == 1) ki = -ki;
        ks = (args[2] & 0xf);            // positive only
        // answer
        ANSWER(answ, 4, COMMAND_SET_COEFS, _COMMAND_SET_COEFS)
        break;
    case COMMAND_SET_SYNC:
        if (isize() < 4) return; // S-sync-St-St
        for (int i = 0; i < 4; i++) receive(args[i]);
        // validate data packet
        if (args[0] != COMMAND_SET_SYNC  ||
            args[2] != _COMMAND_SET_SYNC ||
            args[3] != _COMMAND_SET_SYNC) break;
        // unpack data
        sync = args[1];
        // answer
        ANSWER(answ, 4, COMMAND_SET_SYNC, _COMMAND_SET_SYNC)
        break;
    case COMMAND_PAUSE:
        if (isize() < 3) return; // S-St-St
        for (int i = 0; i < 3; i++) receive(args[i]);
        // validate data packet
        if (args[0] != COMMAND_PAUSE  ||
            args[1] != _COMMAND_PAUSE ||
            args[3] != _COMMAND_PAUSE) break;
        // execute
        pause = !pause;
        // answer
        ANSWER(answ, 4, COMMAND_SET_SYNC, _COMMAND_SET_SYNC)
        break;
    }
    command_present = false;
}


/* Program logic */


void do_computations()
{
    /* Read ADC input */
  
    REMUX(0);                       /* 1st ADC channel                    */
    ADCSR |= (1 << ADSC);           /* Initialize single-ended conversion */
    while(!(ADCSR & (1 << ADIF)))   /* Wait for the end of the conversion */
      ;
    adc1 = ADCH;                    /* Read the 8-bit conversion result   */
    ADCSR &= ~(1 << ADIF);          /* Clear Conversion Complete flag     */
    
    REMUX(1);                       /* 2nd ADC channel                    */
    ADCSR |= (1 << ADSC);           /* Initialize single-ended conversion */
    while(!(ADCSR & (1 << ADIF)))   /* Wait for the end of the conversion */
      ;
    adc2 = ADCH;                    /* Read the 8-bit conversion result   */
    ADCSR &= ~(1 << ADIF);          /* Clear Conversion Complete flag     */
    
    /* Calculate PWM width */
    
    int e = (int(adc2) - int(adc1));
    int pwmw = ABS(err);
    if (ulog2(pwmw) + ki < 15) // signed 16bit int = 2x 15bit
    {
        pwmw = (ki < 0 ? (err >> (-ki)) : (err << ki));
        pwmw = safe_add(pwmw, (kp < 0 ? (e >> (-kp)) : (e << kp)));
    }
    else
    {
        pwmw = (err < 0 ? INT_MIN : INT_MAX);
    }
    
    err = safe_add(err, e);

    /* Answer */
    
    byte report[11];
    report[2] = adc1;
    report[3] = adc2;
    report[4] = (((unsigned int) err) >> 8);
    report[5] = (((unsigned int) err) & 0xff);
    report[4] = (((unsigned int) pwmw) >> 8);
    report[5] = (((unsigned int) pwmw) & 0xff);
    report[8] = byte(pwmw >> ks);
    ANSWER(report, 11, COMMAND_SEND_DATA, _COMMAND_SEND_DATA);
    
    /* Setup PWM width */
    
    OCR2 = (pwmw < 0 ? INT_MAX + (pwmw >> ks) : (((unsigned int) pwmw) >> ks) + INT_MAX);
}


/* Program entry point */


int main()
{
    
    /* Initialization here */
    
    
    init();
    
    
    /* Program Loop */
    
    
    for(;;)
    {
        
        
        /* Message Loop */
        
        
        if (command_present)
        {
            process_command();
        }
        else
        {
            command_present = receive(command);
        }
        
        
        /* Program Body */
        
        
        if (!pause)
        {
            do_computations();
        }
        
        
    }
    
    
    return 0;
}
