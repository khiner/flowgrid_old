#include "CreateTrack.h"

// NOTE: assumes the track hasn't been added yet!
String makeTrackNameUnique(const Tracks &tracks, const String &trackName) {
    for (const auto *track : tracks.getChildren()) {
        const auto &otherTrackName = track->getName();
        if (otherTrackName == trackName) {
            if (trackName.contains("-")) {
                int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                if (i != 0)
                    return makeTrackNameUnique(tracks, trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
            } else {
                return makeTrackNameUnique(tracks, trackName + "-" + String(1));
            }
        }
    }
    return trackName;
}

static int calculateInsertIndex(bool isMaster, const Track *derivedFromTrack, Tracks &tracks) {
    if (isMaster) return tracks.getNumNonMasterTracks();
    return derivedFromTrack != nullptr ? derivedFromTrack->getIndex() : tracks.getNumNonMasterTracks();
}

CreateTrack::CreateTrack(bool isMaster, const Track *derivedFromTrack, Tracks &tracks, View &view)
        : CreateTrack(calculateInsertIndex(isMaster, derivedFromTrack, tracks), isMaster, derivedFromTrack, tracks, view) {}

CreateTrack::CreateTrack(int insertIndex, bool isMaster, const Track *derivedFromTrack, Tracks &tracks, View &view)
        : insertIndex(insertIndex), tracks(tracks) {
    // TODO move into method and construct in initializer list
    newTrack.setMaster(isMaster);
    newTrack.setUuid(Uuid().toString());
    bool isSubTrack = !isMaster && derivedFromTrack != nullptr;
    const String name = isMaster ? "Master" : (isSubTrack ? makeTrackNameUnique(tracks, derivedFromTrack->getName()) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)));
    newTrack.setName(name);
    newTrack.setColour(isSubTrack ? derivedFromTrack->getColour() : Colour::fromHSV((1.0f / 8.0f) * tracks.size(), 0.65f, 0.65f, 1.0f));
    newTrack.setSelected(false);

    auto lanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
    auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
    ProcessorLane::setSelectedSlotsMask(lane, BigInteger());
    lanes.appendChild(lane, nullptr);
    newTrack.getState().appendChild(lanes, nullptr);
}

bool CreateTrack::perform() {
    tracks.add(newTrack.getState(), insertIndex);
    return true;
}

bool CreateTrack::undo() {
    tracks.remove(newTrack.getState());
    return true;
}
