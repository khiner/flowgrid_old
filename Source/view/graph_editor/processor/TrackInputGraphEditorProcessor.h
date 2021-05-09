#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "BaseGraphEditorProcessor.h"

class TrackInputGraphEditorProcessor : public BaseGraphEditorProcessor, private ChangeListener {
public:
    TrackInputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener);

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

    Rectangle<int> getBoxBounds() override {
        return getLocalBounds();
    }

private:
    Project &project;
    TracksState &tracks;
    Label nameLabel;
    std::unique_ptr<ImageButton> audioMonitorToggle, midiMonitorToggle;

    void colourChanged() override { repaint(); }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;

    void changeListenerCallback(ChangeBroadcaster *source) override;
};
