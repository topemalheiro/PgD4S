/*
 // PgD4S (plugdata for Spite): a subpatch marked "open in presentation" (Max's openinpresentation,
 // "#X presentation open 1"), opened by [pcontrol] or [thispatcher]: its own window showing its
 // presentation at 100%, staying on top when marked "#X presentation float 1" (Max's window flags float).
 // The window holds a second editor of the same plugin instance, in plugin mode on that subpatch.
 */
#pragma once

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "TabComponent.h"
#include "Pd/Patch.h"

class PresentationWindow final : public DocumentWindow {
public:
    static void open(PluginProcessor* pd, t_canvas* canvas)
    {
        if (auto* window = find(canvas)) {
            window->toFront(true);
            return;
        }
        getWindows().add(new PresentationWindow(pd, canvas));
    }

    static void close(t_canvas const* canvas)
    {
        if (auto* window = find(canvas))
            getWindows().removeObject(window);
    }

    static bool isOpen(t_canvas const* canvas)
    {
        return find(canvas) != nullptr;
    }

    // The canvas's window size or flags changed ([thispatcher] window exec): show it again with them
    static void refresh(PluginProcessor* pd, t_canvas* canvas)
    {
        if (find(canvas)) {
            close(canvas);
            open(pd, canvas);
        }
    }

    static void closeAll(PluginProcessor const* pd)
    {
        auto& windows = getWindows();
        for (int i = windows.size() - 1; i >= 0; i--) {
            if (windows[i]->processor == pd)
                windows.remove(i);
        }
    }

    void closeButtonPressed() override
    {
        // not from inside our own callback
        MessageManager::callAsync([canvas = canvasPtr] { close(canvas); });
    }

    ~PresentationWindow() override
    {
        clearContentComponent();
        editor.reset();

        // An editor going away blocks Pd's messages to the GUI; the instance's other editors still want them
        if (processor->getActiveEditor())
            processor->messageDispatcher->setBlockMessages(false);
    }

private:
    PresentationWindow(PluginProcessor* pd, t_canvas* canvas)
        : DocumentWindow(String::fromUTF8(canvas->gl_name ? canvas->gl_name->s_name : "Presentation"), Colours::black, DocumentWindow::closeButton, true)
        , processor(pd)
        , canvasPtr(canvas)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);

        editor = std::make_unique<PluginEditor>(*pd);
        editor->presentationWindow = true;
        pd::Patch::Ptr const patch = new pd::Patch(pd::WeakReference(canvas, pd), pd, false);
        patch->windowIndex = editor->editorIndex;
        editor->presentationWindowPatch = patch;

        setContentNonOwned(editor.get(), true);
        setAlwaysOnTop(canvas->gl_presentation_float);
        centreWithSize(std::max(getWidth(), 100), std::max(getHeight(), 60));
        setVisible(true);

        editor->getTabComponent().updateNow();
        pd->messageDispatcher->setBlockMessages(false);
    }

    static OwnedArray<PresentationWindow>& getWindows()
    {
        static OwnedArray<PresentationWindow> windows;
        return windows;
    }

    static PresentationWindow* find(t_canvas const* canvas)
    {
        for (auto* window : getWindows()) {
            if (window->canvasPtr == canvas)
                return window;
        }
        return nullptr;
    }

    PluginProcessor* processor;
    t_canvas* canvasPtr;
    std::unique_ptr<PluginEditor> editor;
};
