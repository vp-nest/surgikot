#pragma once
//
// task_base.hpp
// Thin C++ wrapper around a FreeRTOS task so each task is a self-contained
// object instead of a bare C function + global state.
//
#include "FreeRTOS.h"
#include "task.h"

class TaskBase {
public:
    TaskBase(const char* name, uint16_t stackWords, UBaseType_t priority)
        : m_name(name), m_stackWords(stackWords), m_priority(priority), m_handle(nullptr) {}

    virtual ~TaskBase() = default;

    // Non-copyable: a task object owns a live FreeRTOS handle.
    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;

    bool start() {
        BaseType_t ok = xTaskCreate(&TaskBase::trampoline, m_name, m_stackWords,
                                    this, m_priority, &m_handle);
        return ok == pdPASS;
    }

    TaskHandle_t handle() const { return m_handle; }
    const char*  name()   const { return m_name; }

protected:
    // run() is the task body. It must contain the infinite for(;;) loop and
    // must never return.
    virtual void run() = 0;

    // init() runs once, on the task's own stack, immediately before run().
    // Use it for anything that must happen in task context (peripheral
    // init that needs the scheduler running, first-mount of a filesystem,
    // etc.) rather than in a global constructor.
    virtual void init() {}

private:
    static void trampoline(void* pv) {
        auto* self = static_cast<TaskBase*>(pv);
        self->init();
        self->run();
        // run() must not return; this is just a safety net so a stray
        // `return` doesn't fall off the end of the task function.
        vTaskDelete(nullptr);
    }

    const char*  m_name;
    uint16_t     m_stackWords;   // FreeRTOS stack depth is in words, not bytes
    UBaseType_t  m_priority;
    TaskHandle_t m_handle;
};
