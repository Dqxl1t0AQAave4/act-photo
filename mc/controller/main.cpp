#define DLIB

#include <act-common/common.h>
#include <act-common/array.h>
#include <cstdlib>




// ========================================================================
// ===================== Critical Macrodefinitions ========================
// ========================================================================




/*
 * FOSC = 1MHz is confugured via Flash Fuses CKSEL=1 and SUT=2.
 */

#define BAUD_RATE    4800    // 4.8kbps
#define FOSC         1000000 // 1MHz
#define TC2_PRESCALE 1       // no Timer/Counter2 prescale
#define TC2_RELOCATE
#define TC2_RELOCPIN_SET()    PORTB |= (1 << PORTB1)
#define TC2_RELOCPIN_CLEAR()  PORTB &= ~(1 << PORTB1)
#define TC2_RELOCPIN_CONFIG() PORTB &= ~(1 << PORTB2); \
                              DDRB |= (1 << DDB2)

#define SPI_DORD 1 // LSB first
#define SPI_CPOL 0
#define SPI_CPHA 0
#define SPI_CR   2 // FOSC / 64

#define TWI_2M_ADDRESS        1
#define TWI_2M_MASTER_ADDRESS 2

typedef array < byte, 32, byte > ibuf_t;

#define TWI_IBUF_CONTAINER_T ibuf_t
#define SPI_IBUF_CONTAINER_T ibuf_t
#define TWI_OBUF_CONTAINER_T array < byte, byte(128), byte >
#define SPI_OBUF_CONTAINER_T array < byte, byte(128), byte >


#include <act-photo/usart.h>
#include <act-photo/twi.h>
#include <act-photo/spi.h>
#include <act-photo/pwm.h>



#define REMUX(c) ADMUX = (3<<REFS0)|(1<<ADLAR)|(c<<MUX0)




// ========================================================================
// ===================== Initialization ===================================
// ========================================================================




