#include "code_stats_panel.h"

#include "IWorkspace.h"
#include "clWorkspaceManager.h"
#include "ieditor.h"
#include "imanager.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/settings.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <cmath>
#include <numeric>

wxDEFINE_EVENT(wxEVT_FKC_SCAN_PROGRESS, CodeStatsProgressEvent);
wxDEFINE_EVENT(wxEVT_FKC_SCAN_DONE, CodeStatsEvent);

namespace {

const wxColour kMutedText = wxColour(110, 110, 110);
const wxColour kDangerText = wxColour(198, 64, 54);
const wxColour kWarningText = wxColour(190, 118, 24);

double FileCommentRate(const fkcstats::FileStats& file)
{
    const int lines = file.codeLines + file.commentLines;
    return lines > 0 ? static_cast<double>(file.commentLines) * 100.0 / lines : 0.0;
}

int FileMarkerCount(const fkcstats::FileStats& file)
{
    return file.todoCount + file.fixmeCount;
}

int CompareInt(int left, int right)
{
    return (left < right) ? -1 : ((left > right) ? 1 : 0);
}

int CompareDouble(double left, double right)
{
    return (left < right) ? -1 : ((left > right) ? 1 : 0);
}

void InsertColumn(wxListCtrl* list, int index, const wxString& title, int width, int format = wxLIST_FORMAT_LEFT)
{
    list->InsertColumn(index, title, format, width);
}

} // namespace

CodeStatsPanel::CodeStatsPanel(wxWindow* parent, IManager* manager)
    : wxPanel(parent, wxID_ANY)
    , m_mgr(manager)
{
    BuildUI();

    Bind(wxEVT_FKC_SCAN_PROGRESS, &CodeStatsPanel::OnProgress, this);
    Bind(wxEVT_FKC_SCAN_DONE, &CodeStatsPanel::OnScanDone, this);

    m_scanButton->Bind(wxEVT_BUTTON, &CodeStatsPanel::OnScanClicked, this);
    m_cancelButton->Bind(wxEVT_BUTTON, &CodeStatsPanel::OnCancelClicked, this);
    m_scopeChoice->Bind(wxEVT_CHOICE, &CodeStatsPanel::OnScopeChanged, this);

    m_fileList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CodeStatsPanel::OnFileListActivated, this);
    m_fileList->Bind(wxEVT_LIST_COL_CLICK, &CodeStatsPanel::OnFileListColumnClick, this);
    m_topList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CodeStatsPanel::OnTopListActivated, this);
    m_todoList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CodeStatsPanel::OnTodoListActivated, this);

    SetScanning(false);
}

CodeStatsPanel::~CodeStatsPanel()
{
    m_cancel = true;
    JoinWorker();
    DeletePendingEvents();
}

