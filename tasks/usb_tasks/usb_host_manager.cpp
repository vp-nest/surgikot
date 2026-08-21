#include "usb_host_manager.hpp"
#include "task.h"
#include "host_msd_fatfs.h"
#include "usb_app.h"

// ---------------------------------------------------------------------
// Placeholder SDK surface.
//
// In the real project these types/constants/functions come from the
// NXP MCUXpresso USB host middleware (usb_host_config.h, usb_host.h,
// usb_host_msc.h) plus a small amount of app glue the SDK's own host
// examples always provide (host_msc_fatfs, etc.). They're stubbed out
// here so this file illustrates the *structure* of the integration
// without depending on the SDK being present in this sandbox. Delete
// this block and the real headers take over once this is dropped into
// an actual MCUXpresso project.
// ---------------------------------------------------------------------
extern "C" {
	// typedef int      usb_status_t;
    typedef void*    usb_device_handle;
    typedef void*    usb_host_configuration_handle;
    typedef usb_status_t (*host_event_callback_t)(usb_device_handle, usb_host_configuration_handle, uint32_t);

    // constexpr usb_status_t kStatus_USB_Success = 0;
    // constexpr uint32_t     kUSB_HostEventAttach = 1;
    // constexpr uint32_t     kUSB_HostEventDetach = 2;
    constexpr uint8_t      kUsbControllerId     = 0; // CONTROLLER_ID_EHCI0 in the real SDK

    usb_status_t USB_HostInit(uint8_t controllerId, usb_host_handle* hostHandle, host_event_callback_t callback);
    void USB_HostTaskFn(void* hostHandle);            // pumps the host controller driver
    void USB_HostApplicationTaskFn(void* hostHandle);  // internally calls the MSC class driver's task fn
}

// Vendor examples always create USB_HostTask / USB_HostApplicationTask as
// plain FreeRTOS tasks that loop calling the two *TaskFn() entry points
// above. We provide that glue here with C linkage so main.cpp can
// xTaskCreate() them by name exactly the way an NXP host example would.
extern "C" void USB_HostTask(void* param) {
    for (;;) {
        USB_HostTaskFn(param);
    }
}

extern "C" void USB_HostApplicationTask(void* param) {
    for (;;) {
    	USB_HostApplicationTaskFn(param);
    }
}

// Registered with USB_HostInit() as the stack's event callback. This runs
// in USB_HostApplicationTask's context (never in an ISR), so it must not
// block that task for long -- it just forwards a translated event into
// UsbHostManager's own queue and returns immediately.
static usb_status_t UsbHostEventTrampoline(usb_device_handle /*deviceHandle*/,
                                            usb_host_configuration_handle /*configurationHandle*/,
                                            uint32_t eventCode) {
    switch (eventCode) {
        case kUSB_HostEventAttach:
            // A real implementation inspects configurationHandle here to
            // confirm the attached device actually exposes an MSC
            // interface before treating it as "our" drive.
            UsbHostManager::instance().onHostStackEvent(UsbHostEvent::DEVICE_ATTACHED);
            break;
        case kUSB_HostEventDetach:
            UsbHostManager::instance().onHostStackEvent(UsbHostEvent::DEVICE_DETACHED);
            break;
        default:
            break;
    }
    return kStatus_USB_Success;
}

// Priorities are chosen so the vendor stack's two tasks sit above every
// app task in this project (see README for the full ladder):
//   USB_HostTask            -- must pump the controller promptly, or
//                               enumeration/transfers can fail outright.
//   USB_HostApplicationTask -- must preempt the app tasks it notifies
//                               (UsbUpgradeTask / LoggerTask) so attach/
//                               detach events aren't delayed behind them.
static constexpr UBaseType_t kUsbHostTaskPriority    = 7;
static constexpr UBaseType_t kUsbHostAppTaskPriority = 6;

bool UsbHostManager::init() {
    m_eventQueue = xQueueCreate(4, sizeof(UsbHostEvent));
    if (!m_eventQueue) {
        return false;
    }

    USB_HostApplicationInit();

    BaseType_t ok1 = xTaskCreate(USB_HostTask, "USBHostTask", 1024,
                                  m_hostHandle, kUsbHostTaskPriority, nullptr);
    BaseType_t ok2 = xTaskCreate(USB_HostApplicationTask, "USBHostAppTask", 1024,
                                  m_hostHandle, kUsbHostAppTaskPriority, nullptr);

    return ok1 == pdPASS && ok2 == pdPASS;
}

void UsbHostManager::onHostStackEvent(UsbHostEvent evt) {
    if (m_eventQueue) {
        // Never block the stack's own application task.
        xQueueSend(m_eventQueue, &evt, 0);
    }
}

bool UsbHostManager::mount(TickType_t attachTimeout) {
    if (!m_eventQueue) {
        return false; // init() was never called -- programming error
    }

    // Drain any stale event (e.g. a detach left over from a previous
    // session) so we don't act on old news.
    UsbHostEvent evt;
    while (xQueueReceive(m_eventQueue, &evt, 0) == pdTRUE) {
        // discarded
    }

    if (xQueueReceive(m_eventQueue, &evt, attachTimeout) != pdTRUE ||
        evt != UsbHostEvent::DEVICE_ATTACHED) {
        return false; // nothing plugged in within the timeout
    }

    // Bind a filesystem on top of the now-enumerated MSC block device,
    // e.g. FatFs's f_mount(&fs, "0:", 1) or a lightweight custom reader
    // driven through the MSC class driver's read/write transfer calls.
    m_mounted = true;
    return m_mounted;
}

void UsbHostManager::unmount() {
    // f_mount(NULL, "0:", 0) as appropriate; the MSC class driver itself
    // stays attached to USB_HostApplicationTask until physical detach.
    m_mounted = false;
}

bool UsbHostManager::readFile(const char* path, uint8_t* buf, size_t bufLen, size_t& outLen) {
    (void)path; (void)buf; (void)bufLen;
    outLen = 0;
    return m_mounted; // placeholder
}

bool UsbHostManager::writeFile(const char* path, const uint8_t* buf, size_t len, bool append) {
    (void)path; (void)buf; (void)len; (void)append;
    return m_mounted; // placeholder
}

bool UsbHostManager::exists(const char* path) {
    (void)path;
    return m_mounted; // placeholder
}
