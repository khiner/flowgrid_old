#include "CopiedTracks.h"

void CopiedTracks::loadFromState(const ValueTree &fromState) {
    state = ValueTree(getIdentifier());
    for (const auto &track : fromState) {
        auto copiedTrack = ValueTree(TrackStateIDs::TRACK);
        if (track[TracksStateIDs::selected]) {
            copiedTrack.copyPropertiesFrom(track, nullptr);
            for (auto processor : track)
                if (processor.hasType(ProcessorStateIDs::PROCESSOR))
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
        state.appendChild(copiedTrack, nullptr);
    }
}
