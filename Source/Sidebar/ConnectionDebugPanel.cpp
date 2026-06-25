/*
 // Copyright (c) 2021-2025 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "ConnectionDebugPanel.h"

#include "Connection.h"
#include "LookAndFeel.h"
#include "Utility/Fonts.h"

ConnectionDebugPanel::ConnectionDebugPanel()
    : titleLabel("Debug Messages", "Debug Messages")
    , speedSlider(Slider::LinearHorizontal, Slider::TextBoxLeft)
    , speedLabel("Speed:", "Speed:")
    , playPauseButton("Pause")
    , stepButton("Step")
{
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(Fonts::getBoldFont().withHeight(16));
    titleLabel.setColour(Label::textColourId, PlugDataColours::panelTextColour);

    addAndMakeVisible(speedLabel);
    speedLabel.setFont(Fonts::getDefaultFont().withHeight(13));
    speedLabel.setColour(Label::textColourId, PlugDataColours::panelTextColour);

    addAndMakeVisible(speedSlider);
    speedSlider.setRange(0.1, 5.0, 0.1);
    speedSlider.setValue(Connection::getDebugSpeedMultiplier(), dontSendNotification);
    speedSlider.onValueChange = [this] {
        Connection::setDebugSpeedMultiplier(static_cast<float>(speedSlider.getValue()));
    };

    addAndMakeVisible(playPauseButton);
    playPauseButton.onClick = [this] {
        isPlaying = !isPlaying;
        Connection::setDebugPaused(!isPlaying);
        playPauseButton.setButtonText(isPlaying ? "Pause" : "Play");
    };

    addAndMakeVisible(stepButton);
    stepButton.onClick = [] {
        Connection::debugStep();
    };

    addAndMakeVisible(historyList);
    historyList.setModel(&historyModel);
    historyList.setColour(ListBox::backgroundColourId, Colours::transparentBlack);

    startTimerHz(10);
}

void ConnectionDebugPanel::paint(Graphics& g)
{
    g.fillAll(PlugDataColours::panelBackgroundColour);
}

void ConnectionDebugPanel::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    titleLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);

    auto controls = bounds.removeFromTop(28);
    playPauseButton.setBounds(controls.removeFromLeft(60));
    controls.removeFromLeft(4);
    stepButton.setBounds(controls.removeFromLeft(60));
    controls.removeFromLeft(12);
    speedLabel.setBounds(controls.removeFromLeft(44));
    speedSlider.setBounds(controls);

    bounds.removeFromTop(8);
    historyList.setBounds(bounds);
}

void ConnectionDebugPanel::updateConnection(Connection* connection)
{
    if (currentConnection == connection)
        return;

    currentConnection = connection;
    refreshHistory();
}

void ConnectionDebugPanel::timerCallback()
{
    refreshHistory();
}

void ConnectionDebugPanel::refreshHistory()
{
    StringArray items;

    if (currentConnection) {
        auto const& history = currentConnection->getDebugMessageHistory();
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            items.add("#" + String(it->sequenceNumber) + "  " + currentConnection->formatDebugMessage(*it));
        }
    }

    if (items.isEmpty()) {
        items.add("No messages recorded yet.");
    }

    historyModel.setItems(items);
    historyList.updateContent();
}

void ConnectionDebugPanel::HistoryListModel::paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected)
{
    ignoreUnused(rowIsSelected);
    if (!isPositiveAndBelow(rowNumber, items.size()))
        return;

    g.setColour(PlugDataColours::panelTextColour);
    g.setFont(Fonts::getDefaultFont().withHeight(13));
    g.drawFittedText(items[rowNumber], 4, 0, width - 8, height, Justification::centredLeft, 2);
}

void ConnectionDebugPanel::HistoryListModel::setItems(StringArray newItems)
{
    items = std::move(newItems);
}
