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
        push2MidiCommunicator.enableWhiteLedButton(Push2::addTrack);
        push2.enableWhiteLedButton(Push2::mix);
        push2.activateWhiteLedButton(Push2::shift);
    }

    ~Push2Component() override {
        setVisible(false);
        project.getUndoManager().removeChangeListener(this);
        project.getState().removeListener(this);
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        if (!visible) {
            for (auto buttonId : {Push2::shift, Push2::addTrack, Push2::delete_, Push2::addDevice, Push2::mix, Push2::master, Push2::undo}) {
                push2.disableWhiteLedButton(buttonId);
            }
        }
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

    void arrowPressed(Direction direction) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->arrowPressed(direction);
            return;
        }
        switch (direction) {
            case Direction::up: return project.moveSelectionUp();
            case Direction::down: return project.moveSelectionDown();
            case Direction::left: return project.moveSelectionLeft();
            case Direction::right: return project.moveSelectionRight();
            default: return;
        }
    }

private:
    Push2DisplayBridge displayBridge;
    ProcessorGraph &graph;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;
    Push2MixerView mixerView;

    Push2ComponentBase *currentlyViewingChild {};

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

    // Render a frame and send it to the Push 2 display
    void inline drawFrame() {
        static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

        auto &g = displayBridge.getGraphics();
        g.fillAll(CLEAR_COLOR);
        paintEntireComponent(g, true);
        displayBridge.writeFrameToDisplay();
    }

    void timerCallback() override { drawFrame(); }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            if (tree.hasType(IDs::PROCESSOR)) {
                if (auto *processorWrapper = graph.getProcessorWrapperForState(tree)) {
                    selectProcessorIfNeeded(processorWrapper);
                }
                push2.enableWhiteLedButton(Push2::addDevice);
            } else if (tree.hasType(IDs::TRACK) || tree.hasType(IDs::MASTER_TRACK)) {
                push2.enableWhiteLedButton(Push2::addDevice);
            } else {
                selectChild(nullptr);
                processorView.processorSelected(nullptr);
                push2.disableWhiteLedButton(Push2::addDevice);
            }
            if (project.isItemDeletable(tree))
                push2.activateWhiteLedButton(Push2::delete_);
            else
                push2.disableWhiteLedButton(Push2::delete_);
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {}

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::MASTER_TRACK) || child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
            if (!project.getSelectedTrack().isValid()) {
                selectChild(nullptr);
                processorView.processorSelected(nullptr);
                push2.disableWhiteLedButton(Push2::addDevice);
                push2.disableWhiteLedButton(Push2::delete_);
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
