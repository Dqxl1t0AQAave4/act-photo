// controller-gui-test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <cmath>

HANDLE m_Handle;

#define TIMEOUT 1000
#define BAUDRATE 4800

#define SIN_BYTE(phi) (unsigned char) (127 + 128 * sin(phi))
#define SIN_SHORT(phi) (unsigned short int) (32767 + 32768 * sin(phi))
#define SIN_SBYTE(phi) (unsigned char) (255 * sin(phi))
#define SIN_SSHORT(phi) (unsigned short int) ((short int) (32767 * sin(phi)))

int _tmain(int argc, _TCHAR* argv[])
{
    LPCTSTR port_name = _T("COM3");
    if (argc > 1)
    {
        port_name = argv[argc - 1];
    }
    m_Handle = CreateFile(
        port_name,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (m_Handle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Cannot open port.";
        return 1;
    }

    SetCommMask(m_Handle, EV_RXCHAR);
    SetupComm(m_Handle, 1500, 1500);

    COMMTIMEOUTS CommTimeOuts;
    CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
    CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
    CommTimeOuts.ReadTotalTimeoutConstant = TIMEOUT;
    CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
    CommTimeOuts.WriteTotalTimeoutConstant = TIMEOUT;

    if (!SetCommTimeouts(m_Handle, &CommTimeOuts))
    {
        CloseHandle(m_Handle);
        std::cerr << "Cannot setup port timeouts.";
        return 1;
    }

    DCB ComDCM;

    memset(&ComDCM, 0, sizeof(ComDCM));
    ComDCM.DCBlength = sizeof(DCB);
    GetCommState(m_Handle, &ComDCM);
    ComDCM.BaudRate = DWORD(BAUDRATE);
    ComDCM.ByteSize = 8;
    ComDCM.Parity = ODDPARITY;
    ComDCM.StopBits = ONESTOPBIT;
    ComDCM.fAbortOnError = TRUE;
    ComDCM.fDtrControl = DTR_CONTROL_DISABLE;
    ComDCM.fRtsControl = RTS_CONTROL_DISABLE;
    ComDCM.fBinary = TRUE;
    ComDCM.fParity = TRUE;
    ComDCM.fInX = FALSE;
    ComDCM.fOutX = FALSE;
    ComDCM.XonChar = 0;
    ComDCM.XoffChar = (unsigned char) 0xFF;
    ComDCM.fErrorChar = FALSE;
    ComDCM.fNull = FALSE;
    ComDCM.fOutxCtsFlow = FALSE;
    ComDCM.fOutxDsrFlow = FALSE;
    ComDCM.XonLim = 128;
    ComDCM.XoffLim = 128;

    if (!SetCommState(m_Handle, &ComDCM))
    {
        CloseHandle(m_Handle);
        std::cerr << "Cannot setup port configuration.";
        return 1;
    }

    /*
     * Use packet format:
     *     packet: < 0 adc1 adc2 cerr interr pwm ocr2 >
     *     bytes:  < 1    1    1    2      2   2    1 >   =>   10 bytes
     */

    double t = 0; unsigned short int s;
    while (true)
    {
        char packet[10] = { 0 };
        s = SIN_BYTE(t);       packet[1] = s; // adc1
        s = SIN_BYTE(-t);      packet[2] = s; // adc2
        s = SIN_SSHORT(t);     packet[3] = (s >> 8); packet[4] = (s & 0xff); // cerr
        s = SIN_SSHORT(-t);    packet[5] = (s >> 8); packet[6] = (s & 0xff); // interr
        s = SIN_SSHORT(2 * t); packet[7] = (s >> 8); packet[8] = (s & 0xff); // pwm
        s = SIN_BYTE(2 * t);   packet[9] = s; // ocr2
        DWORD written;
        if (!WriteFile(m_Handle, packet, 10, &written, 0) || written != 10)
        {
            CloseHandle(m_Handle);
            std::cerr << "Cannot send data.";
            return 1;
        }
        t += 0.05;
    }

	return 0;
}

