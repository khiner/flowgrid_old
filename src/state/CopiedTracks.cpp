#include "CopiedTracks.h"

void CopiedTracks::loadFromState(const ValueTree &fromState) {
    state = ValueTree(getIdentifier());
    for (const auto &track : fromState) {
        auto copiedTrack = ValueTree(TrackStateIDs::TRACK);
        if (track[TrackStateIDs::selected]) {
            copiedTrack.copyPropertiesFrom(track, nullptr);
            for (auto processor : track)
                if (processor.hasType(ProcessorStateIDs::PROCESSOR))
                    copiedTrack.appendChild(processorGraph.copyProcessor(processor), nullptr);
        } else {
            copiedTrack.setProperty(TrackStateIDs::isMasterTrack, track[TrackStateIDs::isMasterTrack], nullptr);
        }

        auto copiedLanes = ValueTree(ProcessorLanesStateIDs::PROCESSOR_LANES);
        for (const auto &lane : TracksState::getProcessorLanesForTrack(track)) {
            auto copiedLane = ValueTree(ProcessorLaneStateIDs::PROCESSOR_LANE);
            copiedLane.setProperty(ProcessorLaneStateIDs::selectedSlotsMask, lane[ProcessorLaneStateIDs::selectedSlotsMask].toString(), nullptr);
            for (auto processor : lane)
                if (track[TrackStateIDs::selected] || TracksState::isProcessorSelected(processor))
                    copiedLane.appendChild(processorGraph.copyProcessor(processor), nullptr);
            copiedLanes.appendChild(copiedLane, nullptr);
        }

        copiedTrack.appendChild(copiedLanes, nullptr);
        state.appendChild(copiedTrack, nullptr);
    }
}
