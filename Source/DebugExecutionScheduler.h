/*
 // Copyright (c) 2026 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Utility/Config.h"
#include "Pd/ConnectionTraceCollector.h"

class PluginEditor;
class Connection;

class DebugExecutionScheduler final {
public:
    explicit DebugExecutionScheduler(PluginEditor* editor);

    // Called from the editor's timer callback (e.g. 90 Hz).
    void update();

    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled; }

    // True while a scheduler plays messages one at a time in execution order, so connections don't animate them on their own
    static bool isExecutionOrderEnabled() { return executionOrderEnabled.load(std::memory_order_relaxed); }

    void clear();

private:
    // Shared by every scheduler in the process, like the flag Pd used to keep for this
    static inline std::atomic<bool> executionOrderEnabled { false };


    Connection* findConnectionForEvent(pd::ConnectionTraceEvent const& event) const;
    void scheduleNextEvent();
    void reorderPendingEventsByOutletPriority();

    PluginEditor* editor;
    bool enabled = false;

    pd::ConnectionTraceEvent currentEvent {};
    bool hasCurrentEvent = false;
    Component::SafePointer<Connection> currentConnection;

    moodycamel::ReaderWriterQueue<pd::ConnectionTraceEvent> pendingEvents { 4096 };
};
