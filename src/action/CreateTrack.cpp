#include "CreateTrack.h"

// NOTE: assumes the track hasn't been added yet!
String makeTrackNameUnique(Tracks &tracks, const String &trackName) {
    for (const auto &track : tracks.getState()) {
        String otherTrackName = Track::getName(track);
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

static int calculateInsertIndex(bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks) {
    if (isMaster) return tracks.getNumNonMasterTracks();
    return derivedFromTrack.isValid() ? derivedFromTrack.getParent().indexOf(derivedFromTrack) : tracks.getNumNonMasterTracks();
}

CreateTrack::CreateTrack(bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view)
        : CreateTrack(calculateInsertIndex(isMaster, derivedFromTrack, tracks), isMaster, derivedFromTrack, tracks, view) {}

CreateTrack::CreateTrack(int insertIndex, bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view)
        : insertIndex(insertIndex), tracks(tracks) {
    // TODO move into method and construct in initializer list
    newTrack = ValueTree(TrackIDs::TRACK);
    Track::setMaster(newTrack, isMaster);
    Track::setUuid(newTrack, Uuid().toString());
    bool isSubTrack = !isMaster && derivedFromTrack.isValid();
    const String name = isMaster ? "Master" : (isSubTrack ? makeTrackNameUnique(tracks, Track::getName(derivedFromTrack)) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)));
    Track::setName(newTrack, name);
    Track::setColour(newTrack, isSubTrack ? Track::getColour(derivedFromTrack) : Colour::fromHSV((1.0f / 8.0f) * tracks.getNumTracks(), 0.65f, 0.65f, 1.0f));
    Track::setSelected(newTrack, false);

    auto lanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
    auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
    lane.setProperty(ProcessorLaneIDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
    lanes.appendChild(lane, nullptr);
    newTrack.appendChild(lanes, nullptr);
}

bool CreateTrack::perform() {
    tracks.getState().addChild(newTrack, insertIndex, nullptr);
    return true;
}

bool CreateTrack::undo() {
    tracks.getState().removeChild(newTrack, nullptr);
    return true;
}
