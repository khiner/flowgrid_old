#pragma once

#include <state/OutputState.h>
#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct InsertAction : UndoableAction {
    InsertAction(const ValueTree &copiedState, const juce::Point<int> toTrackAndSlot,
                 TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : copiedState(copiedState.createCopy()), fromTrackAndSlot(findFromTrackAndSlot()), toTrackAndSlot(toTrackAndSlot),
              tracks(tracks), view(view), audioProcessorContainer(audioProcessorContainer) {
    }

    bool perform() override {
        const auto trackAndSlotDiff = toTrackAndSlot - fromTrackAndSlot;
        for (const auto &track : copiedState) {
            for (auto processor : track) {
                juce::Point<int> trackAndSlot = {copiedState.indexOf(track), processor[IDs::processorSlot]};
                auto toTrackAndSlot = trackAndSlot + trackAndSlotDiff;
                CreateProcessorAction(audioProcessorContainer.duplicateProcessor(processor), toTrackAndSlot.x, toTrackAndSlot.y,
                                      tracks, view, audioProcessorContainer)
                                      .perform();
            }
        }
        return true;
    }

    bool undo() override {
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:

    ValueTree copiedState;
    juce::Point<int> fromTrackAndSlot;
    juce::Point<int> toTrackAndSlot;

    TracksState &tracks;
    ViewState &view;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> findFromTrackAndSlot() {
        juce::Point<int> fromTrackAndSlot = {INT_MAX, INT_MAX};
        for (int trackIndex = 0; trackIndex < copiedState.getNumChildren(); trackIndex++) {
            const auto &track = copiedState.getChild(trackIndex);
            if (fromTrackAndSlot.x == INT_MAX && (track[IDs::selected] || tracks.trackHasAnySlotSelected(track))) {
                fromTrackAndSlot.x = trackIndex;
            }
            if (!track[IDs::selected]) {
                int lowestSelectedSlotForTrack = tracks.getSlotMask(track).findNextSetBit(0);
                if (lowestSelectedSlotForTrack != -1)
                    fromTrackAndSlot.y = std::min(fromTrackAndSlot.y, lowestSelectedSlotForTrack);
            }
        }
        assert(fromTrackAndSlot.x != INT_MAX); // Copied state must, by definition, have a selection.
        return fromTrackAndSlot;
    }

    JUCE_DECLARE_NON_COPYABLE(InsertAction)
};
