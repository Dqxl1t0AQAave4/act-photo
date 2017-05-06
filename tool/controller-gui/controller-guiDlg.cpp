
// controller-guiDlg.cpp : implementation file
//

#include "stdafx.h"
#include "controller-gui.h"
#include "controller-guiDlg.h"
#include "afxdialogex.h"

#include "common.h"
#include "plot.h"

#include <list>
#include <thread>
#include <mutex>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_UPDATEDATA_MESSAGE (WM_USER + 1000)
#define WM_LOG_MESSAGE (WM_USER + 1001)

using namespace common;

#define LOCK() std::lock_guard<decltype(guard)> lock(guard);
#define LOG(text) RequestLog(_T(text));
#define ERR(handle, text) CloseHandle(handle); handle = INVALID_HANDLE_VALUE; LOG(text);
#define PLOT(ctrl, p)\
    ctrl.plot_layer.with(\
        (plot::plot_builder() << p)\
        .with_ticks(plot::palette::pen(RGB(150, 150, 0)))\
        .with_x_ticks(0, 10, 0)\
        .with_y_ticks(0, 5, 0)\
        .build()\
    );


// CControllerGUIDlg dialog

namespace
{

    std::recursive_timed_mutex guard;

    using packet = struct
    {
        int adc1, adc2, curr_err, int_err, pwm, ocr2;
    };

    enum draw_what { DRAW_NO, DRAW_ADC1, DRAW_ADC2, DRAW_CURR_ERR, DRAW_INT_ERR, DRAW_PWM, DRAW_OCR2 };

    std::list<packet> packets;
    sampled_t adc1_sampled = allocate_sampled(1000, 0.5);
    sampled_t adc2_sampled = allocate_sampled(1000, 0.5);
    sampled_t cur_err_sampled = allocate_sampled(1000, 0.5);
    sampled_t int_err_sampled = allocate_sampled(1000, 0.5);
    sampled_t pwm_sampled = allocate_sampled(1000, 0.5);
    sampled_t ocr2_sampled = allocate_sampled(1000, 0.5);

    simple_list_plot
        adc1_plot    = simple_list_plot::curve(0, 1),
        adc2_plot    = simple_list_plot::curve(0, 1),
        cur_err_plot = simple_list_plot::curve(0, 1),
        int_err_plot = simple_list_plot::curve(0, 1),
        pwm_plot     = simple_list_plot::curve(0, 1),
        ocr2_plot    = simple_list_plot::curve(0, 1)
    ;
    bool working = false;
    bool stopping = false;
    std::thread *worker = nullptr;
    HANDLE com = INVALID_HANDLE_VALUE;
    CString log_str;

    void calc_closest_to_desired(double desired, double &optimal, int &base, int &exponent)
    {
        double x0 = (int) desired, x;
        int y, y0 = x0;
        size_t i0 = 0;
        for (size_t i = 1; i < 15; i++)
        {
            y = (int) ((1 << i) * desired);
            x = ((double) y) / (1 << i);
            if ((abs(x - desired) < abs(x0 - desired)) && (y < 65535))
            {
                x0 = x;
                y0 = y;
                i0 = i;
            }
        }
        optimal = x0;
        base = y0;
        exponent = i0;
    }

}

CControllerGUIDlg::CControllerGUIDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CControllerGUIDlg::IDD, pParent)
    , m_comName(_T("COM2"))
    , m_desiredKp(0)
    , m_desiredKi(0)
    , m_desiredKs(0)
    , m_log(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    PLOT(m_adc1_plot_ctrl, adc1_plot);
    PLOT(m_adc2_plot_ctrl, adc2_plot);
    PLOT(m_cur_err_plot_ctrl, cur_err_plot);
    PLOT(m_int_err_plot_ctrl, int_err_plot);
    PLOT(m_pwm_plot_ctrl, pwm_plot);
    PLOT(m_ocr2_plot_ctrl, ocr2_plot);
}

void CControllerGUIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT1, m_comName);
    DDX_Text(pDX, IDC_EDIT3, m_desiredKp);
    DDX_Text(pDX, IDC_EDIT5, m_desiredKi);
    DDX_Text(pDX, IDC_EDIT7, m_desiredKs);
    DDX_Control(pDX, IDC_PLOT, m_adc1_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT2, m_adc2_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT4, m_cur_err_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT3, m_int_err_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT5, m_pwm_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT6, m_ocr2_plot_ctrl);
    DDX_Text(pDX, IDC_EDIT2, m_log);
    DDX_Control(pDX, IDC_EDIT2, m_logEdit);
}

BEGIN_MESSAGE_MAP(CControllerGUIDlg, CDialogEx)
    ON_MESSAGE(WM_UPDATEDATA_MESSAGE, &CControllerGUIDlg::OnUpdateDataMessage)
    ON_MESSAGE(WM_LOG_MESSAGE, &CControllerGUIDlg::OnLogMessage)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON1, &CControllerGUIDlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CControllerGUIDlg::OnBnClickedButton2)
    ON_BN_CLICKED(IDC_BUTTON10, &CControllerGUIDlg::OnBnClickedButton10)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDOK, &CControllerGUIDlg::OnBnClickedOk)
    ON_WM_CLOSE()
END_MESSAGE_MAP()


// CControllerGUIDlg message handlers

BOOL CControllerGUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

    // Just start the timer
    SetTimer(123456, 500, NULL);

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CControllerGUIDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CControllerGUIDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CControllerGUIDlg::OnBnClickedButton1()
{
    {
        LOCK();
        if (stopping) return;
    }

    CloseConnection();

    {
        LOCK();

        com = CreateFile(
            m_comName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
        if (com == INVALID_HANDLE_VALUE)
        {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                LOG("serial port does not exist");
            }
            else
            {
                LOG("error occurred");
            }
            return;
        }

        SetCommMask(com, EV_RXCHAR);
        SetupComm(com, 1500, 1500);

        COMMTIMEOUTS CommTimeOuts;
        CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
        CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
        CommTimeOuts.ReadTotalTimeoutConstant = 500;
        CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
        CommTimeOuts.WriteTotalTimeoutConstant = 1000;

        if (!SetCommTimeouts(com, &CommTimeOuts))
        {
            ERR(com, "cannot setup port timeouts");
            return;
        }

        DCB ComDCM;

        memset(&ComDCM, 0, sizeof(ComDCM));
        ComDCM.DCBlength = sizeof(DCB);
        GetCommState(com, &ComDCM);
        ComDCM.BaudRate = DWORD(4800);
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

        if (!SetCommState(com, &ComDCM))
        {
            ERR(com, "cannot setup port configuration");
            return;
        }

        LOG("successfully connected to the desired port");

        delete worker;
        worker = new std::thread(&CControllerGUIDlg::DoReading, this);
    }
}


void CControllerGUIDlg::OnBnClickedButton2()
{
    double kp_x0, ki_x0, ks_x0;
    int kp_y0, ki_y0, ks_y0;
    int kp_i0, ki_i0, ks_i0;

    calc_closest_to_desired(m_desiredKp, kp_x0, kp_y0, kp_i0);
    calc_closest_to_desired(m_desiredKi, ki_x0, ki_y0, ki_i0);
    calc_closest_to_desired(m_desiredKs, ks_x0, ks_y0, ks_i0);

    CString str; str.Format(_T("(kp, ki, ks) -> (%lf [%d, %d], %lf [%d, %d], %lf [%d, %d])"),
                            kp_x0, kp_y0, kp_i0, ki_x0, ki_y0, ki_i0, ks_x0, ks_y0, ks_i0);
    RequestLog(str);

    /*
     * <SET COEFS (int) (byte) (int) (byte) (int) (byte)>
     */
    char buffer[11] = {
        1 /* SET */,           5 /* COEFS */,
        ((kp_y0 >> 8) & 0xff), (kp_y0 & 0xff), kp_i0,
        ((ki_y0 >> 8) & 0xff), (ki_y0 & 0xff), ki_i0,
        ((ks_y0 >> 8) & 0xff), (ks_y0 & 0xff), ks_i0
    };

    int to_write = 11;
    int pos = 0;
    DWORD written;

    LOG("sending coefficients");
    do
    {
        {
            LOCK();
            if (!WriteFile(com, buffer + pos, to_write, &written, NULL))
            {
                ERR(com, "error while sending the data");
                working = false;
                return;
            }
            if (!working) break;
        }
        to_write -= written;
        pos += written;
    } while (to_write != 0);

    LOG("coefficients sent");
}

