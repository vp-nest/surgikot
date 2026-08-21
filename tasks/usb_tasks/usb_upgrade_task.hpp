#pragma once
//
// usb_upgrade_task.hpp
// Consumes UpgradeMsg commands from the GUI, mounts the USB drive (via the
// shared UsbHostManager), verifies and flashes a firmware image into the
// inactive bank of an A/B partition scheme, then reports progress/result
// back to the GUI and writes an audit trail through the logger.
//
#include "task_base.hpp"
#include "app_types.hpp"

class UsbUpgradeTask : public TaskBase {
public:
    UsbUpgradeTask() : TaskBase("UpgradeTask", 2048, 4) {}

protected:
    void run() override;

private:
    void handleStart(const UpgradeMsg& msg);
    bool verifyImage(const char* path);   // header magic + version + CRC/signature check
    bool flashImage(const char* path);    // stream-copy into the inactive flash bank

    void reportStatus(AppEventStatus status, uint8_t percent, int32_t err = 0);
    void logEvent(LogSeverity sev, const char* fmt, ...);
};
