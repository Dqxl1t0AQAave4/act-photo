
// controller-guiDlg.cpp : implementation file
//

#include "stdafx.h"
#include "controller-gui.h"
#include "controller-guiDlg.h"
#include "afxdialogex.h"

#include "common.h"
#include "plot.h"
#include <act-common/logger.h>
#include "act-photo.h"
#include <act-common/reactor.h>

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

    std::size_t point_count = 500;

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

    world_t byte_world = world_t(0, point_count, -20, 280);
    world_t short_world = world_t(0, point_count, -35000, 35000);
}

CControllerGUIDlg::CControllerGUIDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CControllerGUIDlg::IDD, pParent)
    , m_log(_T(""))
    , m_textual(_T(""))
    , manager(this, _worker, point_count)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    PLOT(m_adc1_plot_ctrl, adc1_plot, &byte_world);
    PLOT(m_adc2_plot_ctrl, adc2_plot, &byte_world);
    PLOT(m_cur_err_plot_ctrl, cur_err_plot, &short_world);
    PLOT(m_int_err_plot_ctrl, int_err_plot, &short_world);
    PLOT(m_pwm_plot_ctrl, pwm_plot, &short_world);
    PLOT(m_ocr2_plot_ctrl, ocr2_plot, &byte_world);

    logger::set([this] (CString s) {
        RequestLog(s);
    });
}

void CControllerGUIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PLOT, m_adc1_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT2, m_adc2_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT4, m_cur_err_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT3, m_int_err_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT5, m_pwm_plot_ctrl);
    DDX_Control(pDX, IDC_PLOT6, m_ocr2_plot_ctrl);
    DDX_Control(pDX, IDC_EDIT2, m_logEdit);
    DDX_Control(pDX, IDC_EDIT3, m_textualEdit);
}

BEGIN_MESSAGE_MAP(CControllerGUIDlg, CDialogEx)
    ON_MESSAGE(WM_UPDATEDATA_MESSAGE, &CControllerGUIDlg::OnUpdateDataMessage)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_SHOWWINDOW()
    ON_WM_DESTROY()
END_MESSAGE_MAP()


// CControllerGUIDlg message handlers

BOOL CControllerGUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

    UpdateData(FALSE);
    UpdateData(TRUE);

    manager.Create(CManagementDlg::IDD, this);

    // Just start the timer
    SetTimer(123456, 500, NULL);

    _worker.start();

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
        worker::guard_t guard(_worker.iqueue_mutex);
        packets.splice(packets.end(), _worker.iqueue);
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

    byte_world.xmax = point_count;
    short_world.xmax = point_count;
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
    
    CString fmt;
    CString textual("");
    for each (auto o in packets)
    {
        fmt.Format(_T("%10u %10u %10d %10d %10d %10u\r\n"),
                   o.adc1, o.adc2, o.cur_err, o.int_err, o.pwm, o.ocr2);
        textual = fmt + textual;
    }
    fmt.Format(_T("%10s %10s %10s %10s %10s %10s\r\n"),
               _T("adc1"), _T("adc2"), _T("cur_err"), _T("int_err"), _T("PWM"), _T("OCR2"));
    textual = fmt + textual;
    if (textual != m_textual)
    {
        m_textualEdit.SetWindowText(textual);
        m_textual = textual;
    }
}

void CControllerGUIDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialogEx::OnShowWindow(bShow, nStatus);

    CRect rect;
    manager.GetWindowRect(rect);
    int dx = rect.Width();
    int dy = rect.Height();
    GetWindowRect(rect);
    manager.SetWindowPos(&CWnd::wndNoTopMost, rect.right - dx - 10, rect.bottom - dy - 10, dx, dy, SWP_SHOWWINDOW);
    manager.ShowWindow(SW_SHOW);
    manager.UpdateWindow();
}


void CControllerGUIDlg::OnDestroy()
{
    CDialogEx::OnDestroy();

    KillTimer(123456);

    _worker.stop();
    _worker.join();
    logger::set([] (CString) {});
}
