#include "CreateTrackAction.h"

// NOTE: assumes the track hasn't been added yet!
String makeTrackNameUnique(TracksState &tracks, const String &trackName) {
    for (const auto &track : tracks.getState()) {
        String otherTrackName = track[IDs::name];
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
    newTrack = ValueTree(IDs::TRACK);
    newTrack.setProperty(IDs::isMasterTrack, isMaster, nullptr);
    newTrack.setProperty(IDs::uuid, Uuid().toString(), nullptr);
    bool isSubTrack = !isMaster && derivedFromTrack.isValid();
    if (isMaster) {
        newTrack.setProperty(IDs::name, "Master", nullptr);
    } else {
        newTrack.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(tracks, derivedFromTrack[IDs::name]) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)), nullptr);
    }
    newTrack.setProperty(IDs::colour, isSubTrack ? derivedFromTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * tracks.getNumTracks(), 0.65f, 0.65f, 1.0f).toString(), nullptr);
    newTrack.setProperty(IDs::selected, false, nullptr);

    auto lanes = ValueTree(IDs::PROCESSOR_LANES);
    auto lane = ValueTree(IDs::PROCESSOR_LANE);
    lane.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
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
