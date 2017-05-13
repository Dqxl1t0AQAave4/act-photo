
// controller-guiDlg.h : header file
//

#pragma once
#include "afxcmn.h"

#include "PlotStatic.h"
#include "afxwin.h"


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
    CString m_comName;
    afx_msg void OnBnClickedButton1();
    afx_msg void OnBnClickedButton2();
    double m_desiredKp;
    double m_desiredKi;
    double m_desiredKs;
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
    afx_msg void OnBnClickedButton10();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedOk();
    afx_msg void OnClose();
    void CloseConnection();
    CEdit m_logEdit;
};
