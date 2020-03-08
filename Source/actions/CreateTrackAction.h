#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool isMaster, bool addMixer, const ValueTree &derivedFromTrack, TracksState &tracks, ViewState &view)
            : CreateTrackAction(calculateInsertIndex(isMaster, addMixer, derivedFromTrack, tracks),
                                isMaster, addMixer, derivedFromTrack, tracks, view) {}

    CreateTrackAction(int insertIndex, bool isMaster, bool addMixer, ValueTree derivedFromTrack, TracksState &tracks, ViewState &view)
            : insertIndex(insertIndex), tracks(tracks) {
        const auto &focusedTrack = tracks.getFocusedTrack();

        if (!isMaster && !derivedFromTrack.isValid() && focusedTrack.isValid()) {
            // If a track is focused, insert the new track to the left of it if there's no mixer,
            // or to the right of the first track with a mixer if the new track has a mixer.
            derivedFromTrack = focusedTrack;

            if (addMixer)
                while (derivedFromTrack.isValid() && !tracks.getMixerChannelProcessorForTrack(derivedFromTrack).isValid())
                    derivedFromTrack = derivedFromTrack.getSibling(1);
        }
        if (derivedFromTrack == tracks.getMasterTrack())
            derivedFromTrack = {};

        // TODO move into method and construct in initializer list
        int numTracks = tracks.getNumNonMasterTracks();
        trackToCreate = ValueTree(IDs::TRACK);
        trackToCreate.setProperty(IDs::isMasterTrack, isMaster, nullptr);
        trackToCreate.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        if (isMaster) {
            trackToCreate.setProperty(IDs::name, "Master", nullptr);
            trackToCreate.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        } else {
            bool isSubTrack = derivedFromTrack.isValid() && !addMixer;
            trackToCreate.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(derivedFromTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
            trackToCreate.setProperty(IDs::colour, isSubTrack ? derivedFromTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        }
        trackToCreate.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
        trackToCreate.setProperty(IDs::selected, false, nullptr);
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
        return (int) sizeof(*this); //xxx should be more accurate
    }

    ValueTree trackToCreate;
    int insertIndex;

protected:
    TracksState &tracks;

    // NOTE: assumes the track hasn't been added yet!
    String makeTrackNameUnique(const String &trackName) {
        for (const auto &track : tracks.getState()) {
            String otherTrackName = track[IDs::name];
            if (otherTrackName == trackName) {
                if (trackName.contains("-")) {
                    int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                    if (i != 0)
                        return makeTrackNameUnique(trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
                } else {
                    return makeTrackNameUnique(trackName + "-" + String(1));
                }
            }
        }

        return trackName;
    }

private:

    static int calculateInsertIndex(bool isMaster, bool addMixer, const ValueTree &derivedFromTrack, TracksState &tracks) {
        if (isMaster)
            return tracks.getNumNonMasterTracks();
        return derivedFromTrack.isValid() ?
               derivedFromTrack.getParent().indexOf(derivedFromTrack) + (addMixer ? 1 : 0) :
               tracks.getNumNonMasterTracks();
    }

    JUCE_DECLARE_NON_COPYABLE(CreateTrackAction)
};