void CodeStatsPanel::BuildUI()
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto* scopeLabel = new wxStaticText(this, wxID_ANY, wxT("分析范围:"));
    toolbar->Add(scopeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    m_scopeChoice = new wxChoice(this, wxID_ANY);
    m_scopeChoice->Append(wxT("整个工作区"));
    m_scopeChoice->Append(wxT("当前活动项目"));
    m_scopeChoice->Append(wxT("当前文件"));
    m_scopeChoice->SetSelection(0);
    m_scopeChoice->SetToolTip(wxT("选择代码统计的扫描范围"));
    toolbar->Add(m_scopeChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    const bool libclangAvailable = fkcstats::CodeStatsScanner::IsLibclangAvailable();
    m_clangCheck = new wxCheckBox(this, wxID_ANY, wxT("libclang AST"));
    m_clangCheck->SetValue(libclangAvailable);
    m_clangCheck->Enable(libclangAvailable);
    m_clangCheck->SetToolTip(libclangAvailable
                                 ? wxT("使用 libclang 解析 C/C++ AST，以获得更准确的圈复杂度")
                                 : wxT("未找到 libclang，将使用内置的词法分析引擎"));
    toolbar->Add(m_clangCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    toolbar->AddStretchSpacer(1);

    m_scanButton = new wxButton(this, wxID_ANY, wxT("开始扫描"));
    m_scanButton->SetToolTip(wxT("在后台线程扫描并统计代码质量"));
    toolbar->Add(m_scanButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

    m_cancelButton = new wxButton(this, wxID_ANY, wxT("取消"));
    toolbar->Add(m_cancelButton, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(toolbar, 0, wxEXPAND | wxALL, 6);

    auto* cards = new wxWrapSizer(wxHORIZONTAL);
    auto addCard = [&](const wxString& title, wxStaticText** value) {
        wxPanel* card = CreateCard(this, title, value);
        cards->Add(card, 0, wxALL, 4);
    };
    addCard(wxT("文件数"), &m_valFiles);
    addCard(wxT("代码行"), &m_valCode);
    addCard(wxT("注释行"), &m_valComment);
    addCard(wxT("注释率"), &m_valRate);
    addCard(wxT("圈复杂度"), &m_valComplexity);
    addCard(wxT("TODO / FIXME"), &m_valMarkers);
    mainSizer->Add(cards, 0, wxEXPAND | wxLEFT | wxRIGHT, 2);

    m_notebook = new wxNotebook(this, wxID_ANY);
    m_notebook->AddPage(BuildOverviewPage(), wxT("概览"), true);
    m_notebook->AddPage(BuildFilesPage(), wxT("文件明细"));
    m_notebook->AddPage(BuildMarkersPage(), wxT("TODO / FIXME"));
    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 6);

    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    m_statusLabel = new wxStaticText(
        this, wxID_ANY, wxT("就绪。点击「开始扫描」分析工作区代码质量。"), wxDefaultPosition, wxDefaultSize,
        wxST_ELLIPSIZE_MIDDLE);
    statusSizer->Add(m_statusLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    m_progressGauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(220, 12));
    m_progressGauge->SetValue(0);
    statusSizer->Add(m_progressGauge, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(statusSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(mainSizer);
    Layout();
}

wxPanel* CodeStatsPanel::CreateCard(wxWindow* parent, const wxString& title, wxStaticText** value)
{
    auto* card = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(118, 66), wxBORDER_THEME);
    card->SetMinSize(wxSize(112, 64));

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* titleLabel = new wxStaticText(card, wxID_ANY, title);
    titleLabel->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    sizer->Add(titleLabel, 0, wxLEFT | wxRIGHT | wxTOP, 8);

    auto* valueLabel = new wxStaticText(card, wxID_ANY, wxT("-"));
    wxFont valueFont = valueLabel->GetFont();
    valueFont.SetPointSize(valueFont.GetPointSize() + 6);
    valueFont.MakeBold();
    valueLabel->SetFont(valueFont);
    sizer->Add(valueLabel, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER_VERTICAL, 8);

    card->SetSizer(sizer);
    *value = valueLabel;
    return card;
}

void CodeStatsPanel::AddInfoRow(wxWindow* parent,
                                wxFlexGridSizer* grid,
                                const wxString& title,
                                wxStaticText** value)
{
    auto* titleLabel = new wxStaticText(parent, wxID_ANY, title);
    titleLabel->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    grid->Add(titleLabel, 0, wxALIGN_CENTER_VERTICAL);

    auto* valueLabel = new wxStaticText(parent, wxID_ANY, wxT("-"));
    grid->Add(valueLabel, 0, wxALIGN_CENTER_VERTICAL);
    *value = valueLabel;
}

wxPanel* CodeStatsPanel::BuildOverviewPage()
{
    auto* page = new wxPanel(m_notebook);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* gaugeSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* commentBox = new wxStaticBoxSizer(wxVERTICAL, page, wxT("注释率"));
    m_commentGauge = new wxGauge(commentBox->GetStaticBox(), wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 14));
    m_commentGauge->SetValue(0);
    m_commentGaugeLabel = new wxStaticText(commentBox->GetStaticBox(), wxID_ANY, wxT("-"));
    commentBox->Add(m_commentGauge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
    commentBox->Add(m_commentGaugeLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 6);
    gaugeSizer->Add(commentBox, 1, wxEXPAND | wxRIGHT, 8);

    auto* qualityBox = new wxStaticBoxSizer(wxVERTICAL, page, wxT("质量评分"));
    m_qualityGauge = new wxGauge(qualityBox->GetStaticBox(), wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 14));
    m_qualityGauge->SetValue(0);
    m_qualityGaugeLabel = new wxStaticText(qualityBox->GetStaticBox(), wxID_ANY, wxT("-"));
    qualityBox->Add(m_qualityGauge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
    qualityBox->Add(m_qualityGaugeLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 6);
    gaugeSizer->Add(qualityBox, 1, wxEXPAND);
    sizer->Add(gaugeSizer, 0, wxEXPAND | wxALL, 4);

    auto* infoBox = new wxStaticBoxSizer(wxVERTICAL, page, wxT("扫描信息"));
    auto* infoGrid = new wxFlexGridSizer(4, 6, 18);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("总行数"), &m_infoTotal);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("空白行"), &m_infoBlank);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("失败文件"), &m_infoFailed);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("分析引擎"), &m_infoEngine);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("扫描耗时"), &m_infoElapsed);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("扫描时间"), &m_infoTime);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("标记总数"), &m_infoMarkers);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("AST 文件"), &m_infoAst);
    AddInfoRow(infoBox->GetStaticBox(), infoGrid, wxT("统计范围"), &m_infoScope);
    infoBox->Add(infoGrid, 1, wxEXPAND | wxALL, 8);
    sizer->Add(infoBox, 0, wxEXPAND | wxALL, 4);

    auto* topBox = new wxStaticBoxSizer(wxVERTICAL, page, wxT("复杂度最高的文件"));
    m_topList = new wxListCtrl(
        topBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    InsertColumn(m_topList, 0, wxT("文件"), 300);
    InsertColumn(m_topList, 1, wxT("复杂度"), 70, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_topList, 2, wxT("代码行"), 70, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_topList, 3, wxT("注释率"), 80, wxLIST_FORMAT_RIGHT);
    topBox->Add(m_topList, 1, wxEXPAND | wxALL, 4);
    sizer->Add(topBox, 1, wxEXPAND | wxALL, 4);

    page->SetSizer(sizer);
    return page;
}

