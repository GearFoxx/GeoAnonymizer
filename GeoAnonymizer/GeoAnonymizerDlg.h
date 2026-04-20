#pragma once
#include <regex>
#include <vector>
#include <string>

class CGeoAnonymizerDlg : public CDialogEx
{
public:
    CGeoAnonymizerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_GEOANONYMIZER_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    HICON m_hIcon;

    DECLARE_MESSAGE_MAP()

public:
    CString m_strSourcePath;     // путь к исходному файлу
    CString m_strResultPath;     // путь, куда сохраняем

    CEdit m_editSource;
    CListBox m_listCoords;

    afx_msg void OnBnClickedBtnOpen();
    afx_msg void OnBnClickedBtnShowCoords();
    afx_msg void OnBnClickedBtnAnonymize();

private:
    std::vector<std::string> m_foundCoords; // найденные координаты (для показа)

    // Регулярные выражения для поиска координат
    bool FindCoordinates(const std::string& text, std::vector<std::string>& coords);
    std::string AnonymizeText(const std::string& text);
    std::string GetFilenameFromPath(const CString& path);
};