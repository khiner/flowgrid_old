#include "CreateTrack.h"

// NOTE: assumes the track hasn't been added yet!
String makeTrackNameUnique(Tracks &tracks, const String &trackName) {
    for (const auto &track : tracks.getState()) {
        String otherTrackName = track[TrackIDs::name];
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
    newTrack.setMaster(isMaster);
    newTrack.setUuid(Uuid().toString());
    bool isSubTrack = !isMaster && derivedFromTrack.isValid();
    if (isMaster) {
        newTrack.setName("Master");
    } else {
        newTrack.setName(isSubTrack ? makeTrackNameUnique(tracks, derivedFromTrack[TrackIDs::name]) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)));
    }
    newTrack.setColour(isSubTrack ? derivedFromTrack[TrackIDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * tracks.getNumTracks(), 0.65f, 0.65f, 1.0f).toString());
    newTrack.setSelected(false);

    auto lane = ProcessorLane();
    lane.setSelectedSlots(BigInteger());
    auto lanes = ProcessorLanes();
    lanes.addLane(lane);
    newTrack.addLanes(lanes);
}

bool CreateTrack::perform() {
    tracks.addTrack(newTrack, insertIndex);
    return true;
}

bool CreateTrack::undo() {
    tracks.removeTrack(newTrack);
    return true;
}
