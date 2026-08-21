#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "app_types.hpp"
#include "gui_task.hpp"
#include "usb_upgrade_task.hpp"
#include "logger_task.hpp"
#include "uart_comm_task.hpp"
#include "usb_host_manager.hpp"

// Definitions of the extern handles declared in app_types.hpp.
QueueHandle_t g_upgradeQueue   = nullptr;
QueueHandle_t g_loggerQueue    = nullptr;
QueueHandle_t g_uartTxQueue    = nullptr;
QueueHandle_t g_guiEventQueue  = nullptr;
EventGroupHandle_t g_sysEventGroup = nullptr;

// Statically allocated task objects -- no heap churn, and lifetime is
// tied to the whole program, matching FreeRTOS task lifetime.
static GuiTask        s_guiTask;
static UsbUpgradeTask  s_upgradeTask;
static LoggerTask      s_loggerTask;
static UartCommTask    s_uartTask;

int main(void) {
    // BOARD_InitHardware();       // clocks, pinmux, RTC
    // BOARD_InitDebugConsole();

    g_upgradeQueue  = xQueueCreate(4,  sizeof(UpgradeMsg));
    g_loggerQueue   = xQueueCreate(16, sizeof(LogEventMsg));  // deepest: shared by every task
    g_uartTxQueue   = xQueueCreate(8,  sizeof(UartTxMsg));
    g_guiEventQueue = xQueueCreate(8,  sizeof(AppStatusEvent));
    g_sysEventGroup = xEventGroupCreate();

    configASSERT(g_upgradeQueue && g_loggerQueue && g_uartTxQueue &&
                 g_guiEventQueue && g_sysEventGroup);

    // Bring up the USB host stack first: this creates USB_HostTask and
    // USB_HostApplicationTask (see usb_host_manager.cpp) so they're
    // already running and servicing the controller before anything
    // could plug in a drive. UsbUpgradeTask/LoggerTask only ever *use*
    // UsbHostManager, they never start the stack themselves.
    configASSERT(UsbHostManager::instance().init());

    // Logger starts first: every other task may want to log a boot event
    // immediately, so its queue must be alive and being drained already.
    s_loggerTask.start();
    s_uartTask.start();
    s_upgradeTask.start();
    s_guiTask.start();

    vTaskStartScheduler();

    for (;;) {
        // Only reached if the scheduler fails to start (e.g. out of heap
        // for idle/timer task TCBs).
    }
}
