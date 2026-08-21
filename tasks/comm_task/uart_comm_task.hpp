#pragma once
//
// uart_comm_task.hpp
// Talks to the peer microcontroller over a framed UART protocol.
// TX is queue-driven (g_uartTxQueue); RX is DMA/interrupt-driven into a
// ring buffer, with the ISR only signalling a binary semaphore -- all
// parsing happens in task context, never in the ISR.
//
// This task waits on TWO independent wake sources (TX queue + RX-ready
// semaphore) using a FreeRTOS queue set, so it stays fully event-driven
// with no polling.
//
#include "task_base.hpp"
#include "app_types.hpp"

class UartCommTask : public TaskBase {
public:
    // Highest priority in the app: this task's own CPU usage is small
    // (mostly asleep on the queue set), but the peer link is closest to
    // a hard real-time deadline (protocol timeouts on the other side).
    UartCommTask() : TaskBase("UartTask", 2048, 5) {}

protected:
    void init() override;
    void run() override;

private:
    void sendFrame(const UartTxMsg& msg);
    void pollRxAndParse();
    void onFrameReceived(const uint8_t* data, size_t len);
    void logEvent(LogSeverity sev, const char* fmt, ...);

    static constexpr size_t kRxRingSize = 256;
    uint8_t m_rxRing[kRxRingSize]{};
    size_t  m_rxTail = 0; // parser's read cursor into m_rxRing
};
