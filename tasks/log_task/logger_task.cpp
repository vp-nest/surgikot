#include "logger_task.hpp"
#include "littlefs_manager.hpp"
#include "usb_host_manager.hpp"
#include "lfs.h"
#include <cstdio>

void LoggerTask::init() {
    // Mounts on-chip/QSPI-NOR LittleFS; formats automatically on first
    // ever boot if the superblock isn't valid.
    LittleFsManager::instance().mount();
}

void LoggerTask::run() {
    LogEventMsg msg;
    for (;;) {
        if (xQueueReceive(g_loggerQueue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.cmd) {
                case LoggerCmd::LOG_EVENT:     handleLogEvent(msg); break;
                case LoggerCmd::EXPORT_TO_USB: handleExport();      break;
                case LoggerCmd::CANCEL_EXPORT: /* set abort flag for a future streaming export */ break;
            }
        }
    }
}

void LoggerTask::handleLogEvent(const LogEventMsg& msg) {
    char line[160];
    int n = snprintf(line, sizeof(line), "[%lu][%s][%u] %s\n",
                      static_cast<unsigned long>(msg.timestamp),
                      msg.tag, static_cast<unsigned>(msg.severity), msg.message);
    if (n > 0) {
        LittleFsManager::instance().appendLine(kLogFilePath, line, static_cast<size_t>(n));
    }
    // Note: this task must never block waiting on the USB bus while
    // handling a LOG_EVENT -- log appends have to stay fast so the queue
    // never backs up. Exporting is the only path that touches USB.
}

void LoggerTask::handleExport() {
    reportStatus(AppEventStatus::STARTED, 0);

    UsbLock lock(pdMS_TO_TICKS(2000));
    if (!lock.ok()) {
        reportStatus(AppEventStatus::FAILURE, 0, -1);
        return;
    }

    bool ok = UsbHostManager::instance().mount();

    if (ok) {
        lfs_t* lfs = LittleFsManager::instance().raw();
        lfs_file_t file;
        ok = (lfs_file_open(lfs, &file, kLogFilePath, LFS_O_RDONLY) == 0);

        if (ok) {
            // Stream in fixed-size chunks rather than loading the whole
            // log into RAM -- the log file can grow far larger than any
            // sensible on-chip buffer.
            uint8_t buf[512];
            lfs_ssize_t r;
            bool firstChunk = true;
            while ((r = lfs_file_read(lfs, &file, buf, sizeof(buf))) > 0) {
                if (!UsbHostManager::instance().writeFile(kExportUsbPath, buf,
                                                           static_cast<size_t>(r),
                                                           /*append=*/!firstChunk)) {
                    ok = false;
                    break;
                }
                firstChunk = false;
                reportStatus(AppEventStatus::PROGRESS, 50); // could track real % of file size
            }
            lfs_file_close(lfs, &file);
        }
    }

    UsbHostManager::instance().unmount();

    reportStatus(ok ? AppEventStatus::SUCCESS : AppEventStatus::FAILURE, ok ? 100 : 0);
}

void LoggerTask::reportStatus(AppEventStatus status, uint8_t percent, int32_t err) {
    AppStatusEvent evt{ AppEventSource::LOGGER_TASK, status, percent, err };
    xQueueSend(g_guiEventQueue, &evt, 0);
}
