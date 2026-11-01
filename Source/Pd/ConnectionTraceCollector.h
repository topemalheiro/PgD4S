/*
 // Copyright (c) 2026 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#pragma once

#include "Instance.h"
#include <cstring>
#include <readerwriterqueue.h>

namespace pd {

struct ConnectionTraceEvent {
    t_outconnect* connection = nullptr;
    t_symbol* selector = nullptr;
    uint64_t sequenceNumber = 0;
    uint8_t atomCount = 0;
    bool truncated = false; // the message had more atoms than fit in atoms[]
    t_atom atoms[32];
};

class ConnectionTraceCollector final {
public:
    ConnectionTraceCollector() = default;

    void enqueue(t_outconnect* connection, t_symbol* selector, int argc, t_atom* argv)
    {
        if (!connection)
            return;

        // Ignore activity notifications; they use a canvas pointer, not a t_outconnect*.
        if (selector && strcmp(selector->s_name, "_activity") == 0)
            return;

        ConnectionTraceEvent event;
        event.connection = connection;
        event.selector = selector;
        event.sequenceNumber = ++sequenceCounter;
        event.atomCount = static_cast<uint8_t>(jmin(argc, static_cast<int>(std::size(event.atoms))));
        event.truncated = argc > event.atomCount;
        for (int i = 0; i < event.atomCount; ++i) {
            event.atoms[i] = argv[i];
        }

        queue.try_enqueue(event);
    }

    bool tryDequeue(ConnectionTraceEvent& event)
    {
        return queue.try_dequeue(event);
    }

    void clear()
    {
        ConnectionTraceEvent dummy;
        while (queue.try_dequeue(dummy)) { }
    }

    static void staticEnqueue(void* target, int type, void* connection, t_symbol* selector, int argc, t_atom* argv)
    {
        if (type != 1)
            return;

        auto* instance = static_cast<pd::Instance*>(target);
        if (instance && instance->traceCollector)
            instance->traceCollector->enqueue(static_cast<t_outconnect*>(connection), selector, argc, argv);
    }

private:
    moodycamel::ReaderWriterQueue<ConnectionTraceEvent> queue { 4096 };
    std::atomic<uint64_t> sequenceCounter { 0 };
};

} // namespace pd
