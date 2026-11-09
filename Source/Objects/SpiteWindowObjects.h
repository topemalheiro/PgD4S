/*
 // PgD4S (plugdata for Spite): the GUI side of [pcontrol] and [thispatcher] (pure-data/src/x_spite.c).
 // Pd opens and closes ordinary subpatches itself; a subpatch marked "open in presentation" gets its own
 // presentation window here, as Max opens such a subpatcher.
 */
#pragma once

#include "Components/PresentationWindow.h"

class PcontrolObject final : public TextObjectBase {
public:
    PcontrolObject(pd::WeakReference obj, Object* parent)
        : TextObjectBase(obj, parent)
    {
    }

    // the canvases [pcontrol]'s outlet is wired to, that open in presentation
    SmallArray<t_canvas*> getPresentationTargets()
    {
        SmallArray<t_canvas*> targets;
        if (auto pcontrol = ptr.get<t_object>()) {
            t_outlet* out = nullptr;
            auto* connection = obj_starttraverseoutlet(pcontrol.get(), &out, 0);
            while (connection) {
                t_object* destination = nullptr;
                t_inlet* inlet = nullptr;
                int which = 0;
                connection = obj_nexttraverseoutlet(connection, &destination, &inlet, &which);
                if (destination && pd_class(&destination->te_pd) == canvas_class) {
                    auto* canvas = reinterpret_cast<t_canvas*>(destination);
                    if (canvas->gl_presentation_open)
                        targets.add(canvas);
                }
            }
        }
        return targets;
    }

    void receiveObjectMessage(hash32 const symbol, SmallArray<pd::Atom> const& atoms) override
    {
        if (symbol == hash("open")) {
            for (auto* canvas : getPresentationTargets())
                openLater(pd, canvas);
        } else if (symbol == hash("close")) {
            for (auto* canvas : getPresentationTargets())
                closeLater(pd, canvas);
        }
    }

    // Windows open and close after the message: a new window's editor adds message listeners, which
    // must not happen while Pd's messages are being handed out
    static void openLater(PluginProcessor* instance, t_canvas* canvas)
    {
        MessageManager::callAsync([instance, canvas, weak = pd::WeakReference(canvas, instance)] {
            if (weak.isValid())
                PresentationWindow::open(instance, canvas);
        });
    }

    static void closeLater(PluginProcessor* instance, t_canvas* canvas)
    {
        MessageManager::callAsync([canvas] { PresentationWindow::close(canvas); });
    }
};

class ThispatcherObject final : public TextObjectBase {
public:
    ThispatcherObject(pd::WeakReference obj, Object* parent)
        : TextObjectBase(obj, parent)
    {
    }

    void receiveObjectMessage(hash32 const symbol, SmallArray<pd::Atom> const& atoms) override
    {
        auto* canvas = cnv->patch.getRawPointer();
        if (!canvas)
            return;

        switch (symbol) {
        case hash("window"): {
            // "window exec" applied a new size or flags: an open presentation window takes them
            if (!atoms.empty() && atoms[0].isSymbol() && atoms[0].toString() == "exec") {
                // after the message: the window may hold the canvas this object is drawn in
                MessageManager::callAsync([instance = pd, canvas] { PresentationWindow::refresh(instance, canvas); });
            }
            break;
        }
        case hash("front"): {
            if (canvas->gl_presentation_open)
                PcontrolObject::openLater(pd, canvas);
            break;
        }
        case hash("wclose"): {
            // after the message: the window may hold the canvas this object is drawn in
            MessageManager::callAsync([canvas] { PresentationWindow::close(canvas); });
            break;
        }
        default:
            break;
        }
    }
};
