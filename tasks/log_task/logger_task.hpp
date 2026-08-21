#pragma once
//
// logger_task.hpp
// Sole owner of the LittleFS instance. Handles two very different jobs
// through the same inbound queue:
//   LOG_EVENT      - append one line to the on-flash log (from any task)
//   EXPORT_TO_USB  - stream the whole log file out to a USB drive
//                    (only ever triggered by the GUI task)
//
#include "task_base.hpp"
#include "app_types.hpp"

class LoggerTask : public TaskBase {
public:
    LoggerTask() : TaskBase("LoggerTask", 2048, 3) {}

protected:
    void init() override;
    void run() override;

private:
    void handleLogEvent(const LogEventMsg& msg);
    void handleExport();
    void reportStatus(AppEventStatus status, uint8_t percent, int32_t err = 0);

    static constexpr const char* kLogFilePath   = "/logs/events.log";
    static constexpr const char* kExportUsbPath = "/usb/events_export.log";
};
