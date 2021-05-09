#include "CopiedState.h"

void CopiedState::loadFromState(const ValueTree &tracksState) {
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

        auto copiedLanes = ValueTree(IDs::PROCESSOR_LANES);
        for (const auto &lane : TracksState::getProcessorLanesForTrack(track)) {
            auto copiedLane = ValueTree(IDs::PROCESSOR_LANE);
            copiedLane.setProperty(IDs::selectedSlotsMask, lane[IDs::selectedSlotsMask].toString(), nullptr);
            for (auto processor : lane)
                if (track[IDs::selected] || tracks.isProcessorSelected(processor))
                    copiedLane.appendChild(audioProcessorContainer.copyProcessor(processor), nullptr);
            copiedLanes.appendChild(copiedLane, nullptr);
        }

        copiedTrack.appendChild(copiedLanes, nullptr);
        copiedItems.appendChild(copiedTrack, nullptr);
    }
}
