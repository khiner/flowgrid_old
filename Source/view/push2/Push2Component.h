#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include <push2/Push2MidiCommunicator.h>
#include "push2/Push2DisplayBridge.h"
#include "ProcessorGraph.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"
#include "Push2MixerView.h"

class Push2Component :
        public Timer,
        public Push2ComponentBase,
        private ChangeListener,
        private Utilities::ValueTreePropertyChangeListener {
public:
    explicit Push2Component(Project &project, Push2MidiCommunicator &push2MidiCommunicator, ProcessorGraph &audioGraphBuilder)
            : Push2ComponentBase(project, push2MidiCommunicator), graph(audioGraphBuilder),
              processorView(project, push2MidiCommunicator), processorSelector(project, push2MidiCommunicator),
              mixerView(project, push2MidiCommunicator) {
        startTimer(60);

        addChildComponent(processorView);
        addChildComponent(processorSelector);
        addChildComponent(mixerView);

        project.getState().addListener(this);
        project.getUndoManager().addChangeListener(this);
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

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        updateEnabledPush2Buttons();
    }

    void shiftPressed() override {
        Push2ComponentBase::shiftPressed();
        changeListenerCallback(&project.getUndoManager());
    }

    void shiftReleased() override {
        Push2ComponentBase::shiftReleased();
        changeListenerCallback(&project.getUndoManager());
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
    
    void addDeviceButtonPressed() override {
        selectChild(&processorSelector);
    }

    void mixButtonPressed() override {
        selectChild(&mixerView);
    }

    void masterButtonPressed() override {
        project.getMasterTrack().setProperty(IDs::selected, true, nullptr);
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
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->arrowPressed(arrowDirection);
            return;
        }
        switch (arrowDirection) {
            case Push2::upArrowDirection: return project.moveSelectionUp();
            case Push2::downArrowDirection: return project.moveSelectionDown();
            case Push2::leftArrowDirection: return project.moveSelectionLeft();
            case Push2::rightArrowDirection: return project.moveSelectionRight();
            default: return;
        }
    }

    void noteButtonPressed() override { project.setNoteMode(); }

    void sessionButtonPressed() override { project.setSessionMode(); }

    void updateEnabledPush2Buttons() override {
        if (isVisible()) {
            push2.enableWhiteLedButton(Push2::addTrack);
            push2.enableWhiteLedButton(Push2::mix);
            if (project.isInNoteMode()) {
                push2.activateWhiteLedButton(Push2::note);
                push2.enableWhiteLedButton(Push2::session);
            } else if (project.isInSessionMode()) {
                push2.enableWhiteLedButton(Push2::note);
                push2.activateWhiteLedButton(Push2::session);
            }
            push2.activateWhiteLedButton(Push2::shift);
            updatePush2SelectionDependentButtons();
            changeListenerCallback(&project.getUndoManager());
            if (currentlyViewingChild != nullptr)
                currentlyViewingChild->updateEnabledPush2Buttons();
        } else {
            for (auto buttonId : {Push2::addTrack, Push2::delete_, Push2::addDevice,
                                  Push2::mix, Push2::master, Push2::undo, Push2::note, Push2::session,
                                  Push2::shift}) {
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
        const auto &selectedTrack = project.getSelectedTrack();
        if (selectedTrack.isValid()) {
            push2.activateWhiteLedButton(Push2MidiCommunicator::delete_);
            push2.enableWhiteLedButton(Push2MidiCommunicator::addDevice);
        } else {
            push2.disableWhiteLedButton(Push2MidiCommunicator::delete_);
            push2.disableWhiteLedButton(Push2MidiCommunicator::addDevice);
        }

        if (!project.getMasterTrack().isValid())
            push2.disableWhiteLedButton(Push2MidiCommunicator::master);
        else if (selectedTrack != project.getMasterTrack())
            push2.enableWhiteLedButton(Push2MidiCommunicator::master);
        else
            push2.activateWhiteLedButton(Push2MidiCommunicator::master);
    }

    void selectProcessorIfNeeded(StatefulAudioProcessorWrapper* processorWrapper) {
        if (currentlyViewingChild != &mixerView || !dynamic_cast<MixerChannelProcessor *>(processorWrapper->processor)) {
            processorView.processorSelected(processorWrapper);
            selectChild(&processorView);
        }
    }

    void selectChild(Push2ComponentBase* child) {
        if (currentlyViewingChild == child)
            return;

        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(false);

        currentlyViewingChild = child;
        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(true);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            updatePush2SelectionDependentButtons();
            if (tree.hasType(IDs::PROCESSOR)) {
                if (auto *processorWrapper = graph.getProcessorWrapperForState(tree)) {
                    selectProcessorIfNeeded(processorWrapper);
                }
            }
        } else if (i == IDs::controlMode) {
            updateEnabledPush2Buttons();
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::MASTER_TRACK) || child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
            updatePush2SelectionDependentButtons();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::MASTER_TRACK) || child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
            updatePush2SelectionDependentButtons();
            if (!project.getSelectedTrack().isValid()) {
                selectChild(nullptr);
                processorView.processorSelected(nullptr);
            }
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
