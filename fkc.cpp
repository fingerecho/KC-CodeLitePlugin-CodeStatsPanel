#include "fkc.h"

#include "code_stats_panel.h"
#include "event_notifier.h"

#include <wx/filename.h>

namespace {

const wxString& PanelLabel()
{
    static const wxString label = wxT("代码质量看板");
    return label;
}

} // namespace

// Define the plugin entry point
CL_PLUGIN_API IPlugin *CreatePlugin(IManager *manager) {
  return new fkc(manager);
}

CL_PLUGIN_API PluginInfo *GetPluginInfo() {
  static PluginInfo info;
  info.SetAuthor(wxT("fingerecho.com"));
  info.SetName(wxT("fingerecho"));
  info.SetDescription(_(L"一个基于 AST 的智能代码统计与质量看板"));
  info.SetVersion(wxT("v1.0"));
  return &info;
}

CL_PLUGIN_API int GetPluginInterfaceVersion() {
  return PLUGIN_INTERFACE_VERSION;
}

fkc::fkc(IManager *manager) : IPlugin(manager) {
  m_longName = _(L"一个基于 AST 的智能代码统计与质量看板");
  m_shortName = wxT("fkc-AST-CodePannel");

  m_panel = new CodeStatsPanel(m_mgr->BookGet(PaneId::BOTTOM_BAR), m_mgr);
  m_mgr->BookAddPage(PaneId::BOTTOM_BAR, m_panel, PanelLabel());
  m_tabToggler.reset(new clTabTogglerHelper(PanelLabel(), m_panel, wxEmptyString, nullptr));

  EventNotifier::Get()->Bind(wxEVT_WORKSPACE_LOADED, &fkc::OnWorkspaceLoaded, this);
  EventNotifier::Get()->Bind(wxEVT_WORKSPACE_CLOSED, &fkc::OnWorkspaceClosed, this);
  EventNotifier::Get()->Bind(wxEVT_FILE_SAVED, &fkc::OnFileSaved, this);
}

fkc::~fkc() {}

void fkc::CreateToolBar(clToolBar *toolbar) {
  wxUnusedVar(toolbar);
}

void fkc::CreatePluginMenu(wxMenu *pluginsMenu) {
  wxMenu *subMenu = new wxMenu();

  wxMenuItem *showItem = new wxMenuItem(subMenu, wxID_ANY, _(L"显示代码质量看板"));
  wxMenuItem *scanItem = new wxMenuItem(subMenu, wxID_ANY, _(L"重新扫描工作区"));

  subMenu->Append(showItem);
  subMenu->Append(scanItem);

  subMenu->Bind(wxEVT_MENU,
                [this](wxCommandEvent &) {
                  ShowPanel(false);
                },
                showItem->GetId());
  subMenu->Bind(wxEVT_MENU,
                [this](wxCommandEvent &) {
                  ShowPanel(true);
                },
                scanItem->GetId());

  pluginsMenu->AppendSubMenu(subMenu, _(L"克铖插件"));
}

void fkc::UnPlug() {
  EventNotifier::Get()->Unbind(wxEVT_WORKSPACE_LOADED, &fkc::OnWorkspaceLoaded, this);
  EventNotifier::Get()->Unbind(wxEVT_WORKSPACE_CLOSED, &fkc::OnWorkspaceClosed, this);
  EventNotifier::Get()->Unbind(wxEVT_FILE_SAVED, &fkc::OnFileSaved, this);

  m_tabToggler.reset();

  CodeStatsPanel *panel = m_panel;
  m_panel = nullptr;
  if (panel && !m_mgr->BookDeletePage(PaneId::BOTTOM_BAR, panel)) {
    panel->Destroy();
  }
}

void fkc::ShowPanel(bool rescan) {
  if (!m_panel) {
    return;
  }

  m_mgr->ShowPane(wxT("Output View"), true);
  m_mgr->BookSelectPage(PaneId::BOTTOM_BAR, m_panel);
  m_mgr->ShowOutputPane(PanelLabel());

  if (rescan) {
    m_panel->StartScan();
  } else if (!m_panel->HasResult() && !m_panel->IsScanning()) {
    m_panel->StartScan();
  }
}

void fkc::OnWorkspaceLoaded(clWorkspaceEvent &event) {
  event.Skip();
  if (m_panel) {
    m_panel->ResetResults(wxT("工作区已加载，点击「开始扫描」分析代码质量。"));
  }
}

void fkc::OnWorkspaceClosed(clWorkspaceEvent &event) {
  event.Skip();
  if (m_panel) {
    m_panel->ResetResults();
  }
}

void fkc::OnFileSaved(clCommandEvent &event) {
  event.Skip();
  if (!m_panel || !m_panel->HasResult()) {
    return;
  }

  const wxString fileName = event.GetFileName().empty() ? wxString(wxT("文件"))
                                                        : wxFileName(event.GetFileName()).GetFullName();
  m_panel->MarkStale(wxString::Format(wxT("检测到 %s 已保存，统计结果可能已过期，请重新扫描。"), fileName.c_str()));
}
