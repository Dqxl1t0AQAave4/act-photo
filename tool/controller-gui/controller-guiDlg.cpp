
// controller-guiDlg.cpp : implementation file
//

#include "stdafx.h"
#include "controller-gui.h"
#include "controller-guiDlg.h"
#include "afxdialogex.h"

#include "common.h"
#include "plot.h"
#include "logger.h"
#include "act-photo.h"
#include "worker.h"

#include <list>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_UPDATEDATA_MESSAGE (WM_USER + 1000)

using namespace common;

#define PLOT(ctrl, p, world)\
    ctrl.plot_layer.with(\
        (plot::plot_builder() << p)\
        .with_ticks(plot::palette::pen(RGB(150, 150, 0)))\
        .with_x_ticks(0, 3, 0)\
        .with_y_ticks(0, 10, 0)\
        .in_world(world)\
        .build()\
    );


// CControllerGUIDlg dialog

namespace
{

    const std::size_t point_count = 500;

    std::list<act_photo::packet_t> packets;

    sampled_t adc1_sampled = allocate_sampled(point_count, 1);
    sampled_t adc2_sampled = allocate_sampled(point_count, 1);
    sampled_t cur_err_sampled = allocate_sampled(point_count, 1);
    sampled_t int_err_sampled = allocate_sampled(point_count, 1);
    sampled_t pwm_sampled = allocate_sampled(point_count, 1);
    sampled_t ocr2_sampled = allocate_sampled(point_count, 1);

    simple_list_plot
        adc1_plot    = simple_list_plot::curve(0, 2),
        adc2_plot    = simple_list_plot::curve(0, 2),
        cur_err_plot = simple_list_plot::curve(0, 2),
        int_err_plot = simple_list_plot::curve(0, 2),
        pwm_plot     = simple_list_plot::curve(0, 2),
        ocr2_plot    = simple_list_plot::curve(0, 2)
    ;

    CString    log_str; // guarded by `log_mutex`
    std::mutex log_mutex;

    worker     _worker;
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

    PLOT(m_adc1_plot_ctrl, adc1_plot, world_t(0, point_count, -20, 280));
    PLOT(m_adc2_plot_ctrl, adc2_plot, world_t(0, point_count, -20, 280));
    PLOT(m_cur_err_plot_ctrl, cur_err_plot, world_t(0, point_count, -35000, 35000));
    PLOT(m_int_err_plot_ctrl, int_err_plot, world_t(0, point_count, -35000, 35000));
    PLOT(m_pwm_plot_ctrl, pwm_plot, world_t(0, point_count, -35000, 35000));
    PLOT(m_ocr2_plot_ctrl, ocr2_plot, world_t(0, point_count, -20, 280));

    logger::set([this] (CString s) {
        RequestLog(s);
    });
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

    UpdateData(TRUE);
    UpdateData(FALSE);

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
    UpdateData(TRUE);

    com_port port;
    if (!port.open(m_comName))
    {
        // port must log it
        return;
    }

    _worker.supply_port(std::move(port));
}


void CControllerGUIDlg::OnBnClickedButton2()
{
    OnBnClickedButton10();

    _worker.supply_command(act_photo::set_coefs_command(act_photo::desired_coefs_t{ m_desiredKp, m_desiredKi, m_desiredKs }));
}


void CControllerGUIDlg::OnBnClickedButton10()
{
    UpdateData(TRUE);

    act_photo::desired_coefs_t desired = { m_desiredKp, m_desiredKi, m_desiredKs }, optimal;
    act_photo::coefs_t coefs = act_photo::calculate_optimal_coefs(desired, optimal);

    logger::log(_T("sending coefs [kp = %.2e, ki = %.2f, ks = %.2f]"), optimal.kp, optimal.ki, optimal.ks);
    logger::log(_T("kp => [%d, %d, %d], ki => [%d, %d, %d], ks => [%d, %d, %d]"),
                (coefs.kp_m >> 8) & 0xff, coefs.kp_m & 0xff, coefs.kp_d,
                (coefs.ki_m >> 8) & 0xff, coefs.ki_m & 0xff, coefs.ki_d,
                (coefs.ks_m >> 8) & 0xff, coefs.ks_m & 0xff, coefs.ks_d);
}


LRESULT CControllerGUIDlg::OnUpdateDataMessage(WPARAM wpD, LPARAM lpD)
{
    UpdateData(wpD == TRUE);
    return 0;
}


void CControllerGUIDlg::RequestUpdateData(BOOL saveAndValidate)
{
    SendMessage(WM_UPDATEDATA_MESSAGE, saveAndValidate);
}

void CControllerGUIDlg::RequestLog(CString text)
{
    static int line = 0;
    {
        std::lock_guard<decltype(log_mutex)> lock(log_mutex);
        CString line_f; line_f.Format(_T("%.3d: "), line);
        log_str = line_f + text + "\r\n" + log_str;
        ++line;
    }
}

void CControllerGUIDlg::OnTimer(UINT_PTR nIDEvent)
{
    {
        std::lock_guard<decltype(_worker.queue_mutex)> guard(_worker.queue_mutex);
        packets.insert(packets.end(), _worker.packet_queue.begin(), _worker.packet_queue.end());
        _worker.packet_queue.clear();
    }
    while(packets.size() > point_count)
    {
        packets.pop_front();
    }

    std::size_t i = 0;
    for each (auto o in packets)
    {
        adc1_sampled.samples[i] = o.adc1;
        adc2_sampled.samples[i] = o.adc2;
        cur_err_sampled.samples[i] = o.cur_err;
        int_err_sampled.samples[i] = o.int_err;
        pwm_sampled.samples[i] = o.pwm;
        ocr2_sampled.samples[i] = o.ocr2;
        ++i;
    }

    std::size_t size = packets.size();
    setup(adc1_plot, adc1_sampled, size, 0, identity_un_op(), false);
    setup(adc2_plot, adc2_sampled, size, 0, identity_un_op(), false);
    setup(cur_err_plot, cur_err_sampled, size, 0, identity_un_op(), false);
    setup(int_err_plot, int_err_sampled, size, 0, identity_un_op(), false);
    setup(pwm_plot, pwm_sampled, size, 0, identity_un_op(), false);
    setup(ocr2_plot, ocr2_sampled, size, 0, identity_un_op(), false);
    m_adc1_plot_ctrl.RedrawWindow();
    m_adc2_plot_ctrl.RedrawWindow();
    m_cur_err_plot_ctrl.RedrawWindow();
    m_int_err_plot_ctrl.RedrawWindow();
    m_pwm_plot_ctrl.RedrawWindow();
    m_ocr2_plot_ctrl.RedrawWindow();

    {
        std::lock_guard<decltype(log_mutex)> lock(log_mutex);
        m_log = log_str;
    }
    if (m_logEdit.GetWindowTextLength() != m_log.GetLength())
    {
        m_logEdit.SetWindowText(m_log);
    }
}


void CControllerGUIDlg::OnBnClickedOk()
{
    KillTimer(123456);

    _worker.stop();
    _worker.join();
    logger::set([] (CString) {});

    CDialogEx::OnOK();
}


void CControllerGUIDlg::OnClose()
{
    KillTimer(123456);
    
    _worker.stop();
    _worker.join();
    logger::set([] (CString) {});

    CDialogEx::OnClose();
}


void CControllerGUIDlg::CloseConnection()
{
}