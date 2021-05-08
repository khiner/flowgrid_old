#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>
#include <view/PluginWindow.h>
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections,
                          StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), trackIndex(tracks.indexOf(TracksState::getTrackForProcessor(processorToDelete))),
              processorToDelete(processorToDelete),
              processorIndex(processorToDelete.getParent().indexOf(processorToDelete)),
              pluginWindowType(processorToDelete[IDs::pluginWindowType]),
              disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
              audioProcessorContainer(audioProcessorContainer) {}

    bool perform() override {
        audioProcessorContainer.saveProcessorStateInformationToState(processorToDelete);
        performTemporary();
        processorToDelete.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        audioProcessorContainer.onProcessorDestroyed(processorToDelete);
        return true;
    }

    bool undo() override {
        auto track = tracks.getTrack(trackIndex);
        if (TracksState::isTrackIOProcessor(processorToDelete))
            track.appendChild(processorToDelete, nullptr);
        else
            TracksState::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
        audioProcessorContainer.onProcessorCreated(processorToDelete);
        processorToDelete.setProperty(IDs::pluginWindowType, pluginWindowType, nullptr);
        disconnectProcessorAction.undo();

        return true;
    }

    bool performTemporary() {
        disconnectProcessorAction.perform();
        auto track = tracks.getTrack(trackIndex);
        if (TracksState::isTrackIOProcessor(processorToDelete))
            track.removeChild(processorToDelete, nullptr);
        else
            TracksState::getProcessorLaneForTrack(track).removeChild(processorToDelete, nullptr);
        return true;
    }

    bool undoTemporary() {
        auto track = tracks.getTrack(trackIndex);
        if (TracksState::isTrackIOProcessor(processorToDelete))
            track.appendChild(processorToDelete, nullptr);
        else
            TracksState::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
        disconnectProcessorAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:
    TracksState &tracks;
    int trackIndex;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessorAction disconnectProcessorAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
