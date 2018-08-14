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

    void resized() override {
        auto r = getLocalBounds();
        auto labelWidth = getWidth() / NUM_COLUMNS;
        auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
        for (auto* trackLabel : trackLabels) {
            trackLabel->setBounds(bottom.removeFromLeft(labelWidth));
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        auto track = project.getNonMasterTrack(buttonIndex);
        if (track.isValid()) {
            track.setProperty(IDs::selected, true, nullptr);
        }
    }

    void updateEnabledPush2Buttons() override {
        auto selectedTrack = project.getSelectedTrack();
        const auto& masterTrack = project.getMasterTrack();
        for (auto* label : trackLabels) {
            label->setVisible(false);
        }
        if (!isVisible())
            return;
        int labelIndex = 0;
        for (const auto& track : project.getTracks()) {
            if (track == masterTrack)
                continue;
            if (labelIndex >= trackLabels.size())
                return;
            auto *label = trackLabels.getUnchecked(labelIndex++);
            // TODO left/right buttons
            label->setVisible(true);
            label->setMainColour(Colour::fromString(track[IDs::colour].toString()));
            label->setText(track[IDs::name], dontSendNotification);
            label->setSelected(track == selectedTrack);
        }
    }

protected:
    virtual void trackAdded(const ValueTree &track) { updateEnabledPush2Buttons(); }
    virtual void trackRemoved(const ValueTree &track) { updateEnabledPush2Buttons(); }
    virtual void trackSelected(const ValueTree &track) { updateEnabledPush2Buttons(); }

    virtual void selectedTrackColourChanged(const Colour& colour) = 0;

    void valueTreeChildAdded(ValueTree &parent, ValueTree& child) override {
        if (child.hasType(IDs::MASTER_TRACK) || child.hasType(IDs::TRACK)) {
            trackAdded(child);
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree& child, int index) override {
        if (child.hasType(IDs::MASTER_TRACK) || child.hasType(IDs::TRACK)) {
            trackRemoved(child);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::MASTER_TRACK) || tree.hasType(IDs::TRACK)) {
            if (i == IDs::name || i == IDs::colour) {
                int trackIndex = tree.getParent().indexOf(tree);
                if (trackIndex == -1)
                    return;
                jassert(trackIndex < trackLabels.size()); // TODO left/right buttons
                if (i == IDs::name && !tree.hasType(IDs::MASTER_TRACK)) {
                    trackLabels.getUnchecked(trackIndex)->setText(tree[IDs::name], dontSendNotification);
                } else if (i == IDs::colour) {
                    const auto &trackColour = Colour::fromString(tree[IDs::colour].toString());
                    if (!tree.hasType(IDs::MASTER_TRACK)) {
                        trackLabels.getUnchecked(trackIndex)->setMainColour(trackColour);
                    }
                    if (tree == project.getSelectedTrack()) {
                        selectedTrackColourChanged(trackColour);
                    }
                }
            } else if (i == IDs::selected && tree[IDs::selected]) {
                trackSelected(tree);
            }
        }
    }

private:
    OwnedArray<Push2Label> trackLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2TrackManagingView)
};