wxPanel* CodeStatsPanel::BuildFilesPage()
{
    auto* page = new wxPanel(m_notebook);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* hint = new wxStaticText(page, wxID_ANY, wxT("双击文件可在编辑器中打开；点击表头可排序。"));
    hint->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    sizer->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, 6);

    m_fileList = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    InsertColumn(m_fileList, 0, wxT("文件"), 300);
    InsertColumn(m_fileList, 1, wxT("语言"), 100);
    InsertColumn(m_fileList, 2, wxT("总行"), 60, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 3, wxT("代码"), 60, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 4, wxT("注释"), 60, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 5, wxT("空白"), 60, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 6, wxT("注释率"), 72, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 7, wxT("复杂度"), 68, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 8, wxT("TODO/FIXME"), 90, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_fileList, 9, wxT("AST"), 48);
    sizer->Add(m_fileList, 1, wxEXPAND | wxALL, 4);

    page->SetSizer(sizer);
    return page;
}

wxPanel* CodeStatsPanel::BuildMarkersPage()
{
    auto* page = new wxPanel(m_notebook);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* hint = new wxStaticText(page, wxID_ANY, wxT("双击标记可跳转到对应文件与行号。"));
    hint->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    sizer->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, 6);

    m_todoList = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    InsertColumn(m_todoList, 0, wxT("类型"), 64);
    InsertColumn(m_todoList, 1, wxT("文件"), 280);
    InsertColumn(m_todoList, 2, wxT("行"), 60, wxLIST_FORMAT_RIGHT);
    InsertColumn(m_todoList, 3, wxT("内容"), 460);
    sizer->Add(m_todoList, 1, wxEXPAND | wxALL, 4);

    page->SetSizer(sizer);
    return page;
}

