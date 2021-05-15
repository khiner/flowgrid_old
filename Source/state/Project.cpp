#include "Project.h"

#include "actions/SelectProcessorSlotAction.h"
#include "actions/CreateTrackAction.h"
#include "actions/CreateProcessorAction.h"
#include "actions/DeleteSelectedItemsAction.h"
#include "actions/ResetDefaultExternalInputConnectionsAction.h"
#include "actions/SetDefaultConnectionsAllowedAction.h"
#include "actions/UpdateAllDefaultConnectionsAction.h"
#include "actions/MoveSelectedItemsAction.h"
#include "actions/SelectRectangleAction.h"
#include "actions/InsertAction.h"
#include "actions/SelectTrackAction.h"
#include "processors/TrackInputProcessor.h"
#include "processors/TrackOutputProcessor.h"
#include "processors/SineBank.h"
#include "ApplicationPropertiesAndCommandManager.h"

Project::Project(ViewState &view, TracksState &tracks, ConnectionsState &connections, InputState &input, OutputState &output,
                 ProcessorGraph &processorGraph, UndoManager &undoManager, PluginManager &pluginManager, AudioDeviceManager &deviceManager)
        : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
          view(view),
          tracks(tracks),
          connections(connections),
          input(input),
          output(output),
          processorGraph(processorGraph),
          undoManager(undoManager),
          pluginManager(pluginManager),
          deviceManager(deviceManager),
          copiedTracks(tracks, processorGraph) {
    state.setProperty(ProjectIDs::name, "My Project", nullptr);
    state.appendChild(input.getState(), nullptr);
    state.appendChild(output.getState(), nullptr);
    state.appendChild(tracks.getState(), nullptr);
    state.appendChild(connections.getState(), nullptr);
    state.appendChild(view.getState(), nullptr);
    undoManager.addChangeListener(this);
    tracks.addListener(this);
}

Project::~Project() {
    tracks.removeListener(this);
}

void Project::loadFromState(const ValueTree &fromState) {
    clear();

    view.loadFromParentState(fromState);

    const String &inputDeviceName = fromState.getChildWithName(InputStateIDs::INPUT)[TracksStateIDs::deviceName];
    const String &outputDeviceName = fromState.getChildWithName(OutputStateIDs::OUTPUT)[TracksStateIDs::deviceName];

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
    if (isMaster && tracks.getMasterTrack().isValid())
        return; // only one master track allowed!

    setShiftHeld(false); // prevent rectangle-select behavior when doing cmd+shift+t
    undoManager.beginNewTransaction();

    undoManager.perform(new CreateTrackAction(isMaster, {}, tracks, view));
    undoManager.perform(new CreateProcessorAction(TrackInputProcessor::getPluginDescription(),
                                                  tracks.indexOf(mostRecentlyCreatedTrack), tracks, view, processorGraph));
    undoManager.perform(new CreateProcessorAction(TrackOutputProcessor::getPluginDescription(),
                                                  tracks.indexOf(mostRecentlyCreatedTrack), tracks, view, processorGraph));

    setTrackSelected(mostRecentlyCreatedTrack, true);
    updateAllDefaultConnections();
}

void Project::createProcessor(const PluginDescription &description, int slot) {
    undoManager.beginNewTransaction();
    auto focusedTrack = tracks.getFocusedTrack();
    if (focusedTrack.isValid()) {
        doCreateAndAddProcessor(description, focusedTrack, slot);
    }
}

void Project::deleteSelectedItems() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();

    undoManager.beginNewTransaction();
    undoManager.perform(new DeleteSelectedItemsAction(tracks, connections, processorGraph));
    if (view.getFocusedTrackIndex() >= tracks.getNumTracks() && tracks.getNumTracks() > 0)
        setTrackSelected(tracks.getTrack(tracks.getNumTracks() - 1), true);
    updateAllDefaultConnections();
}

void Project::insert() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();
    undoManager.beginNewTransaction();
    undoManager.perform(new InsertAction(false, copiedTracks.getState(), view.getFocusedTrackAndSlot(), tracks, connections, view, input, processorGraph));
    updateAllDefaultConnections();
}

void Project::duplicateSelectedItems() {
    if (isCurrentlyDraggingProcessor())
        endDraggingProcessor();
    CopiedTracks duplicateState(tracks, processorGraph);
    duplicateState.copySelectedItems();

    undoManager.beginNewTransaction();
    undoManager.perform(new InsertAction(true, duplicateState.getState(), view.getFocusedTrackAndSlot(), tracks, connections, view, input, processorGraph));
    updateAllDefaultConnections();
}

