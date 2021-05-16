#include "CreateTrackAction.h"

// NOTE: assumes the track hasn't been added yet!
String makeTrackNameUnique(TracksState &tracks, const String &trackName) {
    for (const auto &track : tracks.getState()) {
        String otherTrackName = track[TracksStateIDs::name];
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

static int calculateInsertIndex(bool isMaster, const ValueTree &derivedFromTrack, TracksState &tracks) {
    if (isMaster) return tracks.getNumNonMasterTracks();
    return derivedFromTrack.isValid() ? derivedFromTrack.getParent().indexOf(derivedFromTrack) : tracks.getNumNonMasterTracks();
}

CreateTrackAction::CreateTrackAction(bool isMaster, const ValueTree &derivedFromTrack, TracksState &tracks, ViewState &view)
        : CreateTrackAction(calculateInsertIndex(isMaster, derivedFromTrack, tracks), isMaster, derivedFromTrack, tracks, view) {}

CreateTrackAction::CreateTrackAction(int insertIndex, bool isMaster, const ValueTree &derivedFromTrack, TracksState &tracks, ViewState &view)
        : insertIndex(insertIndex), tracks(tracks) {
    // TODO move into method and construct in initializer list
    newTrack = ValueTree(TrackStateIDs::TRACK);
    newTrack.setProperty(TracksStateIDs::isMasterTrack, isMaster, nullptr);
    newTrack.setProperty(TracksStateIDs::uuid, Uuid().toString(), nullptr);
    bool isSubTrack = !isMaster && derivedFromTrack.isValid();
    if (isMaster) {
        newTrack.setProperty(TracksStateIDs::name, "Master", nullptr);
    } else {
        newTrack.setProperty(TracksStateIDs::name, isSubTrack ? makeTrackNameUnique(tracks, derivedFromTrack[TracksStateIDs::name]) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)), nullptr);
    }
    newTrack.setProperty(TracksStateIDs::colour, isSubTrack ? derivedFromTrack[TracksStateIDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * tracks.getNumTracks(), 0.65f, 0.65f, 1.0f).toString(), nullptr);
    newTrack.setProperty(TracksStateIDs::selected, false, nullptr);

    auto lanes = ValueTree(TracksStateIDs::PROCESSOR_LANES);
    auto lane = ValueTree(TracksStateIDs::PROCESSOR_LANE);
    lane.setProperty(TracksStateIDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
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