CodeStatsPanel::Scope CodeStatsPanel::GetSelectedScope() const
{
    switch (m_scopeChoice->GetSelection()) {
    case 1:
        return Scope::ActiveProject;
    case 2:
        return Scope::CurrentFile;
    case 0:
    default:
        return Scope::Workspace;
    }
}

wxString CodeStatsPanel::ResolveRoot(Scope scope, wxString& scopeLabel, wxString& singleFile)
{
    singleFile.clear();
    IWorkspace* workspace = clWorkspaceManager::Get().GetWorkspace();

    if (scope == Scope::CurrentFile) {
        IEditor* editor = m_mgr ? m_mgr->GetActiveEditor() : nullptr;
        if (!editor) {
            scopeLabel = wxT("当前文件");
            return wxString();
        }
        singleFile = editor->GetFileName().GetFullPath();
        scopeLabel = wxString::Format(wxT("当前文件: %s"), editor->GetFileName().GetFullName());
        return editor->GetFileName().GetPath();
    }

    if (!workspace) {
        scopeLabel = (scope == Scope::Workspace) ? wxT("整个工作区") : wxT("当前活动项目");
        return wxString();
    }

    if (scope == Scope::Workspace) {
        scopeLabel = wxString::Format(wxT("整个工作区: %s"), workspace->GetName());
        if (workspace->IsRemote()) {
            return wxString();
        }
        return workspace->GetDir();
    }

    wxString projectName = workspace->GetActiveProjectName();
    if (projectName.empty()) {
        const wxArrayString projects = workspace->GetWorkspaceProjects();
        if (!projects.IsEmpty()) {
            projectName = projects[0];
        }
    }
    if (projectName.empty()) {
        scopeLabel = wxT("当前活动项目");
        return wxString();
    }

    scopeLabel = wxString::Format(wxT("项目: %s"), projectName);
    const wxFileName projectFile = workspace->GetProjectFileName(projectName);
    if (projectFile.IsOk()) {
        return projectFile.GetPath();
    }
    return workspace->GetDir();
}

