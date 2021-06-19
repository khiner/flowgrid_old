#include "Project.h"

#include "action/SelectProcessorSlot.h"
#include "action/CreateTrack.h"
#include "action/CreateProcessor.h"
#include "action/DeleteSelectedItems.h"
#include "action/ResetDefaultExternalInputConnectionsAction.h"
#include "action/SetDefaultConnectionsAllowed.h"
#include "action/UpdateAllDefaultConnections.h"
#include "action/MoveSelectedItems.h"
#include "action/SelectRectangle.h"
#include "action/Insert.h"
#include "action/SelectTrack.h"
#include "processors/TrackInputProcessor.h"
#include "processors/TrackOutputProcessor.h"
#include "processors/SineBank.h"
#include "ApplicationPropertiesAndCommandManager.h"

Project::Project(View &view, Tracks &tracks, Connections &connections, Input &input, Output &output,
                 AllProcessors &allProcessors, ProcessorGraph &processorGraph, UndoManager &undoManager, PluginManager &pluginManager, AudioDeviceManager &deviceManager)
        : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
          view(view),
          tracks(tracks),
          connections(connections),
          input(input),
          output(output),
          allProcessors(allProcessors),
          processorGraph(processorGraph),
          undoManager(undoManager),
          pluginManager(pluginManager),
          deviceManager(deviceManager) {
    state.setProperty(ProjectIDs::name, "My Project", nullptr);
    state.appendChild(input.getState(), nullptr);
    state.appendChild(output.getState(), nullptr);
    state.appendChild(tracks.getState(), nullptr);
    state.appendChild(connections.getState(), nullptr);
    state.appendChild(view.getState(), nullptr);
    undoManager.addChangeListener(this);
}

Project::~Project() {
    undoManager.removeChangeListener(this);
}

void Project::loadFromState(const ValueTree &fromState) {
    clear();

    view.loadFromParentState(fromState);

    const String &inputDeviceName = Processor::getDeviceName(fromState.getChildWithName(InputIDs::INPUT));
    const String &outputDeviceName = Processor::getDeviceName(fromState.getChildWithName(OutputIDs::OUTPUT));

    // TODO this should be replaced with the greyed-out IO processor behavior (keeping connections)
    static const String &failureMessage = TRANS("Could not open an Audio IO device used by this project.  "
                                                "All connections with the missing device will be gone.  "
                                                "If you want this project to look like it did when you saved it, "
                                                "the best thing to do is to reconnect the missing device and "
                                                "reload this project (without saving first!).");

    if (isDeviceWithNamePresent(inputDeviceName))
        input.loadFromParentState(fromState);
    else
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open input device \"") + inputDeviceName + "\"", failureMessage);

    if (isDeviceWithNamePresent(outputDeviceName))
        output.loadFromParentState(fromState);
    else
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open output device \"") + outputDeviceName + "\"", failureMessage);

    tracks.loadFromParentState(fromState);
    connections.loadFromParentState(fromState);
    selectProcessor(tracks.getFocusedProcessor());
    undoManager.clearUndoHistory();
    sendChangeMessage();
}

void Project::clear() {
    input.clear();
    output.clear();
    tracks.clear();
    connections.clear();
    undoManager.clearUndoHistory();
}

void Project::createTrack(bool isMaster) {
    if (isMaster && tracks.getMasterTrack() != nullptr) return; // only one master track allowed!

    setShiftHeld(false); // prevent rectangle-select behavior when doing cmd+shift+t
    undoManager.beginNewTransaction();

    undoManager.perform(new CreateTrack(isMaster, -1, tracks, view));
    auto *mostRecentlyCreatedTrack = tracks.getMostRecentlyCreatedTrack();
    undoManager.perform(new CreateProcessor(TrackInputProcessor::getPluginDescription(), mostRecentlyCreatedTrack->getIndex(), tracks, view, processorGraph));
    undoManager.perform(new CreateProcessor(TrackOutputProcessor::getPluginDescription(), mostRecentlyCreatedTrack->getIndex(), tracks, view, processorGraph));

    setTrackSelected(mostRecentlyCreatedTrack, true);
    updateAllDefaultConnections();
}

void Project::createProcessor(const PluginDescription &description, int slot) {
    undoManager.beginNewTransaction();
    auto *focusedTrack = tracks.getFocusedTrack();
    if (focusedTrack != nullptr) {
        doCreateAndAddProcessor(description, focusedTrack, slot);
    }
}

