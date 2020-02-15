#pragma once

#include <state/TracksState.h>

#include "JuceHeader.h"
#include "InsertProcessorAction.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(ValueTree processorToCreate, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
              insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
              audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : CreateProcessorAction(createProcessor(description), trackIndex, slot, tracks, view, audioProcessorContainer, pluginManager) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : CreateProcessorAction(createProcessor(description), trackIndex, getInsertSlot(description, trackIndex, tracks),
                                    tracks, view, audioProcessorContainer, pluginManager) {}

    bool perform() override {
        insertAction.perform();
        if (processorToCreate.isValid())
            audioProcessorContainer.onProcessorCreated(processorToCreate);
        return true;
    }

    bool undo() override {
        insertAction.undo();
        if (processorToCreate.isValid()) {
            pluginManager.closeWindowFor(StatefulAudioProcessorContainer::getNodeIdForState(processorToCreate));
            audioProcessorContainer.onProcessorDestroyed(processorToCreate);
        }
        return true;
    }

    // The temporary versions of the perform/undo methods are used only to change the grid state
    // in the creation of other undoable actions that affect the grid.
    bool performTemporary() { return insertAction.perform(); }

    bool undoTemporary() { return insertAction.undo(); }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

    int trackIndex, slot;

    static ValueTree createProcessor(const PluginDescription &description) {
        ValueTree processorToCreate(IDs::PROCESSOR);
        processorToCreate.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processorToCreate.setProperty(IDs::name, description.name, nullptr);
        processorToCreate.setProperty(IDs::allowDefaultConnections, true, nullptr);
        return processorToCreate;
    }

private:
    ValueTree processorToCreate;
    InsertProcessorAction insertAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    static int getInsertSlot(const PluginDescription &description, int trackIndex, TracksState &tracks) {
        const auto &track = tracks.getTrack(trackIndex);

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
