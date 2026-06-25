/*
 // Copyright (c) 2021-2025 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#pragma once

#include "Utility/Config.h"

class Connection;

class ConnectionDebugPanel final : public Component
    , private Timer {
public:
    ConnectionDebugPanel();
    ~ConnectionDebugPanel() override = default;

    void paint(Graphics& g) override;
    void resized() override;

    void updateConnection(Connection* connection);

private:
    void timerCallback() override;
    void refreshHistory();

    Label titleLabel;
    Slider speedSlider;
    Label speedLabel;
    TextButton playPauseButton;
    TextButton stepButton;
    ListBox historyList;

    class HistoryListModel final : public ListBoxModel {
    public:
        void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
        int getNumRows() override { return static_cast<int>(items.size()); }
        void setItems(StringArray newItems);

    private:
        StringArray items;
    };

    HistoryListModel historyModel;

    SafePointer<Connection> currentConnection;
    bool isPlaying = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionDebugPanel)
};
