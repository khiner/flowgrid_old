#pragma once

#include "model/Project.h"
#include "Push2ComponentBase.h"
#include "Push2Label.h"

class Push2TrackManagingView : public Push2ComponentBase, protected ValueTree::Listener, protected StatefulList<Track>::Listener {
public:
    explicit Push2TrackManagingView(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2);

    ~Push2TrackManagingView() override;

    void resized() override;

    void belowScreenButtonPressed(int buttonIndex) override;

    void updateEnabledPush2Buttons() override;

protected:
    void onChildAdded(Track *) override { updateEnabledPush2Buttons(); }
    void onChildRemoved(Track *, int oldIndex) override { updateEnabledPush2Buttons(); }
    void onChildChanged(Track *, const Identifier &i) override;
    void onOrderChanged() override { updateEnabledPush2Buttons(); }

    virtual void trackSelected(Track *track) { updateEnabledPush2Buttons(); }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == ViewIDs::gridTrackOffset) updateEnabledPush2Buttons();
    }

protected:
    Project &project;

    void trackColourChanged(const String &trackUuid, const Colour &colour) override;

private:
    OwnedArray<Push2Label> trackLabels{};
};
