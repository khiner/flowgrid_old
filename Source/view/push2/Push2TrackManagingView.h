#pragma once

#include "JuceHeader.h"
#include "Push2ComponentBase.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2TrackManagingView : public Push2ComponentBase, protected Utilities::ValueTreePropertyChangeListener {
public:
    explicit Push2TrackManagingView(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        project.getState().addListener(this);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            addChildComponent(trackLabels.add(new Push2Label(i, false, push2MidiCommunicator)));
        }
    }

    ~Push2TrackManagingView() override {
        project.getState().removeListener(this);
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        push2.enableWhiteLedButton(Push2::master);
        if (visible)
            updateLabels();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto labelWidth = getWidth() / NUM_COLUMNS;
        auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
        for (auto* trackLabel : trackLabels) {
            trackLabel->setBounds(bottom.removeFromLeft(labelWidth));
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        selectTrack(buttonIndex);
    }

protected:

    virtual void selectedTrackColourChanged(const Colour& colour) = 0;

    virtual void emptyTrackSelected(const ValueTree& emptyTrack) {
        updateLabels();
    }

    virtual void updateLabels() {
        auto selectedTrack = project.getSelectedTrack();
        if (!selectedTrack.isValid())
            return;

        for (int i = 0; i < trackLabels.size(); i++) {
            auto *label = trackLabels.getUnchecked(i);
            // TODO left/right buttons
            if (i < project.getNumNonMasterTracks()) {
                const auto &track = project.getTrack(i);
                label->setVisible(true);
                label->setMainColour(Colour::fromString(track[IDs::colour].toString()));
                label->setText(track[IDs::name], dontSendNotification);
                label->setSelected(track == selectedTrack);
            } else {
                label->setVisible(false);
            }
        }

        if (!project.getMasterTrack().isValid())
            push2.disableWhiteLedButton(Push2::master);
        else if (selectedTrack != project.getMasterTrack())
            push2.enableWhiteLedButton(Push2::master);
        else
            push2.activateWhiteLedButton(Push2::master);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::MASTER_TRACK) || tree.hasType(IDs::TRACK)) {
            if (i == IDs::name || i == IDs::colour) {
                int trackIndex = tree.getParent().indexOf(tree);
                if (trackIndex == -1)
                    return;
                jassert(trackIndex < trackLabels.size()); // TODO left/right buttons
                if (i == IDs::name) {
                    trackLabels.getUnchecked(trackIndex)->setText(tree[IDs::name], dontSendNotification);
                } else if (i == IDs::colour) {
                    const auto &trackColour = Colour::fromString(tree[IDs::colour].toString());
                    trackLabels.getUnchecked(trackIndex)->setMainColour(trackColour);
                    if (tree == project.getSelectedTrack()) {
                        selectedTrackColourChanged(trackColour);
                    }
                }
            } else if (i == IDs::selected && tree[IDs::selected]) {
                if (tree.getNumChildren() == 0) { // TODO manage this on its own
                    emptyTrackSelected(tree);
                }
            }
        }
    }

private:
    OwnedArray<Push2Label> trackLabels;

    void selectTrack(int trackIndex) {
        if (trackIndex < project.getNumNonMasterTracks()) {
            project.getTrack(trackIndex).setProperty(IDs::selected, true, nullptr);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2TrackManagingView)
};
