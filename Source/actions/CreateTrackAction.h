#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool addMixer, const ValueTree &derivedFromTrack,
                      TracksState &tracks, ConnectionsState &connections, ViewState &view)
            : CreateTrackAction(calculateInsertIndex(addMixer, derivedFromTrack, tracks),
                                addMixer, derivedFromTrack, tracks, connections, view) {}

    CreateTrackAction(int insertIndex, bool addMixer, ValueTree derivedFromTrack,
                      TracksState &tracks, ConnectionsState &connections, ViewState &view)
            : insertIndex(insertIndex), tracks(tracks) {
        const auto& focusedTrack = tracks.getFocusedTrack();

        if (!derivedFromTrack.isValid()) {
            if (focusedTrack.isValid()) {
                // If a track is focused, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                derivedFromTrack = focusedTrack;

                if (addMixer)
                    while (derivedFromTrack.isValid() && !tracks.getMixerChannelProcessorForTrack(derivedFromTrack).isValid())
                        derivedFromTrack = derivedFromTrack.getSibling(1);
            }
        }
        if (derivedFromTrack == tracks.getMasterTrack())
            derivedFromTrack = {};

        bool isSubTrack = derivedFromTrack.isValid() && !addMixer;

        // TODO move into method and construct in initializer list
        int numTracks = tracks.getNumNonMasterTracks();
        trackToCreate = ValueTree(IDs::TRACK);
        trackToCreate.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        trackToCreate.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(derivedFromTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
        trackToCreate.setProperty(IDs::colour, isSubTrack ? derivedFromTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
    }

    bool perform() override {
        tracks.getState().addChild(trackToCreate, insertIndex, nullptr);
        return true;
    }

    bool undo() override {
        tracks.getState().removeChild(trackToCreate, nullptr);
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    ValueTree trackToCreate;
    int insertIndex;

protected:
    TracksState &tracks;

    // NOTE: assumes the track hasn't been added yet!
    String makeTrackNameUnique(const String& trackName) {
        for (const auto& track : tracks.getState()) {
            String otherTrackName = track[IDs::name];
            if (otherTrackName == trackName) {
                if (trackName.contains("-")) {
                    int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                    if (i != 0) {
                        return makeTrackNameUnique(trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
                    }
                } else {
                    return makeTrackNameUnique(trackName + "-" + String(1));
                }
            }
        }

        return trackName;
    }
private:

    static int calculateInsertIndex(bool addMixer, const ValueTree &derivedFromTrack, TracksState &tracks) {
        return derivedFromTrack.isValid() ?
               derivedFromTrack.getParent().indexOf(derivedFromTrack) + (addMixer ? 1 : 0) :
               tracks.getNumNonMasterTracks();
    }

    JUCE_DECLARE_NON_COPYABLE(CreateTrackAction)
};
