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
                for (auto processor : track)
                    if (processor.hasType(IDs::PROCESSOR))
                        copiedTrack.appendChild(audioProcessorContainer.copyProcessor(processor), nullptr);
            } else {
                copiedTrack.setProperty(IDs::isMasterTrack, track[IDs::isMasterTrack], nullptr);
            }

            const ValueTree &lane = TracksState::getProcessorLaneForTrack(track);
            auto copiedLane = ValueTree(IDs::PROCESSOR_LANE);
            copiedLane.setProperty(IDs::selectedSlotsMask, lane[IDs::selectedSlotsMask].toString(), nullptr);
            for (auto processor : lane)
                if (track[IDs::selected] || tracks.isProcessorSelected(processor))
                    copiedLane.appendChild(audioProcessorContainer.copyProcessor(processor), nullptr);

            copiedTrack.appendChild(copiedLane, nullptr);
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
