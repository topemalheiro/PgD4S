/*
 // Copyright (c) 2026 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel.h"
#include "Utility/SettingsFile.h"
#include "Utility/NVGUtils.h"
#include "Components/Buttons.h"

class PluginProcessor;
class LatencyDisplayButton;
class ZoomLabel;
class StatusbarButtonGroup;
class EditModeButton;
class PresentationModeButton;

class Statusbar;

class Statusbar final : public Component
    , public NVGComponent
    , public AsyncUpdater {
public:
    Statusbar(PluginProcessor* processor, PluginEditor* e);

    ~Statusbar() override;

    void updateZoomLevel() { triggerAsyncUpdate(); }

    void setEditButtonState(bool locked, bool present = false);
    void setDetectiveModeEnabled(bool enabled);

    static constexpr int statusbarHeight = 30;
    float currentZoomLevel = 100.0f;

    // Widths used by resized(), so the floating status bar can be made wide enough for all its items
    static constexpr int margin = 6;
    static constexpr int spacing = 10;
    static constexpr int editModeWidth = 78; // icon, name ("Edit" or "Run") and chevron
    static constexpr int presentationGap = 4;
    static constexpr int presentationWidth = 22;
    static constexpr int zoomWidth = 55;
    static constexpr int gridWidth = 34;
    static constexpr int overlayWidth = 34;
    static constexpr int detectiveButtonWidth = 64;
    static constexpr int detectiveGap = 4;
    static constexpr int detectiveSliderWidth = 120;

    // Width that fits every item, with a margin on both sides (the floating status bar's width)
    static constexpr int getDesiredWidth()
    {
        return margin + zoomWidth + spacing + gridWidth + spacing + overlayWidth + spacing + detectiveButtonWidth + detectiveGap
            + detectiveSliderWidth + spacing + presentationWidth + presentationGap + editModeWidth + margin;
    }

private:
    void updateCachedRenderingMode();

    void handleAsyncUpdate() override;

    void paint(Graphics& g) override;

    void resized() override;

    PluginProcessor* pd;
    PluginEditor* editor;

    std::unique_ptr<ZoomLabel> zoomSelector;
    std::unique_ptr<StatusbarButtonGroup> gridGroup;
    std::unique_ptr<StatusbarButtonGroup> overlayGroup;
    std::unique_ptr<EditModeButton> editModeGroup;
    std::unique_ptr<PresentationModeButton> presentationButton; // PgD4S: one click to presentation mode, as in Max
    std::unique_ptr<TextButton> detectiveModeButton;
    std::unique_ptr<Slider> detectiveSpeedSlider;

    bool cachedRenderingEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Statusbar)
};
