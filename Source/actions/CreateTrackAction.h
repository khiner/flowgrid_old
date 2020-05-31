#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>
#include <processors/TrackOutputProcessor.h>

#include "JuceHeader.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool isMaster, bool addMixer, const ValueTree &derivedFromTrack, TracksState &tracks, ViewState &view)
            : CreateTrackAction(calculateInsertIndex(isMaster, addMixer, derivedFromTrack, tracks),
                                isMaster, addMixer, derivedFromTrack, tracks, view) {}

    CreateTrackAction(int insertIndex, bool isMaster, bool addMixer, ValueTree derivedFromTrack, TracksState &tracks, ViewState &view)
            : insertIndex(insertIndex), tracks(tracks) {
        // TODO move into method and construct in initializer list
        int numTracks = tracks.getNumNonMasterTracks();
        newTrack = ValueTree(IDs::TRACK);
        newTrack.setProperty(IDs::isMasterTrack, isMaster, nullptr);
        newTrack.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        if (isMaster) {
            newTrack.setProperty(IDs::name, "Master", nullptr);
            newTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        } else {
            bool isSubTrack = derivedFromTrack.isValid() && !addMixer;
            newTrack.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(derivedFromTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
            newTrack.setProperty(IDs::colour, isSubTrack ? derivedFromTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        }
        newTrack.setProperty(IDs::selected, false, nullptr);

        auto lane = ValueTree(IDs::PROCESSOR_LANE);
        lane.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);
        newTrack.appendChild(lane, nullptr);
    }

    bool perform() override {
        tracks.getState().addChild(newTrack, insertIndex, nullptr);
        return true;
    }

    bool undo() override {
        tracks.getState().removeChild(newTrack, nullptr);
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

    ValueTree newTrack;
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