void Project::beginDragging(const juce::Point<int> trackAndSlot) {
    if (trackAndSlot.x == TracksState::INVALID_TRACK_AND_SLOT.x ||
        (trackAndSlot.y == -1 && TracksState::isMasterTrack(tracks.getTrack(trackAndSlot.x))))
        return;

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
        trackAndSlot == TracksState::INVALID_TRACK_AND_SLOT)
        return;

    if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot)
        undoManager.beginNewTransaction();

    auto onlyMoveActionInCurrentTransaction = [&]() -> bool {
        Array<const UndoableAction *> actionsFound;
        undoManager.getActionsInCurrentTransaction(actionsFound);
        return actionsFound.size() == 1 && dynamic_cast<const MoveSelectedItemsAction *>(actionsFound.getUnchecked(0)) != nullptr;
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
        undoManager.perform(new MoveSelectedItemsAction(initialDraggingTrackAndSlot, trackAndSlot, isAltHeld(),
                                                        tracks, connections, view, input, output, processorGraph))) {
        currentlyDraggingTrackAndSlot = trackAndSlot;
    }
}

void Project::setProcessorSlotSelected(const ValueTree &track, int slot, bool selected, bool deselectOthers) {
    if (!track.isValid()) return;

    SelectAction *selectAction = nullptr;
    if (selected) {
        const juce::Point<int> trackAndSlot(tracks.indexOf(track), slot);
        if (push2ShiftHeld || shiftHeld)
            selectAction = new SelectRectangleAction(selectionStartTrackAndSlot, trackAndSlot, tracks, connections, view, input, processorGraph);
        else
            selectionStartTrackAndSlot = trackAndSlot;
    }
    if (selectAction == nullptr) {
        if (slot == -1)
            selectAction = new SelectTrackAction(track, selected, deselectOthers, tracks, connections, view, input, processorGraph);
        else
            selectAction = new SelectProcessorSlotAction(track, slot, selected, selected && deselectOthers, tracks, connections, view, input, processorGraph);
    }
    undoManager.perform(selectAction);
}

void Project::setTrackSelected(const ValueTree &track, bool selected, bool deselectOthers) {
    setProcessorSlotSelected(track, -1, selected, deselectOthers);
}

void Project::selectProcessor(const ValueTree &processor) {
    setProcessorSlotSelected(TracksState::getTrackForProcessor(processor), processor[TracksStateIDs::processorSlot], true);
}

bool Project::disconnectCustom(const ValueTree &processor) {
    undoManager.beginNewTransaction();
    return processorGraph.doDisconnectNode(processor, all, false, true, true, true);
}

void Project::setDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed) {
    undoManager.beginNewTransaction();
    undoManager.perform(new SetDefaultConnectionsAllowedAction(processor, defaultConnectionsAllowed, connections));
    undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, processorGraph));
}

void Project::toggleProcessorBypass(ValueTree processor) {
    undoManager.beginNewTransaction();
    processor.setProperty(TracksStateIDs::bypassed, !processor[TracksStateIDs::bypassed], &undoManager);
}

void Project::selectTrackAndSlot(juce::Point<int> trackAndSlot) {
    if (trackAndSlot.x < 0 || trackAndSlot.x >= tracks.getNumTracks())
        return;

    const auto &track = tracks.getTrack(trackAndSlot.x);
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
    doCreateAndAddProcessor(SineBank::getPluginDescription(), mostRecentlyCreatedTrack, 0);
    // Select action only does this if the focused track changes, so we just need to do this once ourselves
    undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, processorGraph));
    undoManager.clearUndoHistory();
    sendChangeMessage();
}

void Project::doCreateAndAddProcessor(const PluginDescription &description, ValueTree &track, int slot) {
    if (PluginManager::isGeneratorOrInstrument(&description) &&
        tracks.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
        undoManager.perform(new CreateTrackAction(false, track, tracks, view));
        return doCreateAndAddProcessor(description, mostRecentlyCreatedTrack, slot);
    }

    if (slot == -1)
        undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), tracks, view, processorGraph));
    else
        undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), slot, tracks, view, processorGraph));

    selectProcessor(mostRecentlyCreatedProcessor);
    updateAllDefaultConnections();
}

void Project::changeListenerCallback(ChangeBroadcaster *source) {
    if (source == &undoManager) {
        // if there is nothing to undo, there is nothing to save!
        setChangedFlag(undoManager.canUndo());
    }
}

void Project::updateAllDefaultConnections() {
    undoManager.perform(new UpdateAllDefaultConnectionsAction(false, true, tracks, connections, input, output, processorGraph));
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
    for (const auto &track : tracks.getState())
        for (auto processorState : TracksState::getProcessorLaneForTrack(track))
            processorGraph.saveProcessorStateInformationToState(processorState);

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