void CControllerGUIDlg::DoReading()
{
    {
        LOCK();
        working = true;
    }
    
    /*
     * <GET PACKET>
     */
    char buffer[2] = {
        0 /* GET */, 7 /* PACKET */
    };

    int to_write = 2;
    int pos = 0;
    DWORD written;

    LOG("sending request of data packet output mode");
    do
    {
        {
            LOCK();
            if (!WriteFile(com, buffer + pos, to_write, &written, NULL))
            {
                ERR(com, "error while sending the data");
                working = false;
                return;
            }
            if (!working) break;
        }
        to_write -= written;
        pos += written;
    } while (to_write != 0);

    LOG("request sent");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    /*
     * Use packet format:
     *     packet: < 0 adc1 adc2 cerr interr pwm ocr2 >
     *     bytes:  < 1    1    1    2      2   2    1 >   =>   10 bytes
     */

    const int packet_size = 10;
    const int packets_for_detection = 3;

    bool detected = false;

    while (true)
    {
        {
            LOCK();
            if (!working) break;
        }

        /*
         * Try to detect the packet start
         */
        while (!detected)
        {
            {
                LOCK();
                if (!working) break;
            }
            LOG("detecting packet start");
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
            char buffer[packets_for_detection * packet_size], buffer1[packet_size];
            int to_read = packets_for_detection * packet_size;
            int pos = 0;
            DWORD read;

            do
            {
                {
                    LOCK();
                    if (!ReadFile(com, buffer + pos, to_read, &read, NULL))
                    {
                        ERR(com, "error while reading the data");
                        working = false;
                        return;
                    }
                    if (!working) break;
                }
                to_read -= read;
                pos += read;
            } while (to_read != 0);

            size_t i;
            for (i = 0; (i < packet_size) && !detected; i++)
            {
                detected = true;
                for (size_t j = 0; (j < packets_for_detection) && detected; j++)
                {
                    detected &= (buffer[i + j * packet_size] == 0);
                }
            }
            if (!detected)
            {
                LOG("cannot detect packet start... sleep for 1 second and try again...");
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
                continue;
            }
            LOG("packet start detected");
            --i;
            if ((i % packet_size) != 0) // need to receive some pending bytes
            {
                LOG("receiving pending bytes if any");
                to_read = packet_size - (i % packet_size);
                pos = 0;
                do
                {
                    {
                        LOCK();
                        if (!ReadFile(com, buffer1, to_read, &read, NULL))
                        {
                            ERR(com, "error while reading the data");
                            working = false;
                            return;
                        }
                        if (!working) break;
                    }
                    to_read -= read;
                    pos += read;
                } while (to_read != 0);
            }
            LOG("packet start detection complete");
            LOG("receiving the data");

            {
                LOCK();
                if (!working) break;
            }
        }
        /*
         * Receive and unpack the packet
         */
        char buffer[packet_size];
        int to_read = packet_size;
        int pos = 0;
        DWORD read;
        do
        {
            {
                LOCK();
                if (!ReadFile(com, buffer + pos, to_read, &read, NULL))
                {
                    ERR(com, "error while reading the data");
                    working = false;
                    return;
                }
                if (!working) break;
            }
            to_read -= read;
            pos += read;
        } while (to_read != 0);
        if (buffer[0] != 0)
        {
            LOG("incorrect packet format detected... re-detecting packet start...");
            detected = false;
            continue;
        }
        {
            LOCK();
            packet p = { 
                (int) ((unsigned char) buffer[1]),
                (int) ((unsigned char) buffer[2]),
                (short) ((((unsigned char) buffer[3]) << 8) | ((unsigned char) buffer[4])),
                (short) ((((unsigned char) buffer[5]) << 8) | ((unsigned char) buffer[6])),
                (short) ((((unsigned char) buffer[7]) << 8) | ((unsigned char) buffer[8])),
                (int) ((unsigned char) buffer[9])
            };
            packets.push_back(p);
            if (packets.size() > 1000) packets.pop_front();
        }
    }
    {
        LOCK();
        if (com != INVALID_HANDLE_VALUE)
        {
            CloseHandle(com);
            com = INVALID_HANDLE_VALUE;
            LOG("port closed");
        }
    }
    LOG("the current session successfully closed");
}


