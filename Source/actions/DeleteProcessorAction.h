#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>
#include <view/PluginWindow.h>

#include "JuceHeader.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections,
                          StatefulAudioProcessorContainer &audioProcessorContainer)
            : parentTrack(processorToDelete.getParent()), processorToDelete(processorToDelete),
              processorIndex(parentTrack.indexOf(processorToDelete)), pluginWindowType(processorToDelete[IDs::pluginWindowType]),
              disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
              audioProcessorContainer(audioProcessorContainer) {}

    bool perform() override {
        audioProcessorContainer.saveProcessorStateInformationToState(processorToDelete);
        processorToDelete.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        disconnectProcessorAction.perform();
        parentTrack.removeChild(processorToDelete, nullptr);
        audioProcessorContainer.onProcessorDestroyed(processorToDelete);
        return true;
    }

    bool undo() override {
        parentTrack.addChild(processorToDelete, processorIndex, nullptr);
        audioProcessorContainer.onProcessorCreated(processorToDelete);
        disconnectProcessorAction.undo();
        processorToDelete.setProperty(IDs::pluginWindowType, pluginWindowType, nullptr);

        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree parentTrack;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessorAction disconnectProcessorAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
