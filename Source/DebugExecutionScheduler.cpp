/*
 // Copyright (c) 2026 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "DebugExecutionScheduler.h"

#include <algorithm>
#include <functional>
#include <vector>

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

void DebugExecutionScheduler::reorderPendingEventsByOutletPriority()
{
    // Drain the pending queue into a local vector.
    std::vector<pd::ConnectionTraceEvent> events;
    events.reserve(pendingEvents.size_approx());
    pd::ConnectionTraceEvent event;
    while (pendingEvents.try_dequeue(event)) {
        events.push_back(event);
    }

    if (events.size() < 2)
        return;

    struct EventNode {
        pd::ConnectionTraceEvent event;
        Connection* connection = nullptr;
        std::vector<EventNode> children;
    };

    // Parse the depth-first trace stream into an execution tree.
    std::function<std::pair<std::vector<EventNode>, size_t>(size_t, size_t, Object*)> parseForest;
    parseForest = [&](size_t start, size_t end, Object* parentDestination) -> std::pair<std::vector<EventNode>, size_t> {
        std::vector<EventNode> forest;
        size_t i = start;
        while (i < end) {
            Connection* conn = findConnectionForEvent(events[i]);
            Object* sourceObj = conn ? conn->outobj.get() : nullptr;

            // If we've left the current subtree, hand control back to the parent.
            if (parentDestination && sourceObj != parentDestination)
                break;

            EventNode node;
            node.event = events[i];
            node.connection = conn;

            Object* destinationObj = conn ? conn->inobj.get() : nullptr;
            auto [children, nextIndex] = parseForest(i + 1, end, destinationObj);
            node.children = std::move(children);
            i = nextIndex;

            forest.push_back(std::move(node));
        }
        return { std::move(forest), i };
    };

    auto [roots, _] = parseForest(0, events.size(), nullptr);

    // Sort children of each node by outlet index descending: right outlets first.
    std::function<void(EventNode&)> sortNodes;
    sortNodes = [&](EventNode& node) {
        if (node.children.size() > 1) {
            std::sort(node.children.begin(), node.children.end(), [](EventNode const& a, EventNode const& b) {
                int const outA = a.connection ? a.connection->outIdx : 0;
                int const outB = b.connection ? b.connection->outIdx : 0;
                return outA > outB;
            });
        }
        for (auto& child : node.children)
            sortNodes(child);
    };

    for (auto& root : roots)
        sortNodes(root);

    // Enqueue events back in pre-order (now with right-to-left outlet priority).
    std::function<void(EventNode const&)> enqueueNode;
    enqueueNode = [&](EventNode const& node) {
        pendingEvents.try_enqueue(node.event);
        for (auto const& child : node.children)
            enqueueNode(child);
    };

    for (auto const& root : roots)
        enqueueNode(root);
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

    // Reorder pending events so that, for each source object, right outlets fire
    // before left outlets while preserving depth-first execution order.
    reorderPendingEventsByOutletPriority();

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
