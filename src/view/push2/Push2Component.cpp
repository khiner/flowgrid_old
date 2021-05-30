#include "Push2Component.h"

#include "ApplicationPropertiesAndCommandManager.h"

Push2Component::Push2Component(View &view, Tracks &tracks, Connections &connections, Project &project, StatefulAudioProcessorWrappers &processorWrappers, Push2MidiCommunicator &push2MidiCommunicator)
        : Push2ComponentBase(view, tracks, push2MidiCommunicator),
          project(project), connections(connections), processorWrappers(processorWrappers),
          processorView(view, tracks, project, push2MidiCommunicator), processorSelector(view, tracks, project, push2MidiCommunicator),
          mixerView(view, tracks, project, processorWrappers, push2MidiCommunicator), push2NoteModePadLedManager(tracks, push2MidiCommunicator) {
    startTimer(60);

    addChildComponent(processorView);
    addChildComponent(processorSelector);
    addChildComponent(mixerView);

    tracks.addTracksListener(this);
    tracks.addListener(this);
    tracks.addTracksListener(this);
    connections.addListener(this);
    view.addListener(this);
    this->project.getUndoManager().addChangeListener(this);

    setBounds(0, 0, Push2Display::WIDTH, Push2Display::HEIGHT);
    const auto &r = getLocalBounds();
    mixerView.setBounds(r);
    processorView.setBounds(r);
    processorSelector.setBounds(r);
    updateEnabledPush2Buttons();
}

Push2Component::~Push2Component() {
    setVisible(false);
    project.getUndoManager().removeChangeListener(this);
    view.removeListener(this);
    connections.removeListener(this);
    tracks.removeListener(this);
    tracks.removeTracksListener(this);
}

void Push2Component::handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) {
    if (message.isNoteOff() || (message.isNoteOnOrOff() && view.isInNoteMode())) {
        push2NoteModePadLedManager.handleIncomingMidiMessage(source, message);
    }
}

void Push2Component::setVisible(bool visible) {
    if (currentlyViewingChild != nullptr)
        currentlyViewingChild->setVisible(visible);

    Push2ComponentBase::setVisible(visible);
    updatePush2NoteModePadLedManagerVisibility();
}

void Push2Component::shiftPressed() {
    Push2ComponentBase::shiftPressed();
    changeListenerCallback(&project.getUndoManager());
    project.setPush2ShiftHeld(true);
}

void Push2Component::shiftReleased() {
    Push2ComponentBase::shiftReleased();
    changeListenerCallback(&project.getUndoManager());
    project.setPush2ShiftHeld(false);
}

void Push2Component::masterEncoderRotated(float changeAmount) {
    const auto &trackOutputProcessor = Track::getOutputProcessor(tracks.getMasterTrackState());
    if (auto *masterGainProcessor = processorWrappers.getProcessorWrapperForState(trackOutputProcessor))
        if (auto *masterGainParameter = masterGainProcessor->getParameter(1))
            masterGainParameter->setValue(masterGainParameter->getValue() + changeAmount);
}

void Push2Component::encoderRotated(int encoderIndex, float changeAmount) {
    if (currentlyViewingChild != nullptr) {
        currentlyViewingChild->encoderRotated(encoderIndex, changeAmount);
    }
}

void Push2Component::undoButtonPressed() {
    getCommandManager().invokeDirectly(isShiftHeld ? CommandIDs::redo : CommandIDs::undo, false);
}

void Push2Component::addTrackButtonPressed() {
    getCommandManager().invokeDirectly(isShiftHeld ? CommandIDs::insertProcessorLane : CommandIDs::insertTrack, false);
}

void Push2Component::deleteButtonPressed() {
    getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
}

void Push2Component::duplicateButtonPressed() {
    getCommandManager().invokeDirectly(CommandIDs::duplicateSelected, false);
}

void Push2Component::addDeviceButtonPressed() {
    showChild(&processorSelector);
}

void Push2Component::mixButtonPressed() {
    showChild(&mixerView);
}

void Push2Component::masterButtonPressed() {
    project.setTrackSelected(tracks.getMasterTrack(), true);
}

void Push2Component::aboveScreenButtonPressed(int buttonIndex) {
    if (currentlyViewingChild != nullptr) {
        currentlyViewingChild->aboveScreenButtonPressed(buttonIndex);
    }
}

void Push2Component::belowScreenButtonPressed(int buttonIndex) {
    if (currentlyViewingChild != nullptr) {
        currentlyViewingChild->belowScreenButtonPressed(buttonIndex);
    }
}

void Push2Component::arrowPressed(int arrowDirection) {
    if (currentlyViewingChild == &processorSelector) {
        currentlyViewingChild->arrowPressed(arrowDirection);
        return;
    }
    switch (arrowDirection) {
        case Push2::leftArrowDirection:
            return project.navigateLeft();
        case Push2::rightArrowDirection:
            return project.navigateRight();
        case Push2::upArrowDirection:
            return project.navigateUp();
        case Push2::downArrowDirection:
            return project.navigateDown();
        default:
            return;
    }
}

