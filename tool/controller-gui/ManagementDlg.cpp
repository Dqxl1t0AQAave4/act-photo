// ManagementDlg.cpp : implementation file
//

#include "stdafx.h"
#include "controller-gui.h"
#include "ManagementDlg.h"
#include "afxdialogex.h"


// CManagementDlg dialog

IMPLEMENT_DYNAMIC(CManagementDlg, CDialogEx)

CManagementDlg::CManagementDlg(CWnd* pParent, worker &_worker, std::size_t &m_dataPointCount)
	: CDialogEx(CManagementDlg::IDD, pParent)
    , m_comName(_T("COM2"))
    , m_desired({})
    , _worker(_worker)
    , m_dataPointCount(m_dataPointCount)
    , m_bufferCap(5000)
    , m_packetQueueLength(1000)
{
}

CManagementDlg::~CManagementDlg()
{
}

void CManagementDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_CONN_COM_NAME, m_comName);
    DDX_Text(pDX, IDC_COEFS_KP, m_desired.kp);
    DDX_Text(pDX, IDC_COEFS_KI, m_desired.ki);
    DDX_Text(pDX, IDC_COEFS_KS, m_desired.ks);
    DDX_Control(pDX, IDC_COEFS_LAST, m_memoryCombo);
    DDX_Text(pDX, IDC_BUF_SIZE, m_bufferCap);
    DDX_Text(pDX, IDC_QUEUE_LENGTH, m_packetQueueLength);
    DDX_Text(pDX, IDC_DATA_POINT_COUNT, m_dataPointCount);
}


BEGIN_MESSAGE_MAP(CManagementDlg, CDialogEx)
    ON_BN_CLICKED(IDC_CONN_OPEN, &CManagementDlg::OnBnClickedConnOpen)
    ON_BN_CLICKED(IDC_CONN_CLOSE, &CManagementDlg::OnBnClickedConnClose)
    ON_BN_CLICKED(IDC_COEFS_SET, &CManagementDlg::OnBnClickedCoefsSet)
    ON_CBN_SELCHANGE(IDC_COEFS_LAST, &CManagementDlg::OnCbnSelchangeCoefsLast)
    ON_BN_CLICKED(IDC_SET_IO_PARAMS, &CManagementDlg::OnBnClickedSetIoParams)
END_MESSAGE_MAP()


// CManagementDlg message handlers


void CManagementDlg::OnBnClickedConnOpen()
{
    UpdateData(TRUE);

    com_port port;
    if (!port.open(com_port_options(m_comName, CBR_4800, 8, true, ODDPARITY, ONESTOPBIT)))
    {
        // port must log it
        return;
    }

    _worker.supply_port(std::move(port));
}


void CManagementDlg::OnBnClickedConnClose()
{
    UpdateData(TRUE);

    _worker.supply_port(com_port());
}


void CManagementDlg::OnBnClickedCoefsSet()
{
    UpdateData(TRUE);

    act_photo::desired_coefs_t optimal;
    act_photo::coefs_t coefs = act_photo::calculate_optimal_coefs(m_desired, optimal);

    logger::log(_T("sending coefs [kp = %.2e, ki = %.2f, ks = %.2f]"), optimal.kp, optimal.ki, optimal.ks);
    logger::log(_T("kp => [%d, %d, %d], ki => [%d, %d, %d], ks => [%d, %d, %d]"),
                (coefs.kp_m >> 8) & 0xff, coefs.kp_m & 0xff, coefs.kp_d,
                (coefs.ki_m >> 8) & 0xff, coefs.ki_m & 0xff, coefs.ki_d,
                (coefs.ks_m >> 8) & 0xff, coefs.ks_m & 0xff, coefs.ks_d);

    std::size_t existing = m_memory.size();
    for (std::size_t i = 0; i < m_memory.size(); ++i)
    {
        act_photo::desired_coefs_t o = m_memory[i];
        if ((o.kp == m_desired.kp) && (o.ki == m_desired.ki) && (o.ks == m_desired.ks))
        {
            existing = i;
            break;
        }
    }

    if (existing == m_memory.size())
    {
        m_memory.push_back(m_desired);

        CString str; str.Format(_T("%.3f %.3f %.3f"), m_desired.kp, m_desired.ki, m_desired.ks);
        m_memoryCombo.InsertString(0, str);
        m_memoryCombo.SetCurSel(0);
    }
    else
    {
        m_memoryCombo.SetCurSel(m_memory.size() - 1 - existing);
    }


    _worker.supply_opacket(act_photo::set_coefs_command(coefs));
}


void CManagementDlg::OnCbnSelchangeCoefsLast()
{
    m_desired = m_memory[m_memory.size() - 1 - m_memoryCombo.GetCurSel()];
    UpdateData(FALSE);
}


void CManagementDlg::OnBnClickedSetIoParams()
{
    UpdateData(TRUE);
    _worker.supply_ibuffer_size(m_bufferCap);
    _worker.supply_iqueue_length(m_packetQueueLength);
}