void CodeStatsPanel::JoinWorker()
{
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void CodeStatsPanel::StartScan()
{
    if (m_scanning) {
        return;
    }

    wxString scopeLabel;
    wxString singleFile;
    const wxString root = ResolveRoot(GetSelectedScope(), scopeLabel, singleFile);
    if (root.empty() && singleFile.empty()) {
        m_statusLabel->SetLabel(wxT("没有可扫描的工作区或文件，请先打开工作区/文件。"));
        m_statusLabel->SetToolTip(m_statusLabel->GetLabel());
        return;
    }

    JoinWorker();

    m_cancel = false;
    ++m_generation;
    const unsigned generation = m_generation;

    fkcstats::ScanRequest request;
    request.root = root;
    request.scopeLabel = scopeLabel;
    request.singleFile = singleFile;
    request.useLibclang = m_clangCheck->IsChecked();

    SetScanning(true);
    m_progressGauge->SetRange(100);
    m_progressGauge->SetValue(0);
    m_statusLabel->SetLabel(wxT("正在遍历工作区..."));
    m_statusLabel->SetToolTip(m_statusLabel->GetLabel());

    m_worker = std::thread([this, request, generation]() {
        auto progressCallback = [this, generation](const fkcstats::ScanProgress& progress) {
            auto* event = new CodeStatsProgressEvent(wxEVT_FKC_SCAN_PROGRESS, static_cast<int>(generation));
            event->SetProgress(progress.processed, progress.total, progress.current, progress.phase);
            wxQueueEvent(this, event);
        };

        std::shared_ptr<fkcstats::ScanResult> result =
            fkcstats::CodeStatsScanner::Scan(request, m_cancel, progressCallback);

        auto* event = new CodeStatsEvent(wxEVT_FKC_SCAN_DONE, static_cast<int>(generation));
        event->SetResult(std::move(result));
        wxQueueEvent(this, event);
    });
}

void CodeStatsPanel::CancelScan()
{
    if (!m_scanning) {
        return;
    }
    m_cancel = true;
    m_cancelButton->Enable(false);
    m_statusLabel->SetLabel(wxT("正在取消扫描..."));
}

void CodeStatsPanel::SetScanning(bool scanning)
{
    m_scanning = scanning;
    m_scanButton->Enable(!scanning);
    m_scanButton->SetLabel(scanning ? wxT("扫描中...") : wxT("开始扫描"));
    m_cancelButton->Enable(scanning);
    m_scopeChoice->Enable(!scanning);
    m_clangCheck->Enable(!scanning && fkcstats::CodeStatsScanner::IsLibclangAvailable());
}

void CodeStatsPanel::OnScanClicked(wxCommandEvent& event)
{
    wxUnusedVar(event);
    StartScan();
}

void CodeStatsPanel::OnCancelClicked(wxCommandEvent& event)
{
    wxUnusedVar(event);
    CancelScan();
}

void CodeStatsPanel::OnScopeChanged(wxCommandEvent& event)
{
    wxUnusedVar(event);
    if (!m_scanning) {
        m_statusLabel->SetLabel(wxT("范围已切换，点击「开始扫描」重新分析。"));
    }
}

void CodeStatsPanel::OnProgress(CodeStatsProgressEvent& event)
{
    if (event.GetId() != static_cast<int>(m_generation) || !m_scanning) {
        return;
    }

    if (event.GetTotal() > 0) {
        if (m_progressGauge->GetRange() != event.GetTotal()) {
            m_progressGauge->SetRange(event.GetTotal());
        }
        m_progressGauge->SetValue(std::min(event.GetProcessed(), event.GetTotal()));
    } else {
        m_progressGauge->Pulse();
    }

    wxString status = event.GetPhase();
    if (event.GetTotal() > 0) {
        status += wxString::Format(wxT("  %d/%d"), event.GetProcessed(), event.GetTotal());
    }
    if (!event.GetCurrent().empty()) {
        status += wxT("  ") + event.GetCurrent();
    }
    m_statusLabel->SetLabel(status);
    m_statusLabel->SetToolTip(event.GetCurrent());
}

void CodeStatsPanel::OnScanDone(CodeStatsEvent& event)
{
    JoinWorker();

    if (event.GetId() != static_cast<int>(m_generation)) {
        return;
    }

    SetScanning(false);

    const std::shared_ptr<fkcstats::ScanResult> result = event.GetResult();
    if (!result) {
        m_statusLabel->SetLabel(wxT("扫描失败：后台线程未返回结果。"));
        return;
    }

    const bool firstResult = !HasResult();
    m_result = result;
    UpdateAll();

    if (!result->cancelled) {
        m_progressGauge->SetRange(100);
        m_progressGauge->SetValue(100);
    }
    m_statusLabel->SetLabel(result->message);
    m_statusLabel->SetToolTip(result->message);
    if (firstResult) {
        m_notebook->SetSelection(0);
    }
}

void CodeStatsPanel::UpdateAll()
{
    if (!m_result) {
        return;
    }
    UpdateSummaryCards();
    UpdateOverview();
    PopulateFileList();
    PopulateTodoList();
    PopulateTopComplexity();
}

void CodeStatsPanel::UpdateSummaryCards()
{
    const fkcstats::ScanTotals& totals = m_result->totals;
    m_valFiles->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.files));
    m_valCode->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.codeLines));
    m_valComment->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.commentLines));
    m_valRate->SetLabel(fkcstats::CodeStatsScanner::FormatPercent(totals.commentRate));
    m_valComplexity->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.complexity));
    m_valMarkers->SetLabel(
        fkcstats::CodeStatsScanner::FormatNumber(totals.todoCount + totals.fixmeCount));
}

