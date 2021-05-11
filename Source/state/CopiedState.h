#pragma once

#include "TracksState.h"
#include "StatefulAudioProcessorContainer.h"

struct CopiedState : public Stateful {
    CopiedState(TracksState &tracks, StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), audioProcessorContainer(audioProcessorContainer) {}

    ValueTree &getState() override { return copiedItems; }

    void loadFromState(const ValueTree &tracksState) override;

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    TracksState &tracks;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    ValueTree copiedItems;
};
