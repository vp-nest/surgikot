#pragma once
//
// usb_host_manager.hpp
//
// The MCUXpresso USB host stack is NOT a passive library you call into --
// it runs its own two FreeRTOS tasks, created once at boot:
//
//   USB_HostTask            Pumps the host controller driver (EHCI/OHCI).
//                            Must run at a priority high enough to service
//                            the controller promptly -- this is the stack's
//                            equivalent of an interrupt bottom half.
//
//   USB_HostApplicationTask  Delivers attach/detach/enumeration-done events
//                            up to the application via the USB_HostEvent()
//                            callback the app registers with USB_HostInit().
//                            Runs at a lower priority than USB_HostTask but
//                            still above the tasks that merely consume USB
//                            data (our UsbUpgradeTask / LoggerTask).
//
// UsbHostManager therefore does NOT talk to the controller directly. It
// (1) starts the stack and its two tasks once, at boot, and
// (2) exposes a small event queue that USB_HostEvent() feeds, so
//     mount() can synchronously wait for "MSC device attached" instead of
//     the app having to know anything about the stack's internal task
//     structure.
//
// The upgrade/export mutex below is still needed on top of all this: the
// stack lets exactly one class-driver session use the MSC device at a
// time, and UsbUpgradeTask / LoggerTask must not both try to drive it
// concurrently.
//
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include <cstdint>
#include <cstddef>

// Forward-declare the SDK's host handle type without pulling the whole
// USB host stack into every translation unit that includes this header.
extern "C" {
    typedef void* usb_host_handle;
}

enum class UsbHostEvent : uint8_t {
    DEVICE_ATTACHED,
    DEVICE_DETACHED,
};

class UsbHostManager {
public:
    static UsbHostManager& instance() {
        static UsbHostManager inst;
        return inst;
    }

    // Called once from main(), before the scheduler starts servicing any
    // app task that might call mount(). Initializes the host stack
    // (USB_HostInit), creates USB_HostTask and USB_HostApplicationTask,
    // and registers UsbHostManager's static callback as the stack's
    // USB_HostEvent handler.
    bool init();

    // Exclusive session for one caller (UsbUpgradeTask xor LoggerTask) at
    // a time -- see UsbLock below.
    bool acquire(TickType_t timeout = portMAX_DELAY) {
        return xSemaphoreTake(m_sessionMutex, timeout) == pdTRUE;
    }
    void release() { xSemaphoreGive(m_sessionMutex); }

    // Waits (with timeout) for a DEVICE_ATTACHED event from the stack,
    // then binds a filesystem on top of the enumerated MSC volume.
    bool mount(TickType_t attachTimeout = pdMS_TO_TICKS(3000));
    void unmount();

    // Whole/streamed-file helpers used by the upgrade/export flows.
    bool readFile(const char* path, uint8_t* buf, size_t bufLen, size_t& outLen);
    bool writeFile(const char* path, const uint8_t* buf, size_t len, bool append);
    bool exists(const char* path);

    // Invoked by the vendor stack (from USB_HostApplicationTask context)
    // on attach/detach/enumeration events. Public because it's called
    // from a C-linkage trampoline registered with USB_HostInit, not
    // because application code should call it directly.
    void onHostStackEvent(UsbHostEvent evt);

private:
    UsbHostManager() { m_sessionMutex = xSemaphoreCreateMutex(); }

    SemaphoreHandle_t m_sessionMutex;
    QueueHandle_t     m_eventQueue = nullptr;   // UsbHostEvent, depth 4
    usb_host_handle   m_hostHandle = nullptr;
    bool              m_mounted    = false;
};

// Scope guard: `UsbLock lock; if (!lock.ok()) return;` acquires on
// construction, releases on destruction.
class UsbLock {
public:
    explicit UsbLock(TickType_t timeout = portMAX_DELAY)
        : m_ok(UsbHostManager::instance().acquire(timeout)) {}
    ~UsbLock() { if (m_ok) UsbHostManager::instance().release(); }
    bool ok() const { return m_ok; }

    UsbLock(const UsbLock&) = delete;
    UsbLock& operator=(const UsbLock&) = delete;

private:
    bool m_ok;
};