void CodeStatsPanel::UpdateOverview()
{
    const fkcstats::ScanTotals& totals = m_result->totals;
    const int commentRate = std::max(0, std::min(100, static_cast<int>(std::lround(totals.commentRate))));
    const int quality = std::max(0, std::min(100, static_cast<int>(std::lround(totals.qualityScore))));

    m_commentGauge->SetValue(commentRate);
    m_commentGaugeLabel->SetLabel(
        wxString::Format(wxT("%s · 注释 %s 行 / 代码 %s 行"), fkcstats::CodeStatsScanner::FormatPercent(totals.commentRate),
                         fkcstats::CodeStatsScanner::FormatNumber(totals.commentLines).c_str(),
                         fkcstats::CodeStatsScanner::FormatNumber(totals.codeLines).c_str()));

    m_qualityGauge->SetValue(quality);
    m_qualityGaugeLabel->SetLabel(
        wxString::Format(wxT("%d 分 · %s"), quality, fkcstats::CodeStatsScanner::QualityGrade(totals.qualityScore).c_str()));

    m_infoTotal->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.totalLines));
    m_infoBlank->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.blankLines));
    m_infoFailed->SetLabel(fkcstats::CodeStatsScanner::FormatNumber(totals.failedFiles));
    m_infoEngine->SetLabel(m_result->engineLabel);
    m_infoElapsed->SetLabel(wxString::Format(wxT("%ld ms"), m_result->elapsedMs));
    m_infoTime->SetLabel(wxDateTime::Now().Format(wxT("%Y-%m-%d %H:%M:%S")));
    m_infoMarkers->SetLabel(wxString::Format(wxT("%d TODO / %d FIXME"), totals.todoCount, totals.fixmeCount));
    m_infoAst->SetLabel(wxString::Format(wxT("%d / %d"), totals.astFiles, totals.files));
    m_infoScope->SetLabel(m_result->scopeLabel);

    Layout();
}

void CodeStatsPanel::PopulateFileList()
{
    m_fileList->Freeze();
    m_fileList->DeleteAllItems();
    m_fileView.clear();

    if (!m_result) {
        m_fileList->Thaw();
        return;
    }

    const auto& files = m_result->files;
    m_fileView.resize(files.size());
    std::iota(m_fileView.begin(), m_fileView.end(), size_t(0));

    if (m_fileSortColumn >= 0) {
        std::sort(m_fileView.begin(), m_fileView.end(), [&](size_t leftIndex, size_t rightIndex) {
            const fkcstats::FileStats& left = files[leftIndex];
            const fkcstats::FileStats& right = files[rightIndex];
            int compare = 0;
            switch (m_fileSortColumn) {
            case 0:
                compare = left.displayPath.CmpNoCase(right.displayPath);
                break;
            case 1:
                compare = left.language.CmpNoCase(right.language);
                break;
            case 2:
                compare = CompareInt(left.totalLines, right.totalLines);
                break;
            case 3:
                compare = CompareInt(left.codeLines, right.codeLines);
                break;
            case 4:
                compare = CompareInt(left.commentLines, right.commentLines);
                break;
            case 5:
                compare = CompareInt(left.blankLines, right.blankLines);
                break;
            case 6:
                compare = CompareDouble(FileCommentRate(left), FileCommentRate(right));
                break;
            case 7:
                compare = CompareInt(left.complexity, right.complexity);
                break;
            case 8:
                compare = CompareInt(FileMarkerCount(left), FileMarkerCount(right));
                break;
            case 9:
                compare = CompareInt(left.astUsed ? 1 : 0, right.astUsed ? 1 : 0);
                break;
            default:
                break;
            }
            if (compare == 0) {
                compare = left.displayPath.CmpNoCase(right.displayPath);
            }
            return m_fileSortAscending ? compare < 0 : compare > 0;
        });
    }

    int row = 0;
    for (size_t index : m_fileView) {
        const fkcstats::FileStats& file = files[index];
        const long item = m_fileList->InsertItem(row, file.displayPath);
        m_fileList->SetItem(item, 1, file.language);
        m_fileList->SetItem(item, 2, fkcstats::CodeStatsScanner::FormatNumber(file.totalLines));
        m_fileList->SetItem(item, 3, fkcstats::CodeStatsScanner::FormatNumber(file.codeLines));
        m_fileList->SetItem(item, 4, fkcstats::CodeStatsScanner::FormatNumber(file.commentLines));
        m_fileList->SetItem(item, 5, fkcstats::CodeStatsScanner::FormatNumber(file.blankLines));
        m_fileList->SetItem(item, 6, fkcstats::CodeStatsScanner::FormatPercent(FileCommentRate(file)));
        m_fileList->SetItem(item, 7, fkcstats::CodeStatsScanner::FormatNumber(file.complexity));
        m_fileList->SetItem(item, 8, fkcstats::CodeStatsScanner::FormatNumber(FileMarkerCount(file)));
        m_fileList->SetItem(item, 9, file.astUsed ? wxT("AST") : wxT("-"));
        m_fileList->SetItemData(item, static_cast<long>(row));

        if (file.readError) {
            m_fileList->SetItemTextColour(item, kDangerText);
        } else if (file.complexity >= 60) {
            m_fileList->SetItemTextColour(item, kDangerText);
        } else if (file.complexity >= 30) {
            m_fileList->SetItemTextColour(item, kWarningText);
        }
        ++row;
    }

    m_fileList->Thaw();
    Layout();
}

