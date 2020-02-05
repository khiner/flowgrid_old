#pragma once

#include <state/TracksState.h>

#include "JuceHeader.h"
#include "InsertProcessorAction.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(const PluginDescription &description, const ValueTree& track, int slot,
                          TracksState &tracks, InputState &input, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : processorToCreate(createProcessor(description)), insertAction(createInsertAction(description, track, slot, tracks, view)), audioProcessorContainer(audioProcessorContainer) {
    }

    bool perform() override {
        insertAction.perform();
        audioProcessorContainer.onProcessorCreated(processorToCreate);
        return true;
    }

    bool undo() override {
        insertAction.undo();
        audioProcessorContainer.onProcessorDestroyed(processorToCreate);
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree processorToCreate;
    InsertProcessorAction insertAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;

    static ValueTree createProcessor(const PluginDescription &description) {
        ValueTree processorToCreate(IDs::PROCESSOR);
        processorToCreate.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processorToCreate.setProperty(IDs::name, description.name, nullptr);
        processorToCreate.setProperty(IDs::allowDefaultConnections, true, nullptr);
        return processorToCreate;
    }

    InsertProcessorAction createInsertAction(const PluginDescription &description, const ValueTree& track, int slot,
                                             TracksState &tracks, ViewState &view) {
        if (TracksState::isMixerChannelProcessor(processorToCreate) && !tracks.getMixerChannelProcessorForTrack(track).isValid()) {
            slot = tracks.getMixerChannelSlotForTrack(track);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(track);
                int indexToInsertProcessor = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren();
                slot = indexToInsertProcessor <= 0 ? 0 : int(track.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
            }
        }

        return {processorToCreate, tracks.indexOf(track), slot, tracks, view};
    }

    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
