#pragma once

#include "JuceHeader.h"
#include "Push2ComponentBase.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2TrackManagingView : public Push2ComponentBase, protected ValueTree::Listener {
public:
    explicit Push2TrackManagingView(Project &project, Push2MidiCommunicator &push2)
            : Push2ComponentBase(project, push2) {
        project.getState().addListener(this);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            addChildComponent(trackLabels.add(new Push2Label(i, false, push2)));
        }
    }

    ~Push2TrackManagingView() override {
        push2.getPush2Colours().removeListener(this);
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
        auto track = project.getTrack(buttonIndex);
        if (track.isValid() && !track.hasType(IDs::MASTER_TRACK)) {
            track.setProperty(IDs::selected, true, nullptr);
        }
    }

    void updateEnabledPush2Buttons() override {
        auto selectedTrack = project.getSelectedTrack();
        for (auto* label : trackLabels) {
            label->setVisible(false);
        }
        if (!isVisible())
            return;
        int labelIndex = 0;
        for (int i = 0; i < jmin(trackLabels.size(), project.getNumNonMasterTracks()); i++) {
            auto *label = trackLabels.getUnchecked(labelIndex++);
            const auto& track = project.getTrack(i);
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
                }
            } else if (i == IDs::selected && tree[IDs::selected]) {
                trackSelected(tree);
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {
        if (tree.hasType(IDs::TRACKS)) {
            updateEnabledPush2Buttons();
        }
    }

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

protected:
    void trackColourChanged(const String &trackUuid, const Colour &colour) override {
        auto track = project.findTrackWithUuid(trackUuid);
        if (!track.hasType(IDs::MASTER_TRACK)) {
            trackLabels.getUnchecked(track.getParent().indexOf(track))->setMainColour(colour);
        }

    }

private:
    OwnedArray<Push2Label> trackLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2TrackManagingView)
};
