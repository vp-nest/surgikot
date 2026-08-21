#include "uart_comm_task.hpp"
#include "semphr.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

// Given from the UART DMA/idle-line ISR; consumed only by this task.
static SemaphoreHandle_t s_rxReadySem;
static QueueSetHandle_t  s_queueSet;

// Example ISR-side hook (called from the LPUART DMA-complete or
// idle-line-detect interrupt handler registered in init()):
//
//   extern "C" void UART_RxIdleLine_IRQHandler(void) {
//       BaseType_t hpw = pdFALSE;
//       xSemaphoreGiveFromISR(s_rxReadySem, &hpw);
//       portYIELD_FROM_ISR(hpw);
//   }

void UartCommTask::init() {
    // BOARD_InitUartPeripheral(): configure LPUART baud/parity/framing,
    // set up a DMA channel to continuously fill m_rxRing circularly, and
    // enable an idle-line/half-full DMA interrupt that gives s_rxReadySem.

    s_rxReadySem = xSemaphoreCreateBinary();

    // A queue set lets this task block on "TX command pending" OR
    // "RX data ready" at the same time, without polling either one.
    s_queueSet = xQueueCreateSet(8 /* uartTxQueue depth */ + 1 /* rx sem */);
    xQueueAddToSet(g_uartTxQueue, s_queueSet);
    xQueueAddToSet(s_rxReadySem, s_queueSet);
}

void UartCommTask::run() {
    for (;;) {
        QueueSetMemberHandle_t activated =
            xQueueSelectFromSet(s_queueSet, pdMS_TO_TICKS(1000));

        if (activated == g_uartTxQueue) {
            UartTxMsg msg;
            if (xQueueReceive(g_uartTxQueue, &msg, 0) == pdTRUE) {
                sendFrame(msg);
            }
        } else if (activated == s_rxReadySem) {
            xSemaphoreTake(s_rxReadySem, 0);
            pollRxAndParse();
        }
        // Timeout with nothing activated: good place for an optional
        // link-alive heartbeat / watchdog check on the peer connection.
    }
}

void UartCommTask::sendFrame(const UartTxMsg& msg) {
    // Wrap msg.payload[0..msg.len) in the wire framing below and kick off
    // a (non-blocking) DMA TX:
    //   [STX][LEN][PAYLOAD...][CRC16][ETX]
    // e.g. LPUART_TransferSendNonBlocking(UART_PERIPH, &txHandle, &xfer);
}

void UartCommTask::pollRxAndParse() {
    // Simple framed-protocol state machine reading out of m_rxRing:
    //   1. scan for STX
    //   2. read LEN
    //   3. accumulate LEN payload bytes (wait for more RX-ready signals
    //      if the ring doesn't have them all yet)
    //   4. validate CRC16 + trailing ETX
    //   5. on success -> onFrameReceived(payload, len)
    //      on CRC/framing error -> logEvent(LOG_WARNING, "bad frame")
    //
    // m_rxTail tracks how far the parser has consumed; the DMA/ISR side
    // owns the write cursor independently, so this is a classic
    // single-producer/single-consumer ring buffer.
}

void UartCommTask::onFrameReceived(const uint8_t* data, size_t len) {
    // Forward decoded telemetry/state to the GUI for display...
    AppStatusEvent evt{ AppEventSource::UART_TASK, AppEventStatus::PROGRESS, 0, 0 };
    xQueueSend(g_guiEventQueue, &evt, 0);

    // ...and record it in the persistent event log.
    logEvent(LogSeverity::LOG_INFO, "RX frame len=%u", static_cast<unsigned>(len));
    (void)data;
}

void UartCommTask::logEvent(LogSeverity sev, const char* fmt, ...) {
    LogEventMsg msg{};
    msg.cmd = LoggerCmd::LOG_EVENT;
    msg.severity = sev;
    std::strncpy(msg.tag, "UART", sizeof(msg.tag) - 1);
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.message, sizeof(msg.message), fmt, args);
    va_end(args);
    // Never block: if the logger queue is momentarily full, drop the log
    // line rather than stall the UART link.
    xQueueSend(g_loggerQueue, &msg, 0);
}
