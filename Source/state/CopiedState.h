#pragma once

#include "TracksState.h"
#include "ConnectionsState.h"

struct CopiedState : public Stateful {
    CopiedState(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), connections(connections), audioProcessorContainer(audioProcessorContainer) {}

    ValueTree& getState() override { return copiedItems; }

    void loadFromState(const ValueTree &tracksState) override {
        copiedItems = ValueTree(IDs::TRACKS);
        for (const auto &track : tracksState) {
            auto copiedTrack = ValueTree(IDs::TRACK);
            if (track[IDs::selected]) {
                copiedTrack.copyPropertiesFrom(track, nullptr);
            } else {
                copiedTrack.setProperty(IDs::selectedSlotsMask, track[IDs::selectedSlotsMask].toString(), nullptr);
            }
            for (auto processor : track) {
                if (track[IDs::selected] || tracks.isProcessorSelected(processor)) {
                    copiedTrack.appendChild(audioProcessorContainer.copyProcessor(processor), nullptr);
                }
            }
            copiedItems.appendChild(copiedTrack, nullptr);
        }
    }

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    TracksState &tracks;
    ConnectionsState &connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    ValueTree copiedItems;
};
