#include "pch.h"
#include "framework.h"
#include "GeoAnonymizer.h"
#include "GeoAnonymizerDlg.h"
#include "afxdialogex.h"
#include <fstream>
#include <sstream>
#include <regex>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CGeoAnonymizerDlg::CGeoAnonymizerDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_GEOANONYMIZER_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CGeoAnonymizerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_SOURCE, m_editSource);
    DDX_Control(pDX, IDC_LIST_COORDS, m_listCoords);
}

BEGIN_MESSAGE_MAP(CGeoAnonymizerDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OPEN, &CGeoAnonymizerDlg::OnBnClickedBtnOpen)
    ON_BN_CLICKED(IDC_BTN_SHOW_COORDS, &CGeoAnonymizerDlg::OnBnClickedBtnShowCoords)
    ON_BN_CLICKED(IDC_BTN_ANONYMIZE, &CGeoAnonymizerDlg::OnBnClickedBtnAnonymize)
END_MESSAGE_MAP()

// =============================================
// Основная функция поиска координат
// =============================================
bool CGeoAnonymizerDlg::FindCoordinates(const std::string& text, std::vector<std::string>& coords)
{
    coords.clear();

    // 1. Decimal формат (55.7539, 37.6208)
    {
        std::regex re(R"(([-+]?\d{1,3}\.\d{4,})\s*,\s*([-+]?\d{1,3}\.\d{4,}))");
        std::sregex_iterator it(text.begin(), text.end(), re);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            std::string match = it->str();
            if (std::find(coords.begin(), coords.end(), match) == coords.end())
                coords.push_back(match);
        }
    }

    // 2. Всё, что содержит символ ° — захватываем максимально широко
    size_t pos = 0;
    while ((pos = text.find('°', pos)) != std::string::npos)
    {
        // Берём большой кусок вокруг символа °
        size_t start = (pos > 50) ? pos - 50 : 0;
        std::string chunk = text.substr(start, 100);

        // Обрезаем по ближайшей точке или новой строке
        size_t end1 = chunk.find('.');
        size_t end2 = chunk.find('\n');
        size_t end = (end1 < end2) ? end1 : end2;

        if (end != std::string::npos)
            chunk = chunk.substr(0, end);

        // Если в куске есть ° — добавляем как есть
        if (chunk.find('°') != std::string::npos)
        {
            if (std::find(coords.begin(), coords.end(), chunk) == coords.end())
                coords.push_back(chunk);
        }
        pos++;
    }

    return !coords.empty();
}