void CodeStatsPanel::PopulateTodoList()
{
    m_todoList->Freeze();
    m_todoList->DeleteAllItems();
    m_todoView.clear();

    if (!m_result) {
        m_todoList->Thaw();
        return;
    }

    m_todoView.resize(m_result->todos.size());
    std::iota(m_todoView.begin(), m_todoView.end(), size_t(0));

    int row = 0;
    for (size_t index : m_todoView) {
        const fkcstats::TodoItem& todo = m_result->todos[index];
        const long item = m_todoList->InsertItem(row, todo.fixme ? wxT("FIXME") : wxT("TODO"));
        m_todoList->SetItem(item, 1, todo.displayPath);
        m_todoList->SetItem(item, 2, wxString::Format(wxT("%d"), todo.line));
        m_todoList->SetItem(item, 3, todo.text);
        m_todoList->SetItemData(item, static_cast<long>(row));
        m_todoList->SetItemTextColour(item, todo.fixme ? kDangerText : kWarningText);
        ++row;
    }

    m_todoList->Thaw();
    Layout();
}

void CodeStatsPanel::PopulateTopComplexity()
{
    m_topList->Freeze();
    m_topList->DeleteAllItems();
    m_topView.clear();

    if (!m_result) {
        m_topList->Thaw();
        return;
    }

    const auto& files = m_result->files;
    for (size_t i = 0; i < files.size(); ++i) {
        if (files[i].complexity > 0) {
            m_topView.push_back(i);
        }
    }

    std::sort(m_topView.begin(), m_topView.end(), [&](size_t leftIndex, size_t rightIndex) {
        const fkcstats::FileStats& left = files[leftIndex];
        const fkcstats::FileStats& right = files[rightIndex];
        if (left.complexity != right.complexity) {
            return left.complexity > right.complexity;
        }
        return left.displayPath.CmpNoCase(right.displayPath) < 0;
    });

    if (m_topView.size() > 10) {
        m_topView.resize(10);
    }

    int row = 0;
    for (size_t index : m_topView) {
        const fkcstats::FileStats& file = files[index];
        const long item = m_topList->InsertItem(row, file.displayPath);
        m_topList->SetItem(item, 1, fkcstats::CodeStatsScanner::FormatNumber(file.complexity));
        m_topList->SetItem(item, 2, fkcstats::CodeStatsScanner::FormatNumber(file.codeLines));
        m_topList->SetItem(item, 3, fkcstats::CodeStatsScanner::FormatPercent(FileCommentRate(file)));
        m_topList->SetItemData(item, static_cast<long>(row));
        if (file.complexity >= 60) {
            m_topList->SetItemTextColour(item, kDangerText);
        }
        ++row;
    }

    m_topList->Thaw();
    Layout();
}

void CodeStatsPanel::OnFileListActivated(wxListEvent& event)
{
    if (!m_result) {
        return;
    }
    const long row = m_fileList->GetItemData(event.GetIndex());
    if (row < 0 || static_cast<size_t>(row) >= m_fileView.size()) {
        return;
    }
    const size_t fileIndex = m_fileView[static_cast<size_t>(row)];
    if (fileIndex < m_result->files.size()) {
        OpenFileAt(m_result->files[fileIndex].path, 1);
    }
}

