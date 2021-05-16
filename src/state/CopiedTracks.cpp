#include "CopiedTracks.h"

void CopiedTracks::loadFromState(const ValueTree &fromState) {
    state = ValueTree(getIdentifier());
    for (const auto &track : fromState) {
        auto copiedTrack = ValueTree(TrackIDs::TRACK);
        if (track[TrackIDs::selected]) {
            copiedTrack.copyPropertiesFrom(track, nullptr);
            for (auto processor : track)
                if (processor.hasType(ProcessorIDs::PROCESSOR))
                    copiedTrack.appendChild(processorGraph.copyProcessor(processor), nullptr);
        } else {
            copiedTrack.setProperty(TrackIDs::isMasterTrack, track[TrackIDs::isMasterTrack], nullptr);
        }

        auto copiedLanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
        for (const auto &lane : Tracks::getProcessorLanesForTrack(track)) {
            auto copiedLane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
            copiedLane.setProperty(ProcessorLaneIDs::selectedSlotsMask, lane[ProcessorLaneIDs::selectedSlotsMask].toString(), nullptr);
            for (auto processor : lane)
                if (track[TrackIDs::selected] || Tracks::isProcessorSelected(processor))
                    copiedLane.appendChild(processorGraph.copyProcessor(processor), nullptr);
            copiedLanes.appendChild(copiedLane, nullptr);
        }

        copiedTrack.appendChild(copiedLanes, nullptr);
        state.appendChild(copiedTrack, nullptr);
    }
}
