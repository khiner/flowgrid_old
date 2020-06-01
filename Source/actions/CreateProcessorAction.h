#pragma once

#include <state/TracksState.h>
#include <view/PluginWindow.h>

#include "JuceHeader.h"
#include "InsertProcessorAction.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(ValueTree processorToCreate, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
            pluginWindowType(this->processorToCreate[IDs::pluginWindowType]),
              insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
              audioProcessorContainer(audioProcessorContainer) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateProcessorAction(createProcessor(description), trackIndex, slot, tracks, view, audioProcessorContainer) {}

    CreateProcessorAction(const PluginDescription &description, int trackIndex,
                          TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateProcessorAction(createProcessor(description), trackIndex, getInsertSlot(description, trackIndex, tracks),
                                    tracks, view, audioProcessorContainer) {}

    bool perform() override {
        performTemporary();
        if (processorToCreate.isValid()) {
            audioProcessorContainer.onProcessorCreated(processorToCreate);
            processorToCreate.setProperty(IDs::pluginWindowType, pluginWindowType, nullptr);
        }
        return true;
    }

    bool undo() override {
        undoTemporary();
        if (processorToCreate.isValid()) {
            processorToCreate.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
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
        processorToCreate.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        return processorToCreate;
    }

private:
    ValueTree processorToCreate;
    int pluginWindowType;
    InsertProcessorAction insertAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;

    static int getInsertSlot(const PluginDescription &description, int trackIndex, TracksState &tracks) {
        const auto &track = tracks.getTrack(trackIndex);

        if (description.name == TrackOutputProcessor::name())
            return -1;

        if (description.numInputChannels == 0) {
            return 0;
        } else {
            // Insert new effect processors right after the lane's last processor
            const auto &lane = TracksState::getProcessorLaneForTrack(track);
            int indexToInsertProcessor = lane.getNumChildren();
            return indexToInsertProcessor <= 0 ? 0 : int(lane.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
        }
    }

    JUCE_DECLARE_NON_COPYABLE(CreateProcessorAction)
};
