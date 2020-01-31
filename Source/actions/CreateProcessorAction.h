#pragma once

#include <state/TracksState.h>

#include "JuceHeader.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(const PluginDescription &description, const ValueTree& parentTrack, int slot,
                          TracksState &tracks, InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : parentTrack(parentTrack), tracks(tracks) {
        processorToCreate = ValueTree(IDs::PROCESSOR);
        processorToCreate.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processorToCreate.setProperty(IDs::name, description.name, nullptr);
        processorToCreate.setProperty(IDs::allowDefaultConnections, true, nullptr);

        if (tracks.isMixerChannelProcessor(processorToCreate)) {
            indexToInsertProcessor = -1;
            slot = tracks.getMixerChannelSlotForTrack(parentTrack);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                indexToInsertProcessor = 0;
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(parentTrack);
                indexToInsertProcessor = mixerChannelProcessor.isValid() ? parentTrack.indexOf(mixerChannelProcessor) : parentTrack.getNumChildren();
                slot = indexToInsertProcessor <= 0 ? 0 : int(parentTrack.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
            }
        } else {
            tracks.setProcessorSlot(parentTrack, processorToCreate, slot, nullptr);
            indexToInsertProcessor = tracks.getParentIndexForProcessor(parentTrack, processorToCreate, nullptr);
        }
        tracks.setProcessorSlot(parentTrack, processorToCreate, slot, nullptr);
    }

    bool perform() override {
        parentTrack.addChild(processorToCreate, indexToInsertProcessor, nullptr);
        tracks.makeSlotsValid(parentTrack, nullptr); // TODO this is a bug - no undo version of this
        // makeSlotsValid should use the new moveProcessorsAction, since it moves processors (if any) by the same amount (at most one slot down)
        // however, this is fully testable beforehand without the case of adding a processor to a populated slot
        return true;
    }

    bool undo() override {
        parentTrack.removeChild(processorToCreate, nullptr);
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    ValueTree parentTrack;
    ValueTree processorToCreate;
    int indexToInsertProcessor;

protected:
    TracksState &tracks;

private:
    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
