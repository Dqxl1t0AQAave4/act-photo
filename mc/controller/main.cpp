#include <act-common/common.h>




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



#include <act-common/usart.h>
#include <act-photo/pwm.h>



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



/*
 * #40 <bug> <enchancement>
 *
 *     Modify coefficient format.
 *
 * Let C = one of { KP KI KS }.
 *
 *     C * x = (M * x) >> d
 *
 * Since ATMega8 does not support floating point operations,
 * and just (x << d) or (x >> d) cannot provide precise regulation.
 */


unsigned int kp_m = 0; byte kp_d = 0;   /* proportional factor */
unsigned int ki_m = 0; byte ki_d = 0;   /* integral factor     */
unsigned int ks_m = 0; byte ks_d = 0;   /* scale factor        */

#define COEF(c, x) (((long) x) * (c##_m)) >> (c##_d)

bool  pause  = false; /* paused state                          */
bool  sync   = false; /* USART async/sync transmit operation   */

byte  adc1, adc2;     /* ADC 1st and 2nd channel               */

int   int_err = 0;    /* integral error                        */
int   cur_err = 0;    /* current error                         */
int   pwm     = 0;    /* PWM                                   */





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

/*
 * #47 <enchancement>
 *
 *     Add `COEFS` variable and `PACKET` output format
 *     to support controller-gui tool.
 *
 * Data packet GET format is:
 *     <0 ADC1 ADC2 CUR_ERR INT_ERR PWM OCR2>
 * Coefficient packet SET format is:
 *     <KP KI KS>
 */

#define METHOD_GET     (byte) 0
#define METHOD_SET     (byte) 1
#define METHOD_ECHO    (byte) 2

// SET
#define VAR_PAUSE      (byte) 0
#define VAR_SYNC_USART (byte) 1
#define VAR_KP         (byte) 2
#define VAR_KI         (byte) 3
#define VAR_KS         (byte) 4
#define VAR_COEFS      (byte) 5

// GET
#define NO_OUTPUT      (byte) 0
#define VAR_ADC1       (byte) 1
#define VAR_ADC2       (byte) 2
#define VAR_INT_ERR    (byte) 3
#define VAR_CUR_ERR    (byte) 4
#define VAR_PWM        (byte) 5
#define VAR_OCR2       (byte) 6
#define VAR_PACKET     (byte) 7


byte output_mode = NO_OUTPUT;






// ========================================================================
// ===================== Utility Functions ================================
// ========================================================================







void send_all(byte *data, byte size)
{
    if (!sync)
    {
        transmit_all(data, size);
    }
    else
    {
        while (!transmit_all(data, size)) // wait in sync mode
            ;
    }
}


void send_byte(byte data)
{
    send_all(&data, 1);
}


void send_int(unsigned int data)
{
    byte hi_lo[2] = { (byte) (data >> 8), (byte) (data & 0xff) };
    send_all(hi_lo, 2);
}


void send_packet(/* byte adc1, byte adc2, int cur_err, int int_err, int pwm, byte OCR2 */)
{
    /*
     * #53 <enchancement>
     *     Review the packet format.
     *     
     * The new packet format is < delimiter | data | checksum >,
     * where checksum is calculated as delimiter ^ adc1 ^ (cerr & 0xff) ^ ocr2.
     */
    
    byte packet[11] = {
       103,
       adc1,
       adc2,
       ((unsigned int) cur_err) >> 8, cur_err & 0xff,
       ((unsigned int) int_err) >> 8, int_err & 0xff,
       ((unsigned int) pwm) >> 8,     pwm     & 0xff,
       OCR2
    };
    packet[10] = packet[0] ^ packet[1] ^ packet[4] ^ packet[9];
    send_all(packet, 11);
}





// ========================================================================
// ===================== Command Processors ===============================
// ========================================================================





bool process_set_variable()
{
    byte packet[9];
    switch(command_variable)
    {
    case VAR_PAUSE:
        if (!receive(&packet[0])) return false; // wait for the value
        pause = packet[0];
        break;
    case VAR_SYNC_USART:
        if (!receive(packet)) return false; // wait for the value
        sync = packet[0];
        break;
    case VAR_KP:
        if (!receive_all(packet, 3)) return false; // wait for the value
        kp_m = (((unsigned int) packet[0]) << 8) | (packet[1]);
        kp_d = packet[2];
        break;
    case VAR_KI:
        if (!receive_all(packet, 3)) return false; // wait for the value
        ki_m = (((unsigned int) packet[0]) << 8) | (packet[1]);
        ki_d = packet[2];
        break;
    case VAR_KS:
        if (!receive_all(packet, 3)) return false; // wait for the value
        ks_m = (((unsigned int) packet[0]) << 8) | (packet[1]);
        ks_d = packet[2];
        break;
    case VAR_COEFS:
        if (!receive_all(packet, 9)) return false; // wait for the value
        kp_m = (((unsigned int) packet[0]) << 8) | (packet[1]);
        kp_d = packet[2];
        ki_m = (((unsigned int) packet[3]) << 8) | (packet[4]);
        ki_d = packet[5];
        ks_m = (((unsigned int) packet[6]) << 8) | (packet[7]);
        ks_d = packet[8];
        break;
    }
    return true;
}


void process_command()
{
    byte echo_value;
    switch(command_method)
    {
    case METHOD_ECHO:
        if (!receive(&echo_value)) return; // wait for the value to send back
        send_byte(echo_value);
        break;
    case METHOD_GET:
        if (!receive(&output_mode)) return; // wait for the variable
        break;
    case METHOD_SET:
        // wait for the variable
        if (!command_variable_present)
        {
            if (!(command_variable_present = receive(&command_variable))) return;
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
     * pwm = KP * e + KI * err
     */
    
    cur_err = ((int)(adc2) - (int)(adc1));
    
    long p = COEF(kp, cur_err), q = COEF(ki, int_err);
    if (ABS(p) > INT_MAX) p = ((p < 0) ? (INT_MIN) : (INT_MAX));
    if (ABS(q) > INT_MAX) q = ((q < 0) ? (INT_MIN) : (INT_MAX));
    pwm = safe_add((int) p, (int) q);
    
    int_err = safe_add(int_err, cur_err);
    
    /* Setup PWM width */
    
    long t = COEF(ks, pwm) + INT_MAX;
    if (ABS(t) > INT_MAX) t = ((p < 0) ? (INT_MIN) : (INT_MAX));
    
    OCR2 = (((unsigned int) t) + INT_MAX) >> 8;

    /* Report back */
    
    switch(output_mode)
    {
    case NO_OUTPUT:
        break;
    case VAR_ADC1:
        send_byte(adc1);
        break;
    case VAR_ADC2:
        send_byte(adc2);
        break;
    case VAR_INT_ERR:
        send_int(int_err);
        break;
    case VAR_CUR_ERR:
        send_int(cur_err);
        break;
    case VAR_PWM:
        send_int(pwm);
        break;
    case VAR_OCR2:
        send_byte(OCR2);
        break;
    case VAR_PACKET:
        send_packet();
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
            command_present = receive(&command_method);
        }
        
        
        /* Program Body */
        
        
        if (!pause)
        {
            do_computations();
        }
        
        
    }
    
    
    return 0;
}
