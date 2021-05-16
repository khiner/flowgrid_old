#include "CreateTrackAction.h"

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

CreateTrackAction::CreateTrackAction(bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view)
        : CreateTrackAction(calculateInsertIndex(isMaster, derivedFromTrack, tracks), isMaster, derivedFromTrack, tracks, view) {}

CreateTrackAction::CreateTrackAction(int insertIndex, bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view)
        : insertIndex(insertIndex), tracks(tracks) {
    // TODO move into method and construct in initializer list
    newTrack = ValueTree(TrackIDs::TRACK);
    newTrack.setProperty(TrackIDs::isMasterTrack, isMaster, nullptr);
    newTrack.setProperty(TrackIDs::uuid, Uuid().toString(), nullptr);
    bool isSubTrack = !isMaster && derivedFromTrack.isValid();
    if (isMaster) {
        newTrack.setProperty(TrackIDs::name, "Master", nullptr);
    } else {
        newTrack.setProperty(TrackIDs::name, isSubTrack ? makeTrackNameUnique(tracks, derivedFromTrack[TrackIDs::name]) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)), nullptr);
    }
    newTrack.setProperty(TrackIDs::colour, isSubTrack ? derivedFromTrack[TrackIDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * tracks.getNumTracks(), 0.65f, 0.65f, 1.0f).toString(), nullptr);
    newTrack.setProperty(TrackIDs::selected, false, nullptr);

    auto lanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
    auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
    lane.setProperty(ProcessorLaneIDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
    lanes.appendChild(lane, nullptr);
    newTrack.appendChild(lanes, nullptr);
}

bool CreateTrackAction::perform() {
    tracks.getState().addChild(newTrack, insertIndex, nullptr);
    return true;
}

bool CreateTrackAction::undo() {
    tracks.getState().removeChild(newTrack, nullptr);
    return true;
}