inline __monitor void init()
{
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

    /* USART initialization */
    usart_init();
    /* SPI initialization */
    spi_init();
    /* TWI initialization */
    twi_init();
    /* Timer/Counter2 initialization */
    init_tc2_wg();
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




inline byte transmit_all(byte * src, byte length)
{
    byte res = iobuf_write < sp_process_full, lp_use_lock > (usart_obuf, src, length);
    usart_notify();
    return res;
}

inline byte receive_all(byte * dst, byte length)
{
    return iobuf_read < sp_process_full, lp_use_lock > (dst, usart_ibuf, length);
}

inline byte receive(byte * dst)
{
    return iobuf_read < lp_use_lock > (*dst, usart_ibuf);
}


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
// ================= Shell-like Command Processors ========================
// ========================================================================


bool strstw(const char * pre, const char * str)
{
    byte lenpre = strlen(pre),
         lenstr = strlen(str);
    return (lenstr < lenpre) ? false : (strncmp(pre, str, lenpre) == 0);
}

byte itoa(char * out, unsigned int n)
{
    char * old = out; byte d;
    bool lz = true;
    if (((d = byte(n / 10000)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 10000; lz = false; }
    if (((d = byte(n / 1000)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 1000; lz = false; }
    if (((d = byte(n / 100)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 100; lz = false; }
    if (((d = byte(n / 10)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 10; }
    *out++ = (char) (n + '0');
    return (byte) (out - old);
}

byte itoa(char * out, byte n)
{
    char * old = out; byte d;
    bool lz = true;
    if (((d = byte(n / 100)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 100; lz = false; }
    if (((d = byte(n / 10)) != 0) || !lz) { *out++ = (char) (d + '0'); n -= d * 10; }
    *out++ = (char) (n + '0');
    return (byte) (out - old);
}

byte atoi(const char * in, byte len, unsigned int & n)
{
    byte c;
    if (len > 5) len = 5;
    n = 0;
    for (c = 0; c < len; ++c)
    {
        if ((byte(in[c]) < byte('0')) || (byte(in[c]) > byte('9'))) return c;
        n = n * 10 + (byte(in[c]) - byte('0'));
    }
    return c;
}

byte atoi(const char * in, byte len, byte & n)
{
    byte c;
    if (len > 3) len = 3;
    n = 0;
    for (c = 0; c < len; ++c)
    {
        if ((byte(in[c]) < byte('0')) || (byte(in[c]) > byte('9'))) return c;
        n = n * 10 + (byte(in[c]) - byte('0'));
    }
    return c;
}

byte wsskip(const char * in)
{
    byte c = 0;
    while (*(in + c) == ' ') ++c;
    return c;
}

ibuf_t shell_spi_ibuf;     /* command interpreter input buffer      */
byte   shell_spi_ibuf_len; /* command interpreter buffer position   */
ibuf_t shell_twi_ibuf;     /* command interpreter input buffer      */
byte   shell_twi_ibuf_len; /* command interpreter buffer position   */
char   sfmtbuf[64];
byte   sfmtpos;

#define sfmt_begin()  sfmtpos = 0
#define sfmt_str(str) strcpy((char*)sfmtbuf + sfmtpos, str); \
                      sfmtpos += sizeof(str) - 1
#define sfmt_num(num) sfmtpos += itoa((char*)sfmtbuf + sfmtpos, num)
#define sfmt_end()    *((char*)sfmtbuf + sfmtpos) = '\0'


void shell_read_pending_commands()
{
    shell_spi_ibuf_len += iobuf_read < sp_process_any, lp_use_lock > (
        shell_spi_ibuf + shell_spi_ibuf_len,
        spi_ibuf,
        array_size(shell_spi_ibuf) - shell_spi_ibuf_len
    );
    shell_twi_ibuf_len += iobuf_read < sp_process_any, lp_use_lock > (
        shell_twi_ibuf + shell_twi_ibuf_len,
        twi_ibuf,
        array_size(shell_twi_ibuf) - shell_twi_ibuf_len
    );
}

const char * shell_process_command(ibuf_t & buf, byte & len)
{
    byte cmd_end = 0;
    for (byte i = 0; i < len; ++i)
    {
        if (char(buf[i]) == '\0')
        {
            cmd_end = i + 1;
            break;
        }
    }
    /* no complete command in the buffer, wait */
    if (cmd_end == 0)
    {
        /* buffer overflow, clear the buffer */
        if (len == array_size(buf))
        {
            len = 0;
        }
        return 0;
    }
    bool unknown_cmd = false;
    const char * cmd = (const char *) buf.data;
    const char * result;
    if (strstw("help", cmd))
    {
        result =
            "act-photo cmd interpreter:\r\n\r\n"
            "  help\r\n"
            "  echo <text>\r\n"
            "  adcdump - dump adc\r\n"
            "  getcoef"
            "  setcoef -(p|i|s) <m> <d>";
    }
    else if (strstw("echo ", cmd))
    {
        strcpy(sfmtbuf, cmd + sizeof("echo ") - 1);
        result = sfmtbuf;
    }
    else if (strstw("adcdump", cmd))
    {
        sfmt_begin();
            sfmt_str("adc1="); sfmt_num(adc1);
            sfmt_str(", adc2="); sfmt_num(adc2);
        sfmt_end();
        result = sfmtbuf;
    }
    else if (strstw("getcoef", cmd))
    {
        sfmt_begin();
            sfmt_str("kp=("); sfmt_num(kp_m); sfmt_str(","); sfmt_num(kp_d); sfmt_str("), ");
            sfmt_str("ki=("); sfmt_num(ki_m); sfmt_str(","); sfmt_num(ki_d); sfmt_str("), ");
            sfmt_str("ks=("); sfmt_num(ks_m); sfmt_str(","); sfmt_num(ks_d); sfmt_str(")");
        sfmt_end();
        result = sfmtbuf;
    }
    else if (strstw("setcoef -", cmd))
    {
        sfmtpos = sizeof("setcoef -") - 1;
        char k; unsigned int m; byte d;
        k = cmd[sfmtpos]; ++sfmtpos;
        sfmtpos += wsskip(cmd + sfmtpos);
        sfmtpos += atoi(cmd + sfmtpos, cmd_end - sfmtpos, m);
        sfmtpos += wsskip(cmd + sfmtpos);
        sfmtpos = atoi(cmd + sfmtpos, cmd_end - sfmtpos, d);
        if (!sfmtpos)
        {
            unknown_cmd = true;
        }
        else
        {
            if (k == 'p')
            {
                kp_m = m; kp_d = (byte)(d);
            }
            else if (k == 'i')
            {
                ki_m = m; ki_d = (byte)(d);
            }
            else if (k == 's')
            {
                ks_m = m; ks_d = (byte)(d);
            }
            else
            {
                unknown_cmd = true;
            }
            if (!unknown_cmd)
            {
                sfmt_begin();
                    sfmt_str("setting m="); sfmt_num(m); sfmt_str(", ");
                    sfmt_str("d="); sfmt_num(d);
                sfmt_end();
                result = sfmtbuf;
            }
        }
    }
    else
    {
        unknown_cmd = true;
    }
    if (unknown_cmd)
    {
        result = "unknown or incorrect command";
    }
    len -= cmd_end;
    for (byte i = 0; i < len; ++i)
    {
        buf[i] = buf[i + cmd_end];
    }
    return result;
}

void shell_process_pending_commands()
{
    const char * result;
    result = shell_process_command(shell_twi_ibuf, shell_twi_ibuf_len);
    if (result)
    {
        iobuf_write < sp_process_full, lp_use_lock > (twi_obuf, (byte *) result, strlen(result) + 1);
        twi_notify();
    }
    result = shell_process_command(shell_spi_ibuf, shell_spi_ibuf_len);
    if (result)
    {
        iobuf_write < sp_process_full, lp_use_lock > (spi_obuf, (byte *) result, strlen(result) + 1);
        spi_notify();
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
        
        
        /* Shell Command Processor */


        shell_process_pending_commands();


        /* Program Body */
        
        
        if (!pause)
        {
            do_computations();
        }
        
        
    }
    
    
    return 0;
}
