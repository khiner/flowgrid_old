#pragma once

#include <state/TracksState.h>

#include <utility>

#include "JuceHeader.h"
#include "InsertProcessorAction.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(const ValueTree& processorToCreate, const PluginDescription &description, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : processorToCreate(processorToCreate), insertAction(processorToCreate, trackIndex, slot, tracks, view),
              audioProcessorContainer(audioProcessorContainer) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateProcessorAction(createProcessor(description), description, trackIndex, slot, tracks, view, audioProcessorContainer) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateProcessorAction(createProcessor(description), description, trackIndex, getInsertSlot(description, trackIndex, tracks),
                                    tracks, view, audioProcessorContainer) {}

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

    static int getInsertSlot(const PluginDescription &description, int trackIndex, TracksState &tracks) {
        const auto& track = tracks.getTrack(trackIndex);

        int slot;
        if (description.name == MixerChannelProcessor::name() && !tracks.getMixerChannelProcessorForTrack(track).isValid()) {
            slot = tracks.getMixerChannelSlotForTrack(track);
        } else {
            if (description.numInputChannels == 0) {
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(track);
                int indexToInsertProcessor = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren();
                slot = indexToInsertProcessor <= 0 ? 0 : int(track.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
            }
        }
        return slot;
    }

    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
