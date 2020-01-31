#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include <ValueTreeItems.h>
#include <PluginManager.h>
#include <push2/Push2MidiCommunicator.h>
#include "push2/Push2DisplayBridge.h"
#include "ProcessorGraph.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"
#include "Push2MixerView.h"
#include "Push2NoteModePadLedManager.h"

class Push2Component :
        public Timer,
        public Push2ComponentBase,
        private ChangeListener,
        private Utilities::ValueTreePropertyChangeListener {
public:
    explicit Push2Component(Project &project, Push2MidiCommunicator &push2MidiCommunicator, ProcessorGraph &audioGraphBuilder)
            : Push2ComponentBase(project, push2MidiCommunicator), graph(audioGraphBuilder),
              processorView(project, push2MidiCommunicator), processorSelector(project, push2MidiCommunicator),
              mixerView(project, push2MidiCommunicator), push2NoteModePadLedManager(project, push2MidiCommunicator) {
        startTimer(60);

        addChildComponent(processorView);
        addChildComponent(processorSelector);
        addChildComponent(mixerView);

        this->project.getState().addListener(this);
        this->project.getUndoManager().addChangeListener(this);

        setBounds(0, 0, Push2Display::WIDTH, Push2Display::HEIGHT);
        const auto &r = getLocalBounds();
        mixerView.setBounds(r);
        processorView.setBounds(r);
        processorSelector.setBounds(r);
        updateEnabledPush2Buttons();
    }

    ~Push2Component() override {
        setVisible(false);
        project.getUndoManager().removeChangeListener(this);
        project.getState().removeListener(this);
    }

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override {
        if (message.isNoteOff() || (message.isNoteOnOrOff() && view.isInNoteMode())) {
            push2NoteModePadLedManager.handleIncomingMidiMessage(source, message);
        }
    }

    void setVisible(bool visible) override {
        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(visible);

        Push2ComponentBase::setVisible(visible);
        updatePush2NoteModePadLedManagerVisibility();
    }

    void shiftPressed() override {
        Push2ComponentBase::shiftPressed();
        changeListenerCallback(&project.getUndoManager());
        project.setPush2ShiftHeld(true);
    }

    void shiftReleased() override {
        Push2ComponentBase::shiftReleased();
        changeListenerCallback(&project.getUndoManager());
        project.setPush2ShiftHeld(false);
    }

    void masterEncoderRotated(float changeAmount) override {
        auto *masterGainParameter = graph.getMasterGainProcessor()->getParameter(1);
        if (masterGainParameter != nullptr)
            masterGainParameter->setValue(masterGainParameter->getValue() + changeAmount);
    }
    
    void encoderRotated(int encoderIndex, float changeAmount) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->encoderRotated(encoderIndex, changeAmount);
        }
    }
    
    void undoButtonPressed() override {
        getCommandManager().invokeDirectly(isShiftHeld ? CommandIDs::redo : CommandIDs::undo, false);
    }
    
    void addTrackButtonPressed() override {
        getCommandManager().invokeDirectly(isShiftHeld ? CommandIDs::insertTrackWithoutMixer : CommandIDs::insertTrack, false);
    }
    
    void deleteButtonPressed() override {
        getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
    }

    void duplicateButtonPressed() override {
        getCommandManager().invokeDirectly(CommandIDs::duplicateSelected, false);
    }
    
    void addDeviceButtonPressed() override {
        showChild(&processorSelector);
    }

    void mixButtonPressed() override {
        showChild(&mixerView);
    }

    void masterButtonPressed() override {
        project.setTrackSelected(tracks.getMasterTrack(), true, true);
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->aboveScreenButtonPressed(buttonIndex);
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->belowScreenButtonPressed(buttonIndex);
        }
    }

    void arrowPressed(int arrowDirection) override {
        if (currentlyViewingChild == &processorSelector) {
            currentlyViewingChild->arrowPressed(arrowDirection);
            return;
        }
        switch (arrowDirection) {
            case Push2::leftArrowDirection: return project.navigateLeft();
            case Push2::rightArrowDirection: return project.navigateRight();
            case Push2::upArrowDirection: return project.navigateUp();
            case Push2::downArrowDirection: return project.navigateDown();
            default: return;
        }
    }

    void noteButtonPressed() override { view.setNoteMode(); }

    void sessionButtonPressed() override { view.setSessionMode(); }

    void updateEnabledPush2Buttons() override {
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

private:
    Push2DisplayBridge displayBridge;
    ProcessorGraph &graph;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;
    Push2MixerView mixerView;
    Push2NoteModePadLedManager push2NoteModePadLedManager;

    Push2ComponentBase *currentlyViewingChild {};

    void timerCallback() override { drawFrame(); }

    // Render a frame and send it to the Push 2 display (if it's available)
    void inline drawFrame() {
        static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

        auto &g = displayBridge.getGraphics();
        g.fillAll(CLEAR_COLOR);
        paintEntireComponent(g, true);
        displayBridge.writeFrameToDisplay();
    }

    void updatePush2SelectionDependentButtons() {
        const auto &focusedTrack = tracks.getFocusedTrack();
        if (focusedTrack.isValid()) {
            push2.activateWhiteLedButton(Push2MidiCommunicator::delete_);
            push2.enableWhiteLedButton(Push2MidiCommunicator::addDevice);
        } else {
            push2.disableWhiteLedButton(Push2MidiCommunicator::delete_);
            push2.disableWhiteLedButton(Push2MidiCommunicator::addDevice);
        }

        if (tracks.canDuplicateSelected())
            push2.activateWhiteLedButton(Push2::duplicate);
        else
            push2.disableWhiteLedButton(Push2::duplicate);

        if (!tracks.getMasterTrack().isValid())
            push2.disableWhiteLedButton(Push2MidiCommunicator::master);
        else if (focusedTrack != tracks.getMasterTrack())
            push2.enableWhiteLedButton(Push2MidiCommunicator::master);
        else
            push2.activateWhiteLedButton(Push2MidiCommunicator::master);
        push2NoteModePadLedManager.trackSelected(focusedTrack);

        for (int direction : { Push2::upArrowDirection, Push2::downArrowDirection, Push2::leftArrowDirection, Push2::rightArrowDirection }) {
            if (isVisible() && currentlyViewingChild != &processorSelector && canNavigateInDirection(direction))
                push2.activateWhiteLedButton(Push2::ccNumberForArrowButton(direction));
            else
                push2.disableWhiteLedButton(Push2::ccNumberForArrowButton(direction));
        }
    }

    bool canNavigateInDirection(int direction) const {
        switch (direction) {
            case Push2::rightArrowDirection: return project.canNavigateRight();
            case Push2::downArrowDirection: return project.canNavigateDown();
            case Push2::leftArrowDirection: return project.canNavigateLeft();
            case Push2::upArrowDirection: return project.canNavigateUp();
            default: return false;
        }
    }

    void updatePush2NoteModePadLedManagerVisibility() {
        push2NoteModePadLedManager.setVisible(isVisible() && view.isInNoteMode() && project.isPush2MidiInputProcessorConnected());
    }

    void updateFocusedProcessor() {
        auto *focusedProcessorWrapper = graph.getProcessorWrapperForState(tracks.getFocusedProcessor());
        if (currentlyViewingChild != &mixerView || !dynamic_cast<MixerChannelProcessor *>(focusedProcessorWrapper->processor)) {
            processorView.processorFocused(focusedProcessorWrapper);
            showChild(&processorView);
        }
    }

    void showChild(Push2ComponentBase* child) {
        if (currentlyViewingChild == child)
            return;

        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(false);

        currentlyViewingChild = child;
        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(true);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::focusedTrackIndex) {
            updatePush2SelectionDependentButtons();
        } else if (i == IDs::focusedProcessorSlot || i == IDs::processorInitialized) {
            updateFocusedProcessor();
        } else if (i == IDs::controlMode) {
            updateEnabledPush2Buttons();
            updatePush2NoteModePadLedManagerVisibility();
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK)) {
            updatePush2SelectionDependentButtons();
        } else if (child.hasType(IDs::CONNECTION)) {
            updatePush2NoteModePadLedManagerVisibility();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::TRACK)) {
            updatePush2SelectionDependentButtons();
            if (!tracks.getFocusedTrack().isValid()) {
                showChild(nullptr);
                processorView.processorFocused(nullptr);
            }
        } else if (child.hasType(IDs::PROCESSOR)) {
            updateFocusedProcessor();
            updatePush2SelectionDependentButtons();
        } else if (child.hasType(IDs::CONNECTION)) {
            updatePush2NoteModePadLedManagerVisibility();
        }
    }

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (auto* undoManager = dynamic_cast<UndoManager *>(source)) {
            if ((!isShiftHeld && undoManager->canUndo()) || (isShiftHeld && undoManager->canRedo()))
                push2.activateWhiteLedButton(Push2::undo);
            else
                push2.enableWhiteLedButton(Push2::undo);
        }
    }
};