void Push2Component::updateEnabledPush2Buttons() {
    if (isVisible()) {
        push2.enableWhiteLedButton(Push2::addTrack);
        push2.enableWhiteLedButton(Push2::mix);
        if (view.isInNoteMode()) {
            push2.activateWhiteLedButton(Push2::note);
            push2.enableWhiteLedButton(Push2::session);
        } else if (view.isInSessionMode()) {
            push2.enableWhiteLedButton(Push2::note);
            push2.activateWhiteLedButton(Push2::session);
        }
        push2.activateWhiteLedButton(Push2::shift);
        updatePush2SelectionDependentButtons();
        changeListenerCallback(&project.getUndoManager());
    } else {
        for (auto buttonId : {Push2::addTrack, Push2::delete_, Push2::addDevice,
                              Push2::mix, Push2::master, Push2::undo, Push2::note, Push2::session,
                              Push2::shift, Push2::left, Push2::right, Push2::up, Push2::down,
                              Push2::duplicate}) {
            push2.disableWhiteLedButton(buttonId);
        }
    }
}

void Push2Component::drawFrame() {
    static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

    auto &g = displayBridge.getGraphics();
    g.fillAll(CLEAR_COLOR);
    paintEntireComponent(g, true);
    displayBridge.writeFrameToDisplay();
}

void Push2Component::updatePush2SelectionDependentButtons() {
    const auto *focusedTrack = tracks.getFocusedTrack();
    if (focusedTrack != nullptr) {
        push2.activateWhiteLedButton(Push2MidiCommunicator::delete_);
        push2.enableWhiteLedButton(Push2MidiCommunicator::addDevice);
    } else {
        push2.disableWhiteLedButton(Push2MidiCommunicator::delete_);
        push2.disableWhiteLedButton(Push2MidiCommunicator::addDevice);
    }

    if (tracks.anyTrackHasSelections())
        push2.activateWhiteLedButton(Push2::duplicate);
    else
        push2.disableWhiteLedButton(Push2::duplicate);

    if (tracks.getMasterTrack() == nullptr)
        push2.disableWhiteLedButton(Push2MidiCommunicator::master);
    else if (focusedTrack != tracks.getMasterTrack())
        push2.enableWhiteLedButton(Push2MidiCommunicator::master);
    else
        push2.activateWhiteLedButton(Push2MidiCommunicator::master);
    push2NoteModePadLedManager.trackSelected(focusedTrack);

    for (int direction : {Push2::upArrowDirection, Push2::downArrowDirection, Push2::leftArrowDirection, Push2::rightArrowDirection}) {
        if (isVisible() && currentlyViewingChild != &processorSelector && canNavigateInDirection(direction))
            push2.activateWhiteLedButton(Push2::ccNumberForArrowButton(direction));
        else
            push2.disableWhiteLedButton(Push2::ccNumberForArrowButton(direction));
    }
}

bool Push2Component::canNavigateInDirection(int direction) const {
    switch (direction) {
        case Push2::rightArrowDirection:
            return project.canNavigateRight();
        case Push2::downArrowDirection:
            return project.canNavigateDown();
        case Push2::leftArrowDirection:
            return project.canNavigateLeft();
        case Push2::upArrowDirection:
            return project.canNavigateUp();
        default:
            return false;
    }
}

void Push2Component::updatePush2NoteModePadLedManagerVisibility() {
    push2NoteModePadLedManager.setVisible(isVisible() && view.isInNoteMode() && project.isPush2MidiInputProcessorConnected());
}

void Push2Component::updateFocusedProcessor() {
    auto *focusedProcessorWrapper = processorWrappers.getProcessorWrapperForState(tracks.getFocusedProcessor());
    if (focusedProcessorWrapper == nullptr) return;

    if (currentlyViewingChild != &mixerView || focusedProcessorWrapper->processor->getName() != InternalPluginFormat::getTrackOutputProcessorName()) {
        processorView.processorFocused(focusedProcessorWrapper);
        showChild(&processorView);
    }
}

void Push2Component::showChild(Push2ComponentBase *child) {
    if (currentlyViewingChild == child) return;

    if (currentlyViewingChild != nullptr)
        currentlyViewingChild->setVisible(false);

    currentlyViewingChild = child;
    if (currentlyViewingChild != nullptr)
        currentlyViewingChild->setVisible(true);
}

void Push2Component::trackAdded(Track *track) {
    updatePush2SelectionDependentButtons();
}
void Push2Component::trackRemoved(Track *track, int oldIndex) {
    updatePush2SelectionDependentButtons();
    if (!tracks.getFocusedTrackState().isValid()) {
        showChild(nullptr);
        processorView.processorFocused(nullptr);
    }
}

void Push2Component::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (Connection::isType(child)) {
        updatePush2NoteModePadLedManagerVisibility();
    }
}
void Push2Component::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (Processor::isType(child)) {
        updateFocusedProcessor();
        updatePush2SelectionDependentButtons();
    } else if (Connection::isType(child)) {
        updatePush2NoteModePadLedManagerVisibility();
    }
}
void Push2Component::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ViewIDs::focusedTrackIndex) {
        updatePush2SelectionDependentButtons();
    } else if (i == ViewIDs::focusedProcessorSlot || i == ProcessorIDs::initialized) {
        updateFocusedProcessor();
    } else if (i == ViewIDs::controlMode) {
        updateEnabledPush2Buttons();
        updatePush2NoteModePadLedManagerVisibility();
    }
}

void Push2Component::changeListenerCallback(ChangeBroadcaster *source) {
    if (auto *undoManager = dynamic_cast<UndoManager *>(source)) {
        if ((!isShiftHeld && undoManager->canUndo()) || (isShiftHeld && undoManager->canRedo()))
            push2.activateWhiteLedButton(Push2::undo);
        else
            push2.enableWhiteLedButton(Push2::undo);
    }
}
