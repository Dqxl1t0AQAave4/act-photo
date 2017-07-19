
// controller-guiDlg.h : header file
//

#pragma once
#include "afxcmn.h"

#include "PlotStatic.h"
#include "afxwin.h"

#include "ManagementDlg.h"

#include <act-common/reactor.h>

using namespace com_port_api;
using worker = reactor < act_photo::dialect > ;


// CControllerGUIDlg dialog
class CControllerGUIDlg : public CDialogEx
{
// Construction
public:
	CControllerGUIDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_CONTROLLERGUI_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
    CManagementDlg manager;
    LRESULT OnUpdateDataMessage(WPARAM wpD, LPARAM lpD);
    void RequestUpdateData(BOOL saveAndValidate);
    void RequestLog(CString text);
    PlotStatic m_adc1_plot_ctrl;
    PlotStatic m_adc2_plot_ctrl;
    PlotStatic m_cur_err_plot_ctrl;
    PlotStatic m_int_err_plot_ctrl;
    PlotStatic m_pwm_plot_ctrl;
    PlotStatic m_ocr2_plot_ctrl;
    CString m_log;
    CString m_textual;
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    CEdit m_logEdit;
    CEdit m_textualEdit;
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnDestroy();
};
