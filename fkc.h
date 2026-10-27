#ifndef __fkc__
#define __fkc__

#include "clTabTogglerHelper.h"
#include "plugin.h"

#include <memory>

class CodeStatsPanel;
class clCommandEvent;
class clWorkspaceEvent;

class fkc : public IPlugin
{
public:
    fkc(IManager *manager);
    virtual ~fkc();

    //--------------------------------------------
    //Abstract methods
    //--------------------------------------------
    virtual void CreateToolBar(clToolBar* toolbar);
    /**
     * @brief Add plugin menu to the "Plugins" menu item in the menu bar
     */
    virtual void CreatePluginMenu(wxMenu *pluginsMenu);

    /**
     * @brief Unplug the plugin. Perform here any cleanup needed (e.g. unbind events, destroy allocated windows)
     */
    virtual void UnPlug();

private:
    void ShowPanel(bool rescan);
    void OnWorkspaceLoaded(clWorkspaceEvent& event);
    void OnWorkspaceClosed(clWorkspaceEvent& event);
    void OnFileSaved(clCommandEvent& event);

    CodeStatsPanel* m_panel{nullptr};
    clTabTogglerHelper::Ptr_t m_tabToggler;
};

#endif //fkc