void CControllerGUIDlg::OnBnClickedButton10()
{
    UpdateData(TRUE);
    double kp_x0, ki_x0, ks_x0;
    int kp_y0, ki_y0, ks_y0;
    int kp_i0, ki_i0, ks_i0;

    calc_closest_to_desired(m_desiredKp, kp_x0, kp_y0, kp_i0);
    calc_closest_to_desired(m_desiredKi, ki_x0, ki_y0, ki_i0);
    calc_closest_to_desired(m_desiredKs, ks_x0, ks_y0, ks_i0);
    
    CString str; str.Format(_T("(kp, ki, ks) -> (%lf [%d, %d], %lf [%d, %d], %lf [%d, %d])"),
                            kp_x0, kp_y0, kp_i0, ki_x0, ki_y0, ki_i0, ks_x0, ks_y0, ks_i0);
    RequestLog(str);
}


LRESULT CControllerGUIDlg::OnUpdateDataMessage(WPARAM wpD, LPARAM lpD)
{
    UpdateData(wpD == TRUE);
    return 0;
}

LRESULT CControllerGUIDlg::OnLogMessage(WPARAM wpD, LPARAM lpD)
{
    if (guard.try_lock()) {
        m_log = log_str;
        guard.unlock();
    }
    UpdateData(FALSE);
    return 0;
}


void CControllerGUIDlg::RequestUpdateData(BOOL saveAndValidate)
{
    SendMessage(WM_UPDATEDATA_MESSAGE, saveAndValidate);
}

void CControllerGUIDlg::RequestLog(LPCTSTR text)
{
    static int line = 0;
    {
        LOCK();
        CString line_f; line_f.Format(_T("%.3d: "), line);
        log_str = line_f + CString(text) + "\r\n" + log_str;
    }
    ++line;
}

void CControllerGUIDlg::OnTimer(UINT_PTR nIDEvent)
{
    size_t size;
    {
        if (!guard.try_lock_for(std::chrono::milliseconds(100))) return;

        size = packets.size();

        size_t i = 0;
        for each (auto o in packets)
        {
            adc1_sampled.samples[i] = o.adc1;
            adc2_sampled.samples[i] = o.adc2;
            cur_err_sampled.samples[i] = o.curr_err;
            int_err_sampled.samples[i] = o.int_err;
            pwm_sampled.samples[i] = o.pwm;
            ocr2_sampled.samples[i] = o.ocr2;
            ++i;
        }
        
        m_log = log_str;

        guard.unlock();
    }

    setup(adc1_plot, adc1_sampled, size);
    setup(adc2_plot, adc2_sampled, size);
    setup(cur_err_plot, cur_err_sampled, size);
    setup(int_err_plot, int_err_sampled, size);
    setup(pwm_plot, pwm_sampled, size);
    setup(ocr2_plot, ocr2_sampled, size);
    m_adc1_plot_ctrl.RedrawWindow();
    m_adc2_plot_ctrl.RedrawWindow();
    m_cur_err_plot_ctrl.RedrawWindow();
    m_int_err_plot_ctrl.RedrawWindow();
    m_pwm_plot_ctrl.RedrawWindow();
    m_ocr2_plot_ctrl.RedrawWindow();

    m_logEdit.SetWindowText(m_log);
}


void CControllerGUIDlg::OnBnClickedOk()
{
    KillTimer(123456);

    CloseConnection();

    CDialogEx::OnOK();
}


void CControllerGUIDlg::OnClose()
{
    KillTimer(123456);
    
    CloseConnection();

    CDialogEx::OnClose();
}


void CControllerGUIDlg::CloseConnection()
{
    LOG("closing existing session if any");
    std::thread *current;
    {
        LOCK();
        working = false;
        stopping = true;
        current = worker;
    }
    if (current)
    {
        current->join();
    }
    {
        LOCK();
        stopping = false;
        LOG("closed");
        packets.clear();
    }
}
