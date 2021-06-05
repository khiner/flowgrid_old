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

static int calculateInsertIndex(bool isMaster, int derivedFromTrackIndex, Tracks &tracks) {
    if (isMaster) return tracks.getNumNonMasterTracks();
    return derivedFromTrackIndex != -1 ? derivedFromTrackIndex : tracks.getNumNonMasterTracks();
}

CreateTrack::CreateTrack(bool isMaster, int derivedFromTrackIndex, Tracks &tracks, View &view)
        : CreateTrack(calculateInsertIndex(isMaster, derivedFromTrackIndex, tracks), isMaster, derivedFromTrackIndex, tracks, view) {}

CreateTrack::CreateTrack(int insertIndex, bool isMaster, int derivedFromTrackIndex, Tracks &tracks, View &view)
        : insertIndex(insertIndex), derivedFromTrackIndex(derivedFromTrackIndex), isMaster(isMaster), tracks(tracks) {
}

static ValueTree create(bool isMaster, const Track *derivedFromTrack, Tracks &tracks) {
    ValueTree state(Track::getIdentifier());
    state.appendChild(ValueTree(ProcessorLanesIDs::PROCESSOR_LANES), nullptr);
    state.setProperty(TrackIDs::isMaster, isMaster, nullptr);
    state.setProperty(TrackIDs::uuid, Uuid().toString(), nullptr);
    bool isSubTrack = !isMaster && derivedFromTrack != nullptr;
    const String name = isMaster ? "Master" : (derivedFromTrack != nullptr ? makeTrackNameUnique(tracks, derivedFromTrack->getName()) : ("Track " + String(tracks.getNumNonMasterTracks() + 1)));
    state.setProperty(TrackIDs::name, name, nullptr);
    state.setProperty(TrackIDs::colour, isSubTrack ? derivedFromTrack->getColour().toString() : Colour::fromHSV((1.0f / 8.0f) * tracks.size(), 0.65f, 0.65f, 1.0f).toString(), nullptr);
    state.setProperty(TrackIDs::selected, false, nullptr);

    return state;
}

bool CreateTrack::perform() {
    tracks.add(create(isMaster, tracks.getChild(derivedFromTrackIndex), tracks), insertIndex);
    return true;
}

bool CreateTrack::undo() {
    tracks.remove(insertIndex);
    return true;
}
