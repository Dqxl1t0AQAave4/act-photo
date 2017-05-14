#pragma once
#include "afxwin.h"

#include "worker.h"
#include "logger.h"
#include "act-photo.h"
#include <vector>

// CManagementDlg dialog

class CManagementDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CManagementDlg)

public:
	CManagementDlg(CWnd* pParent, worker &_worker, std::size_t &m_dataPointCount);   // standard constructor
	virtual ~CManagementDlg();

// Dialog Data
	enum { IDD = IDD_MANAGEMENTDLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    std::vector<act_photo::desired_coefs_t> m_memory;
    worker &_worker;
    CString m_comName;
    afx_msg void OnBnClickedConnOpen();
    afx_msg void OnBnClickedConnClose();
    afx_msg void OnBnClickedCoefsSet();
    act_photo::desired_coefs_t m_desired;
    CComboBox m_memoryCombo;
    afx_msg void OnCbnSelchangeCoefsLast();
    afx_msg void OnBnClickedSetIoParams();
    std::size_t m_bufferCap;
    std::size_t m_packetQueueLength;
    std::size_t &m_dataPointCount;
};
