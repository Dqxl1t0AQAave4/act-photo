#include <common.h>




// ========================================================================
// ===================== Critical Macrodefinitions ========================
// ========================================================================




/*
 * FOSC = 1MHz is confugured via Flash Fuses CKSEL=1 and SUT=2.
 */

#define IBUF_SIZE    16
#define OBUF_SIZE    32
#define BAUD_RATE    4800    // 4.8kbps
#define FOSC         1000000 // 1MHz
#define TC2_PRESCALE 1       // no Timer/Counter2 prescale



#include <usart.h>
#include <pwm.h>



#define REMUX(c) ADMUX = (3<<REFS0)|(1<<ADLAR)|(c<<MUX0)




// ========================================================================
// ===================== Initialization ===================================
// ========================================================================




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





// ========================================================================
// ===================== Common Variables =================================
// ========================================================================





sbyte kp = 1,         /* proportional factor                 */
      ki = -7,        /* memory factor                       */
      ks = 8          /* scale factor                        */
      ;

bool  pause  = false; /* paused state                        */
bool  sync   = false; /* USART async/sync transmit operation */

byte  adc1, adc2;     /* ADC 1st and 2nd channel             */

int   err = 0;        /* current error                       */





// ========================================================================
// ===================== Message Loop Variables ===========================
// ========================================================================




/*
 * #33 <enchancement>
 *
 *     Simplify IO protocol.
 *
 * The new protocol is user-friendly and quite simple.
 * The typical command starts with `METHOD`
 * (1 byte: `SET`, `GET`, `ECHO`, etc.). The next byte is
 * method-dependent. E.g. `VARIABLE` for `SET` and `GET`.
 * `SET` method also requires the variable value (1..any bytes)
 * to be set.
 *
 * COMMAND = METHOD [CONTENT]
 * METHOD = `SET` | `GET` | `ECHO` | BYTE
 * CONTENT = {
 *     METHOD=`SET`  => VARIABLE [DATA],
 *     METHOD=`GET`  => VARIABLE,
 *     METHOD=`ECHO` => BYTE,
 *     METHOD=BYTE   => any
 * }
 * VARIABLE = BYTE
 * DATA = any
 *
 * BYTE = 1 any byte
 * any = any bytes
 */


byte command_method;
bool command_present = false;
byte command_variable;
bool command_variable_present = false;


#define METHOD_GET     byte(0)
#define METHOD_SET     byte(1)
#define METHOD_ECHO    byte(2)

// SET
#define VAR_PAUSE      byte(0)
#define VAR_SYNC_USART byte(1)
#define VAR_KP         byte(2)
#define VAR_KI         byte(3)
#define VAR_KS         byte(4)

// GET
#define NO_OUTPUT      byte(0)
#define VAR_ADC1       byte(1)
#define VAR_ADC2       byte(2)
#define VAR_INT_ERR    byte(3)
#define VAR_CUR_ERR    byte(4)
#define VAR_PWM        byte(5)
#define VAR_OCR2       byte(6)


byte output_mode = NO_OUTPUT;






// ========================================================================
// ===================== Utility Functions ================================
// ========================================================================







void send(byte *data, byte size)
{
    if (!sync)
    {
        transmit(data, size);
    }
    else
    {
        while (!transmit(data, size)) // wait in sync mode
            ;
    }
}


void send(byte data)
{
    send(&data, 1);
}


void send(unsigned int data)
{
    byte lo_hi[2] = { byte(data & 0xff), byte(data >> 8) };
    send(lo_hi, 2);
}





// ========================================================================
// ===================== Command Processors ===============================
// ========================================================================





bool process_set_variable()
{
    byte value;
    if (!receive(value)) return false; // wait for the value
    switch(command_variable)
    {
    case VAR_PAUSE:
        pause = value;
        break;
    case VAR_SYNC_USART:
        sync = value;
        break;
    case VAR_KP:
        kp = (sbyte) value;
        break;
    case VAR_KI:
        ki = (sbyte) value;
        break;
    case VAR_KS:
        ks = (sbyte) value;
        break;
    }
    return true;
}


void process_command()
{
    switch(command_method)
    {
    case METHOD_ECHO:
        byte value;
        if (!receive(value)) return; // wait for the value to send back
        send(value);
        break;
    case METHOD_GET:
        if (!receive(output_mode)) return; // wait for the variable
        break;
    case METHOD_SET:
        // wait for the variable
        if (!command_variable_present)
        {
            if (!(command_variable_present = receive(command_variable))) return;
        }
        if (!process_set_variable()) return; // wait for the argument(s)
        break;
    }
    command_present = false;
    command_variable_present = false;
}





// ========================================================================
// ===================== Program Logic ====================================
// ========================================================================





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
    
    /*
     * pwmw = KP * e + KI * err, where KP = 2^kp, KI = 2^ki
     */
    
    int e = (int(adc2) - int(adc1));
    int pwmw = ABS(err);
    if (ulog2(pwmw) + ki <= 15) // signed 16bit int = sign bit + 15bit
    {
        pwmw = (ki < 0 ? (err >> (-ki)) : (err << ki));
        pwmw = safe_add(pwmw, (kp < 0 ? (e >> (-kp)) : (e << kp)));
    }
    else
    {
        pwmw = (err < 0 ? INT_MIN : INT_MAX);
    }
    
    err = safe_add(err, e);
    
    /* Setup PWM width */
    
    /*
     * Must transform pwmw from signed int to unsigned byte.
     * Add INT_MAX to get rid of negative values and divide
     * by 2^ks.
     */
    
    OCR2 = (
               pwmw < 0 ?
               INT_MAX + pwmw :
               (unsigned int) (pwmw) + INT_MAX
           ) >> ks;

    /* Report back */
    
    switch(output_mode)
    {
    case NO_OUTPUT:
        break;
    case VAR_ADC1:
        send(adc1);
        break;
    case VAR_ADC2:
        send(adc2);
        break;
    case VAR_INT_ERR:
        send((unsigned int) err);
        break;
    case VAR_CUR_ERR:
        send((unsigned int) e);
        break;
    case VAR_PWM:
        send((unsigned int) pwmw);
        break;
    case VAR_OCR2:
        send(OCR2);
        break;
    }
}





// ========================================================================
// ===================== Program Entry Point ==============================
// ========================================================================





int main()
{
    
    /* Initialization here */
    
    
    init();
    
    __enable_interrupt();
    
    
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
            command_present = receive(command_method);
        }
        
        
        /* Program Body */
        
        
        if (!pause)
        {
            do_computations();
        }
        
        
    }
    
    
    return 0;
}
