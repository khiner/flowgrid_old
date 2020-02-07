#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool addMixer, ValueTree nextToTrack, bool forceImmediatelyToRight,
                      TracksState &tracks, ConnectionsState &connections, ViewState &view)
            : tracks(tracks) {
        int numTracks = tracks.getNumNonMasterTracks();
        const auto& focusedTrack = tracks.getFocusedTrack();

        if (!nextToTrack.isValid()) {
            if (focusedTrack.isValid()) {
                // If a track is focused, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                nextToTrack = focusedTrack;

                if (addMixer && !forceImmediatelyToRight) {
                    while (nextToTrack.isValid() && !tracks.getMixerChannelProcessorForTrack(nextToTrack).isValid())
                        nextToTrack = nextToTrack.getSibling(1);
                }
            }
        }
        if (nextToTrack == tracks.getMasterTrack())
            nextToTrack = {};

        bool isSubTrack = nextToTrack.isValid() && !addMixer;

        // TODO move into method and construct in initializer list
        trackToCreate = ValueTree(IDs::TRACK);
        trackToCreate.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        trackToCreate.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(nextToTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
        trackToCreate.setProperty(IDs::colour, isSubTrack ? nextToTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);

        indexToInsertTrack = nextToTrack.isValid() ? nextToTrack.getParent().indexOf(nextToTrack) + (addMixer || forceImmediatelyToRight ? 1 : 0): numTracks;
    }

    bool perform() override {
        tracks.getState().addChild(trackToCreate, indexToInsertTrack, nullptr);
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
    int indexToInsertTrack;

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
    JUCE_DECLARE_NON_COPYABLE(CreateTrackAction)
};
