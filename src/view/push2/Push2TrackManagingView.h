#pragma once

#include "model/Project.h"
#include "Push2ComponentBase.h"
#include "Push2Label.h"

class Push2TrackManagingView : public Push2ComponentBase, protected ValueTree::Listener, protected Tracks::Listener {
public:
    explicit Push2TrackManagingView(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2);

    ~Push2TrackManagingView() override;

    void resized() override;

    void belowScreenButtonPressed(int buttonIndex) override;

    void updateEnabledPush2Buttons() override;

protected:
    void trackAdded(Track *track) override { updateEnabledPush2Buttons(); }
    void trackRemoved(Track *track, int oldIndex) override { updateEnabledPush2Buttons(); }
    void trackOrderChanged() override { updateEnabledPush2Buttons(); }

    virtual void trackSelected(Track *track) { updateEnabledPush2Buttons(); }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

protected:
    Project &project;

    void trackColourChanged(const String &trackUuid, const Colour &colour) override;

private:
    OwnedArray<Push2Label> trackLabels{};
};
