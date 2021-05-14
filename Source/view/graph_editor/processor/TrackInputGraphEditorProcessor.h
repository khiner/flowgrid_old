#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "state/Project.h"
#include "BaseGraphEditorProcessor.h"

class TrackInputGraphEditorProcessor : public BaseGraphEditorProcessor, private ChangeListener {
public:
    TrackInputGraphEditorProcessor(const ValueTree &state, ViewState &view, TracksState &tracks, Project &project, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener);

    ~TrackInputGraphEditorProcessor() override;

    String getTrackName() const { return getTrack()[IDs::name].toString(); }

    void setTrackName(const String &trackName) {
        nameLabel.setText(trackName, dontSendNotification);
    }

    void mouseDown(const MouseEvent &e) override;

    void mouseUp(const MouseEvent &e) override;

    void resized() override;

    void paint(Graphics &g) override;

    bool isInView() override { return true; }

    Rectangle<int> getBoxBounds() const override { return getLocalBounds(); }

private:
    Project &project;
    Label nameLabel;
    std::unique_ptr<ImageButton> audioMonitorToggle, midiMonitorToggle;

    void colourChanged() override { repaint(); }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;

    void changeListenerCallback(ChangeBroadcaster *source) override;
};