// Замена координат на ***
std::string CGeoAnonymizerDlg::AnonymizeText(const std::string& text)
{
    std::string result = text;

    std::vector<std::string> dummy;
    FindCoordinates(text, dummy); // чтобы найти все

    // Заменяем через regex (можно улучшить)
    result = std::regex_replace(result, std::regex(R"((?:^|\s)([-+]?(?:90(?:\.0+)?|[0-8]?\d(?:\.\d+)?)),\s*([-+]?(?:180(?:\.0+)?|1[0-7]\d(?:\.\d+)?|[0-9]{1,2}(?:\.\d+)?))(?=\s|$|[^\w.]))"), " *** ");
    result = std::regex_replace(result, std::regex(R"((?:^|\s)([NS]?\s*\d{1,3}°\s*\d{1,2}['′]\s*\d{1,2}(?:\.\d+)?["″]?\s*[NS]?)[\s,]+([EW]?\s*\d{1,3}°\s*\d{1,2}['′]\s*\d{1,2}(?:\.\d+)?["″]?\s*[EW]?))"), " *** ");
    result = std::regex_replace(result, std::regex(R"((?:^|\s)([-+]?\d+\.\d{4,}),\s*([-+]?\d+\.\d{4,})(?=\s|$))"), " *** ");

    return result;
}

std::string CGeoAnonymizerDlg::GetFilenameFromPath(const CString& path)
{
    if (path.IsEmpty())
        return "";

    // Получаем только имя файла из полного пути
    int nPos = path.ReverseFind(_T('\\'));
    if (nPos == -1)
        nPos = path.ReverseFind(_T('/'));

    if (nPos != -1)
        return CT2A(path.Mid(nPos + 1));   // CString → std::string
    else
        return CT2A(path);                 // если пути нет
}

// =============================================
// Обработчики кнопок
// =============================================

void CGeoAnonymizerDlg::OnBnClickedBtnOpen()
{
    CFileDialog dlg(TRUE, _T("txt"), nullptr,
        OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
        _T("Текстовые файлы (*.txt;*.log;*.md;*.csv)|*.txt;*.log;*.md;*.csv|Все файлы (*.*)|*.*||"), this);

    if (dlg.DoModal() == IDOK)
    {
        m_strSourcePath = dlg.GetPathName();
        m_editSource.SetWindowText(m_strSourcePath);

        // === ИСПРАВЛЕННЫЙ КОД ===
        std::string filename = GetFilenameFromPath(m_strSourcePath);

        // Преобразуем std::string → CString правильно
        CString filenameCString = CA2T(filename.c_str());

        // Проверка координат в имени файла
        std::vector<std::string> coordsInName;
        if (FindCoordinates(filename, coordsInName))
        {
            AfxMessageBox(_T("В имени файла обнаружены координаты!\nОни тоже будут заменены."));
        }
    }
}

void CGeoAnonymizerDlg::OnBnClickedBtnShowCoords()
{
    if (m_strSourcePath.IsEmpty())
    {
        AfxMessageBox(_T("Сначала откройте файл!"));
        return;
    }

    m_listCoords.ResetContent();
    m_foundCoords.clear();

    // Читаем файл
    std::ifstream file(m_strSourcePath, std::ios::binary | std::ios::in);
    if (!file.is_open())
    {
        AfxMessageBox(_T("Не удалось открыть файл!"));
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Ищем в содержимом + в имени файла
    std::string filename = GetFilenameFromPath(m_strSourcePath);
    FindCoordinates(filename, m_foundCoords);
    FindCoordinates(content, m_foundCoords);

    if (m_foundCoords.empty())
    {
        AfxMessageBox(_T("Координаты не найдены."));
        return;
    }

    for (const auto& coord : m_foundCoords)
    {
        m_listCoords.AddString(CA2T(coord.c_str()));
    }

    AfxMessageBox(_T("Найденные координаты показаны в списке."));
}

void CGeoAnonymizerDlg::OnBnClickedBtnAnonymize()
{
    if (m_strSourcePath.IsEmpty())
    {
        AfxMessageBox(_T("Откройте файл сначала!"));
        return;
    }

    // Получаем имя файла без расширения
    std::string origName = GetFilenameFromPath(m_strSourcePath);

    // Убираем расширение
    size_t dotPos = origName.find_last_of('.');
    if (dotPos != std::string::npos)
        origName = origName.substr(0, dotPos);

    // === ИСПРАВЛЕННОЕ СОЗДАНИЕ ИМЕНИ ФАЙЛА ===
    CString defaultName = _T("anonymized_");
    defaultName += origName.c_str();        // ← безопасный способ

    // Диалог сохранения
    CFileDialog saveDlg(FALSE,
        _T("txt"),
        defaultName,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("Текстовые файлы (*.txt)|*.txt|Все файлы (*.*)|*.*||"),
        this);

    if (saveDlg.DoModal() != IDOK)
        return;

    m_strResultPath = saveDlg.GetPathName();

    // Чтение файла
    std::ifstream in(m_strSourcePath);   // CString напрямую
    if (!in.is_open())
    {
        AfxMessageBox(_T("Ошибка чтения файла!"));
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();
    in.close();

    // Анонимизация
    std::string anonymized = AnonymizeText(content);

    // Сохранение
    std::ofstream out(m_strResultPath);
    if (!out.is_open())
    {
        AfxMessageBox(_T("Ошибка записи файла!"));
        return;
    }

    out << anonymized;
    out.close();

    CString msg;
    msg.Format(_T("Файл успешно сохранён!\n\n%s"), m_strResultPath.GetString());
    AfxMessageBox(msg);

    SetDlgItemText(IDC_EDIT_RESULT, m_strResultPath);
}