void CodeStatsPanel::OnTopListActivated(wxListEvent& event)
{
    if (!m_result) {
        return;
    }
    const long row = m_topList->GetItemData(event.GetIndex());
    if (row < 0 || static_cast<size_t>(row) >= m_topView.size()) {
        return;
    }
    const size_t fileIndex = m_topView[static_cast<size_t>(row)];
    if (fileIndex < m_result->files.size()) {
        OpenFileAt(m_result->files[fileIndex].path, 1);
    }
}

void CodeStatsPanel::OnTodoListActivated(wxListEvent& event)
{
    if (!m_result) {
        return;
    }
    const long row = m_todoList->GetItemData(event.GetIndex());
    if (row < 0 || static_cast<size_t>(row) >= m_todoView.size()) {
        return;
    }
    const size_t todoIndex = m_todoView[static_cast<size_t>(row)];
    if (todoIndex < m_result->todos.size()) {
        const fkcstats::TodoItem& todo = m_result->todos[todoIndex];
        OpenFileAt(todo.path, todo.line);
    }
}

void CodeStatsPanel::OnFileListColumnClick(wxListEvent& event)
{
    const int column = event.GetColumn();
    if (column == m_fileSortColumn) {
        m_fileSortAscending = !m_fileSortAscending;
    } else {
        m_fileSortColumn = column;
        m_fileSortAscending = true;
    }
    PopulateFileList();
}

void CodeStatsPanel::OpenFileAt(const wxString& path, int line)
{
    if (path.empty()) {
        return;
    }
    if (!wxFileName::FileExists(path)) {
        m_statusLabel->SetLabel(wxT("文件不存在: ") + path);
        m_statusLabel->SetToolTip(m_statusLabel->GetLabel());
        return;
    }

    if (m_mgr) {
        m_mgr->OpenFile(path, wxEmptyString, line > 0 ? line : wxNOT_FOUND);
    }
}

void CodeStatsPanel::ResetResults(const wxString& statusMessage)
{
    if (m_scanning) {
        m_cancel = true;
        ++m_generation;
        JoinWorker();
        SetScanning(false);
    }

    m_result.reset();
    m_fileView.clear();
    m_todoView.clear();
    m_topView.clear();

    m_valFiles->SetLabel(wxT("-"));
    m_valCode->SetLabel(wxT("-"));
    m_valComment->SetLabel(wxT("-"));
    m_valRate->SetLabel(wxT("-"));
    m_valComplexity->SetLabel(wxT("-"));
    m_valMarkers->SetLabel(wxT("-"));

    m_commentGauge->SetValue(0);
    m_commentGaugeLabel->SetLabel(wxT("-"));
    m_qualityGauge->SetValue(0);
    m_qualityGaugeLabel->SetLabel(wxT("-"));

    m_infoTotal->SetLabel(wxT("-"));
    m_infoBlank->SetLabel(wxT("-"));
    m_infoFailed->SetLabel(wxT("-"));
    m_infoEngine->SetLabel(wxT("-"));
    m_infoElapsed->SetLabel(wxT("-"));
    m_infoTime->SetLabel(wxT("-"));
    m_infoMarkers->SetLabel(wxT("-"));
    m_infoAst->SetLabel(wxT("-"));
    m_infoScope->SetLabel(wxT("-"));

    m_fileList->DeleteAllItems();
    m_todoList->DeleteAllItems();
    m_topList->DeleteAllItems();

    m_progressGauge->SetValue(0);
    const wxString message =
        statusMessage.empty() ? wxString(wxT("工作区已关闭，结果已清空。")) : statusMessage;
    m_statusLabel->SetLabel(message);
    m_statusLabel->SetToolTip(message);
    Layout();
}

void CodeStatsPanel::MarkStale(const wxString& reason)
{
    if (!m_result || m_scanning) {
        return;
    }

    const wxString message =
        reason.empty() ? wxT("检测到代码变更，当前统计结果可能已过期，请重新扫描。") : reason;
    m_statusLabel->SetLabel(message);
    m_statusLabel->SetToolTip(message);
}
