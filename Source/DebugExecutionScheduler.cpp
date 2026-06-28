/*
 // Copyright (c) 2026 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "DebugExecutionScheduler.h"

#include "PluginEditor.h"
#include "Canvas.h"
#include "Connection.h"
#include "Pd/Instance.h"

DebugExecutionScheduler::DebugExecutionScheduler(PluginEditor* e)
    : editor(e)
{
}

void DebugExecutionScheduler::setEnabled(bool const shouldBeEnabled)
{
    if (enabled == shouldBeEnabled)
        return;

    enabled = shouldBeEnabled;
    set_plugdata_execution_order_enabled(enabled);

    if (!enabled) {
        clear();
    }
}

void DebugExecutionScheduler::clear()
{
    pendingEvents = moodycamel::ReaderWriterQueue<pd::ConnectionTraceEvent> { 4096 };
    if (currentConnection) {
        currentConnection->clearScheduledMessage();
    }
    currentConnection = nullptr;
    hasCurrentEvent = false;
}

Connection* DebugExecutionScheduler::findConnectionForEvent(pd::ConnectionTraceEvent const& event) const
{
    if (!event.connection)
        return nullptr;

    for (auto* cnv : editor->getCanvases()) {
        for (auto* connection : cnv->connections) {
            if (connection && connection->getPointer() == event.connection)
                return connection;
        }
    }
    return nullptr;
}

void DebugExecutionScheduler::scheduleNextEvent()
{
    if (hasCurrentEvent)
        return;

    pd::ConnectionTraceEvent event;
    if (!pendingEvents.try_dequeue(event))
        return;

    auto* connection = findConnectionForEvent(event);
    if (!connection)
        return;

    Connection::DebugMessage msg;
    msg.selector = event.selector;
    for (int i = 0; i < event.atomCount; ++i) {
        msg.atoms.emplace_back(&event.atoms[i]);
    }
    msg.sequenceNumber = event.sequenceNumber;
    msg.arrivalTimeMs = Time::getMillisecondCounterHiRes();
    msg.progress = 0.0f;

    connection->setScheduledMessage(msg);
    currentConnection = connection;
    currentEvent = event;
    hasCurrentEvent = true;
}

void DebugExecutionScheduler::update()
{
    if (!enabled)
        return;

    // Drain the trace collector into our pending queue.
    if (auto* pd = editor->pd) {
        if (pd->traceCollector) {
            pd::ConnectionTraceEvent event;
            while (pd->traceCollector->tryDequeue(event)) {
                pendingEvents.try_enqueue(event);
            }
        }
    }

    // If the current event has finished animating, clear it and schedule the next one.
    if (hasCurrentEvent) {
        if (!currentConnection || currentConnection->isScheduledMessageComplete()) {
            if (currentConnection)
                currentConnection->clearScheduledMessage();
            currentConnection = nullptr;
            hasCurrentEvent = false;
        }
    }

    scheduleNextEvent();
}
