#include "CopiedState.h"

void CopiedState::loadFromState(const ValueTree &tracksState) {
    copiedItems = ValueTree(TracksStateIDs::TRACKS);
    for (const auto &track : tracksState) {
        auto copiedTrack = ValueTree(TracksStateIDs::TRACK);
        if (track[TracksStateIDs::selected]) {
            copiedTrack.copyPropertiesFrom(track, nullptr);
            for (auto processor : track)
                if (processor.hasType(TracksStateIDs::PROCESSOR))
                    copiedTrack.appendChild(processorGraph.copyProcessor(processor), nullptr);
        } else {
            copiedTrack.setProperty(TracksStateIDs::isMasterTrack, track[TracksStateIDs::isMasterTrack], nullptr);
        }

        auto copiedLanes = ValueTree(TracksStateIDs::PROCESSOR_LANES);
        for (const auto &lane : TracksState::getProcessorLanesForTrack(track)) {
            auto copiedLane = ValueTree(TracksStateIDs::PROCESSOR_LANE);
            copiedLane.setProperty(TracksStateIDs::selectedSlotsMask, lane[TracksStateIDs::selectedSlotsMask].toString(), nullptr);
            for (auto processor : lane)
                if (track[TracksStateIDs::selected] || TracksState::isProcessorSelected(processor))
                    copiedLane.appendChild(processorGraph.copyProcessor(processor), nullptr);
            copiedLanes.appendChild(copiedLane, nullptr);
        }

        copiedTrack.appendChild(copiedLanes, nullptr);
        copiedItems.appendChild(copiedTrack, nullptr);
    }
}