void Project::deleteSelectedItems() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();

    undoManager.beginNewTransaction();
    undoManager.perform(new DeleteSelectedItems(tracks, connections, processorGraph));
    if (view.getFocusedTrackIndex() >= tracks.size() && tracks.size() > 0)
        setTrackSelected(tracks.getChild(tracks.size() - 1), true);
    updateAllDefaultConnections();
}

void Project::insert() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();
    undoManager.beginNewTransaction();
    undoManager.perform(new Insert(false, copiedTracks, view.getFocusedTrackAndSlot(), tracks, connections, view, input, allProcessors, processorGraph));
    updateAllDefaultConnections();
}

void Project::duplicateSelectedItems() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();
    OwnedArray<Track> duplicateTracks;
    tracks.copySelectedItemsInto(duplicateTracks, processorGraph.getProcessorWrappers());
    undoManager.beginNewTransaction();
    undoManager.perform(new Insert(true, duplicateTracks, view.getFocusedTrackAndSlot(), tracks, connections, view, input, allProcessors, processorGraph));
    updateAllDefaultConnections();
}

void Project::beginDragging(const juce::Point<int> trackAndSlot) {
    if (trackAndSlot.x == Tracks::INVALID_TRACK_AND_SLOT.x) return;

    const auto *track = tracks.getChild(trackAndSlot.x);
    if (trackAndSlot.y == -1 && track != nullptr && track->isMaster()) return;

    initialDraggingTrackAndSlot = trackAndSlot;
    currentlyDraggingTrackAndSlot = initialDraggingTrackAndSlot;

    // During drag actions, everything _except the audio graph_ is updated as a preview
    processorGraph.pauseAudioGraphUpdates();
}

void Project::dragToPosition(juce::Point<int> trackAndSlot) {
    if (isCurrentlyDraggingProcessor() && currentlyDraggingTrackAndSlot.y > -1 && trackAndSlot.y <= -1)
        trackAndSlot.y = 0;
    if (!isCurrentlyDraggingProcessor() || trackAndSlot == currentlyDraggingTrackAndSlot ||
        (currentlyDraggingTrackAndSlot.y == -1 && trackAndSlot.x == currentlyDraggingTrackAndSlot.x) ||
        trackAndSlot == Tracks::INVALID_TRACK_AND_SLOT)
        return;

    if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot)
        undoManager.beginNewTransaction();

    auto onlyMoveActionInCurrentTransaction = [&]() -> bool {
        Array<const UndoableAction *> actionsFound;
        undoManager.getActionsInCurrentTransaction(actionsFound);
        return actionsFound.size() == 1 && dynamic_cast<const MoveSelectedItems *>(actionsFound.getUnchecked(0)) != nullptr;
    };

    if (undoManager.getNumActionsInCurrentTransaction() > 0) {
        if (onlyMoveActionInCurrentTransaction()) {
            undoManager.undoCurrentTransactionOnly();
        } else {
            undoManager.beginNewTransaction();
            initialDraggingTrackAndSlot = currentlyDraggingTrackAndSlot;
        }
    }

    if (trackAndSlot == initialDraggingTrackAndSlot ||
        undoManager.perform(new MoveSelectedItems(initialDraggingTrackAndSlot, trackAndSlot, isAltHeld(),
                                                  tracks, connections, view, input, output, allProcessors, processorGraph))) {
        currentlyDraggingTrackAndSlot = trackAndSlot;
    }
}

void Project::setProcessorSlotSelected(Track *track, int slot, bool selected, bool deselectOthers) {
    if (track == nullptr) return;

    Select *selectAction = nullptr;
    if (selected) {
        const juce::Point<int> trackAndSlot(track->getIndex(), slot);
        if (push2ShiftHeld || shiftHeld)
            selectAction = new SelectRectangle(selectionStartTrackAndSlot, trackAndSlot, tracks, connections, view, input, allProcessors, processorGraph);
        else
            selectionStartTrackAndSlot = trackAndSlot;
    }
    if (selectAction == nullptr) {
        if (slot == -1)
            selectAction = new SelectTrack(track, selected, deselectOthers, tracks, connections, view, input, allProcessors, processorGraph);
        else
            selectAction = new SelectProcessorSlot(track, slot, selected, selected && deselectOthers, tracks, connections, view, input, allProcessors, processorGraph);
    }
    undoManager.perform(selectAction);
}

void Project::setTrackSelected(Track *track, bool selected, bool deselectOthers) {
    setProcessorSlotSelected(track, -1, selected, deselectOthers);
}

void Project::selectProcessor(const Processor *processor) {
    setProcessorSlotSelected(tracks.getTrackForProcessor(processor), processor->getSlot(), true);
}

