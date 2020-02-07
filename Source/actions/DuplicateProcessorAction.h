#pragma once

#include <state/TracksState.h>

#include "JuceHeader.h"

struct DuplicateProcessorAction : public UndoableAction {
    DuplicateProcessorAction(ValueTree& processorToDuplicate, int toTrackIndex, int toSlot,
                             TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : createProcessorAction(createProcessor(processorToDuplicate, tracks),
                                    *pluginManager.getDescriptionForIdentifier(processorToDuplicate[IDs::id]),
                                    toTrackIndex, toSlot, tracks, view, audioProcessorContainer) {}

    DuplicateProcessorAction(ValueTree& processorToDuplicate, TracksState &tracks, ViewState &view,
                             StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : DuplicateProcessorAction(processorToDuplicate, tracks.indexOf(processorToDuplicate.getParent()), int(processorToDuplicate[IDs::processorSlot]) + 1,
                    tracks, view, audioProcessorContainer, pluginManager) {}

    bool perform() override {
        return createProcessorAction.perform();
    }

    bool undo() override {
        return createProcessorAction.undo();
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    CreateProcessorAction createProcessorAction;

    static ValueTree createProcessor(ValueTree& fromProcessor, TracksState &tracks) {
        tracks.saveProcessorStateInformationToState(fromProcessor);
        auto duplicatedProcessor = fromProcessor.createCopy();
        duplicatedProcessor.removeProperty(IDs::nodeId, nullptr);
        return duplicatedProcessor;
    }

    JUCE_DECLARE_NON_COPYABLE(DuplicateProcessorAction)
};
