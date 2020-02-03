#pragma once

#include <state/TracksState.h>

#include "JuceHeader.h"
#include "InsertProcessorAction.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(const PluginDescription &description, ValueTree& parentTrack, int slot,
                          TracksState &tracks, InputState &input, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : processorToCreate(createProcessor(description)), insertAction(createInsertAction(description, parentTrack, tracks, view, slot)), audioProcessorContainer(audioProcessorContainer) {
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

    InsertProcessorAction createInsertAction(const PluginDescription &description, ValueTree &parentTrack,
                                             TracksState &tracks, ViewState &view, int slot) {
        if (tracks.isMixerChannelProcessor(processorToCreate)) {
            slot = tracks.getMixerChannelSlotForTrack(parentTrack);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(parentTrack);
                int indexToInsertProcessor = mixerChannelProcessor.isValid() ? parentTrack.indexOf(mixerChannelProcessor) : parentTrack.getNumChildren();
                slot = indexToInsertProcessor <= 0 ? 0 : int(parentTrack.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
            }
        }

        return {parentTrack, processorToCreate, slot, tracks, view};
    }

    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