bool Project::disconnectCustom(Processor *processor) {
    undoManager.beginNewTransaction();
    return processorGraph.doDisconnectNode(processor, all, false, true, true, true);
}

void Project::setDefaultConnectionsAllowed(Processor *processor, bool defaultConnectionsAllowed) {
    undoManager.beginNewTransaction();
    undoManager.perform(new SetDefaultConnectionsAllowed(processor, defaultConnectionsAllowed, connections));
    undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, allProcessors, processorGraph));
}

void Project::toggleProcessorBypass(Processor *processor) {
    undoManager.beginNewTransaction();
    processor->setBypassed(!processor->isBypassed(), &undoManager);
}

void Project::selectTrackAndSlot(juce::Point<int> trackAndSlot) {
    if (trackAndSlot.x < 0 || trackAndSlot.x >= tracks.size()) return;

    auto *track = tracks.getChild(trackAndSlot.x);
    const int slot = trackAndSlot.y;
    if (slot != -1)
        setProcessorSlotSelected(track, slot, true);
    else
        setTrackSelected(track, true);
}

void Project::createDefaultProject() {
    view.initializeDefault();
    input.initializeDefault();
    output.initializeDefault();
    createTrack(true);
    createTrack(false);
    doCreateAndAddProcessor(SineBank::getPluginDescription(), tracks.getMostRecentlyCreatedTrack(), 0);
    // Select action only does this if the focused track changes, so we just need to do this once ourselves
    undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, allProcessors, processorGraph));
    undoManager.clearUndoHistory();
    sendChangeMessage();
}

void Project::doCreateAndAddProcessor(const PluginDescription &description, Track *track, int slot) {
    if (PluginManager::isGeneratorOrInstrument(&description) && track != nullptr && track->hasProducerProcessor()) {
        undoManager.perform(new CreateTrack(false, track->getIndex(), tracks, view));
        return doCreateAndAddProcessor(description, tracks.getMostRecentlyCreatedTrack(), slot);
    }

    if (slot == -1)
        undoManager.perform(new CreateProcessor(description, track->getIndex(), tracks, view, processorGraph));
    else
        undoManager.perform(new CreateProcessor(description, track->getIndex(), slot, tracks, view, processorGraph));

    if (auto *mostRecentlyCreatedProcessor = tracks.getMostRecentlyCreatedProcessor()) {
        setProcessorSlotSelected(track, mostRecentlyCreatedProcessor->getSlot(), true);
    }

    updateAllDefaultConnections();
}

void Project::changeListenerCallback(ChangeBroadcaster *source) {
    if (source == &undoManager) {
        // if there is nothing to undo, there is nothing to save!
        setChangedFlag(undoManager.canUndo());
    }
}

void Project::updateAllDefaultConnections() {
    undoManager.perform(new UpdateAllDefaultConnections(false, true, tracks, connections, input, output, allProcessors, processorGraph));
}

Result Project::loadDocument(const File &file) {
    if (auto xml = std::unique_ptr<XmlElement>(XmlDocument::parse(file))) {
        const ValueTree &newState = ValueTree::fromXml(*xml);
        if (!newState.isValid() || !newState.hasType(ProjectIDs::PROJECT))
            return Result::fail(TRANS("Not a valid project file"));

        loadFromState(newState);
        return Result::ok();
    }
    return Result::fail(TRANS("Not a valid XML file"));
}

bool Project::isDeviceWithNamePresent(const String &deviceName) const {
    for (auto *deviceType : deviceManager.getAvailableDeviceTypes()) {
        // Input devices
        for (const auto &existingDeviceName : deviceType->getDeviceNames(true)) {
            if (deviceName == existingDeviceName)
                return true;
        }
        // Output devices
        for (const auto &existingDeviceName : deviceType->getDeviceNames()) {
            if (deviceName == existingDeviceName)
                return true;
        }
    }
    return false;
}

Result Project::saveDocument(const File &file) {
    for (const auto *track : tracks.getChildren())
        for (auto processorState : track->getProcessorLane()->getState())
            processorGraph.getProcessorWrappers().saveProcessorStateInformationToState(processorState);

    if (auto xml = state.createXml())
        if (!xml->writeTo(file))
            return Result::fail(TRANS("Could not save the project file"));

    return Result::ok();
}

File Project::getLastDocumentOpened() {
    RecentlyOpenedFilesList recentFiles;
    recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));
    return recentFiles.getFile(0);
}

void Project::setLastDocumentOpened(const File &file) {
    RecentlyOpenedFilesList recentFiles;
    recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));
    recentFiles.addFile(file);
    getUserSettings()->setValue("recentProjectFiles", recentFiles.toString());
}
