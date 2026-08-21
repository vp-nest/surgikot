#pragma once
//
// gui_task.hpp
// Owns the display/touch UI (assumed: LVGL). Never blocks on I/O itself --
// it only posts commands into g_upgradeQueue / g_loggerQueue and drains
// g_guiEventQueue for progress/result updates to reflect in the UI.
//
#include "task_base.hpp"
#include "app_types.hpp"

class GuiTask : public TaskBase {
public:
    GuiTask() : TaskBase("GUI", 1024, 3) {}

protected:
    void init() override;
    void run() override;

private:
    void handleStatusEvent(const AppStatusEvent& evt);

    // Wired up as LVGL button-click callbacks during init().
    void onUpgradeButtonPressed();
    void onExportLogsButtonPressed();
};
