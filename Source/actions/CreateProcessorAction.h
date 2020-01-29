#pragma once

#include <state_managers/ConnectionsStateManager.h>
#include <state_managers/TracksStateManager.h>

#include "JuceHeader.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(const PluginDescription &description, const ValueTree& parentTrack, int slot,
                          TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager)
            : parentTrack(parentTrack), tracksManager(tracksManager), connectionsManager(connectionsManager), viewManager(viewManager) {
        processorToCreate = ValueTree(IDs::PROCESSOR);
        processorToCreate.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processorToCreate.setProperty(IDs::name, description.name, nullptr);
        processorToCreate.setProperty(IDs::allowDefaultConnections, true, nullptr);

        if (tracksManager.isMixerChannelProcessor(processorToCreate)) {
            indexToInsertProcessor = -1;
            slot = tracksManager.getMixerChannelSlotForTrack(parentTrack);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                indexToInsertProcessor = 0;
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = tracksManager.getMixerChannelProcessorForTrack(parentTrack);
                indexToInsertProcessor = mixerChannelProcessor.isValid() ? parentTrack.indexOf(mixerChannelProcessor) : parentTrack.getNumChildren();
                slot = indexToInsertProcessor <= 0 ? 0 : int(parentTrack.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
            }
        } else {
            tracksManager.setProcessorSlot(parentTrack, processorToCreate, slot, nullptr);
            indexToInsertProcessor = tracksManager.getParentIndexForProcessor(parentTrack, processorToCreate, nullptr);
        }
        tracksManager.setProcessorSlot(parentTrack, processorToCreate, slot, nullptr);
        selectProcessorAction = std::make_unique<SelectProcessorSlotAction>(parentTrack, slot, true, true,
                                                                            tracksManager, connectionsManager, viewManager);
    }

    bool perform() override {
        parentTrack.addChild(processorToCreate, indexToInsertProcessor, nullptr);
        tracksManager.makeSlotsValid(parentTrack, nullptr); // TODO this is a bug - no undo version of this
        // makeSlotsValid should use the new moveProcessorsAction, since it moves processors (if any) by the same amount (at most one slot down)
        // however, this is fully testable beforehand without the case of adding a processor to a populated slot
        selectProcessorAction->perform();
        connectionsManager.updateAllDefaultConnections(nullptr);
        return true;
    }

    bool undo() override {
        processorToCreate.removeProperty(IDs::processorInitialized, nullptr); // TODO I feel like this isn't really needed anymore
        connectionsManager.disconnectProcessor(processorToCreate, nullptr);
        parentTrack.removeChild(processorToCreate, nullptr);
        selectProcessorAction->undo();
        connectionsManager.updateAllDefaultConnections(nullptr);
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    ValueTree parentTrack;
    ValueTree processorToCreate;
    int indexToInsertProcessor;

protected:
    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;
    ViewStateManager &viewManager;

private:
    std::unique_ptr<SelectProcessorSlotAction> selectProcessorAction;

    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
