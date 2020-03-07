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
            ValueTree copiedTrack;
            if (track[IDs::selected]) {
                copiedTrack = track.createCopy();
            } else {
                copiedTrack = ValueTree(IDs::TRACK);
                copiedTrack.setProperty(IDs::selectedSlotsMask, bool(track[IDs::selectedSlotsMask]), nullptr);
                for (auto processor : track) {
                    if (tracks.isProcessorSelected(processor)) {
                        copiedTrack.appendChild(audioProcessorContainer.duplicateProcessor(processor), nullptr);
                    }
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
