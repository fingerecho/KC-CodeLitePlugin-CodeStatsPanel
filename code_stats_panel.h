#ifndef FKC_CODE_STATS_PANEL_H
#define FKC_CODE_STATS_PANEL_H

#include "code_stats.h"

#include <wx/event.h>
#include <wx/panel.h>
#include <wx/thread.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

class IManager;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxFlexGridSizer;
class wxGauge;
class wxListCtrl;
class wxListEvent;
class wxNotebook;
class wxStaticText;

class CodeStatsProgressEvent : public wxThreadEvent {
public:
    CodeStatsProgressEvent(wxEventType type = wxEVT_THREAD, int id = wxID_ANY)
        : wxThreadEvent(type, id)
    {
    }

    CodeStatsProgressEvent(const CodeStatsProgressEvent& other)
        : wxThreadEvent(other)
        , m_processed(other.m_processed)
        , m_total(other.m_total)
        , m_current(other.m_current)
        , m_phase(other.m_phase)
    {
    }

    wxEvent* Clone() const override { return new CodeStatsProgressEvent(*this); }

    void SetProgress(int processed, int total, const wxString& current, const wxString& phase)
    {
        m_processed = processed;
        m_total = total;
        m_current = current;
        m_phase = phase;
    }

    int GetProcessed() const { return m_processed; }
    int GetTotal() const { return m_total; }
    const wxString& GetCurrent() const { return m_current; }
    const wxString& GetPhase() const { return m_phase; }

private:
    int m_processed{0};
    int m_total{0};
    wxString m_current;
    wxString m_phase;
};

class CodeStatsEvent : public wxThreadEvent {
public:
    CodeStatsEvent(wxEventType type = wxEVT_THREAD, int id = wxID_ANY)
        : wxThreadEvent(type, id)
    {
    }

    CodeStatsEvent(const CodeStatsEvent& other)
        : wxThreadEvent(other)
        , m_result(other.m_result)
    {
    }

    wxEvent* Clone() const override { return new CodeStatsEvent(*this); }

    void SetResult(std::shared_ptr<fkcstats::ScanResult> result) { m_result = std::move(result); }
    const std::shared_ptr<fkcstats::ScanResult>& GetResult() const { return m_result; }

private:
    std::shared_ptr<fkcstats::ScanResult> m_result;
};

wxDECLARE_EVENT(wxEVT_FKC_SCAN_PROGRESS, CodeStatsProgressEvent);
wxDECLARE_EVENT(wxEVT_FKC_SCAN_DONE, CodeStatsEvent);

class CodeStatsPanel : public wxPanel {
public:
    CodeStatsPanel(wxWindow* parent, IManager* manager);
    ~CodeStatsPanel() override;

    void StartScan();
    void CancelScan();
    void ResetResults(const wxString& statusMessage = wxEmptyString);
    void MarkStale(const wxString& reason);

    bool HasResult() const { return m_result && !m_result->files.empty(); }
    bool IsScanning() const { return m_scanning; }

private:
    enum class Scope { Workspace, ActiveProject, CurrentFile };

    void BuildUI();
    wxPanel* CreateCard(wxWindow* parent, const wxString& title, wxStaticText** value);
    wxPanel* BuildOverviewPage();
    wxPanel* BuildFilesPage();
    wxPanel* BuildMarkersPage();
    void AddInfoRow(wxWindow* parent, wxFlexGridSizer* grid, const wxString& title, wxStaticText** value);

    Scope GetSelectedScope() const;
    wxString ResolveRoot(Scope scope, wxString& scopeLabel, wxString& singleFile);
    void JoinWorker();

    void OnScanClicked(wxCommandEvent& event);
    void OnCancelClicked(wxCommandEvent& event);
    void OnScopeChanged(wxCommandEvent& event);
    void OnProgress(CodeStatsProgressEvent& event);
    void OnScanDone(CodeStatsEvent& event);
    void OnFileListActivated(wxListEvent& event);
    void OnFileListColumnClick(wxListEvent& event);
    void OnTopListActivated(wxListEvent& event);
    void OnTodoListActivated(wxListEvent& event);

    void SetScanning(bool scanning);
    void UpdateAll();
    void UpdateSummaryCards();
    void UpdateOverview();
    void PopulateFileList();
    void PopulateTodoList();
    void PopulateTopComplexity();
    void OpenFileAt(const wxString& path, int line);

    IManager* m_mgr{nullptr};

    wxChoice* m_scopeChoice{nullptr};
    wxCheckBox* m_clangCheck{nullptr};
    wxButton* m_scanButton{nullptr};
    wxButton* m_cancelButton{nullptr};
    wxGauge* m_progressGauge{nullptr};
    wxStaticText* m_statusLabel{nullptr};
    wxNotebook* m_notebook{nullptr};

    wxStaticText* m_valFiles{nullptr};
    wxStaticText* m_valCode{nullptr};
    wxStaticText* m_valComment{nullptr};
    wxStaticText* m_valRate{nullptr};
    wxStaticText* m_valComplexity{nullptr};
    wxStaticText* m_valMarkers{nullptr};

    wxGauge* m_commentGauge{nullptr};
    wxStaticText* m_commentGaugeLabel{nullptr};
    wxGauge* m_qualityGauge{nullptr};
    wxStaticText* m_qualityGaugeLabel{nullptr};

    wxStaticText* m_infoTotal{nullptr};
    wxStaticText* m_infoBlank{nullptr};
    wxStaticText* m_infoFailed{nullptr};
    wxStaticText* m_infoEngine{nullptr};
    wxStaticText* m_infoElapsed{nullptr};
    wxStaticText* m_infoTime{nullptr};
    wxStaticText* m_infoMarkers{nullptr};
    wxStaticText* m_infoAst{nullptr};
    wxStaticText* m_infoScope{nullptr};

    wxListCtrl* m_topList{nullptr};
    wxListCtrl* m_fileList{nullptr};
    wxListCtrl* m_todoList{nullptr};

    std::shared_ptr<fkcstats::ScanResult> m_result;
    std::vector<size_t> m_fileView;
    std::vector<size_t> m_todoView;
    std::vector<size_t> m_topView;
    int m_fileSortColumn{-1};
    bool m_fileSortAscending{true};

    std::thread m_worker;
    std::atomic<bool> m_cancel{false};
    bool m_scanning{false};
    unsigned m_generation{0};
};

#endif // FKC_CODE_STATS_PANEL_H
