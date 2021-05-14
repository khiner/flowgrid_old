#pragma once

#include "state/Project.h"
#include "Push2ComponentBase.h"
#include "Push2Label.h"

class Push2TrackManagingView : public Push2ComponentBase, protected ValueTree::Listener {
public:
    explicit Push2TrackManagingView(ViewState &view, TracksState &tracks, Project &project, Push2MidiCommunicator &push2);

    ~Push2TrackManagingView() override;

    void resized() override;

    void belowScreenButtonPressed(int buttonIndex) override;

    void updateEnabledPush2Buttons() override;

protected:
    virtual void trackAdded(const ValueTree &track) { updateEnabledPush2Buttons(); }

    virtual void trackRemoved(const ValueTree &track) { updateEnabledPush2Buttons(); }

    virtual void trackSelected(const ValueTree &track) { updateEnabledPush2Buttons(); }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) override;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override;

protected:
    Project &project;

    void trackColourChanged(const String &trackUuid, const Colour &colour) override;

private:
    OwnedArray<Push2Label> trackLabels{};
};
