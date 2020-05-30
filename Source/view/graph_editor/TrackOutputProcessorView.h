#pragma once

#include <Utilities.h>
#include <view/BasicWindow.h>
#include <state/Project.h>
#include <ProcessorGraph.h>
#include <StatefulAudioProcessorContainer.h>
#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "GraphEditorPin.h"
#include "view/processor_editor/ParametersPanel.h"
#include "GraphEditorProcessor.h"

class TrackOutputProcessorView : public Component, public ValueTree::Listener {
public:
    TrackOutputProcessorView(Project &project, TracksState &tracks, ViewState &view,
                         const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(view), state(state), connectorDragListener(connectorDragListener),
              audioProcessorContainer(project), pluginManager(project.getPluginManager()) {
        this->state.addListener(this);
    }

    ~TrackOutputProcessorView() override {
        if (parametersPanel != nullptr)
            parametersPanel->removeMouseListener(this);
        state.removeListener(this);
    }

    const ValueTree &getState() const {
        return state;
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (!state.isValid())
            return {};
        return ProcessorGraph::getNodeIdForState(state);
    }

    inline int getTrackIndex() const { return tracks.indexOf(state.getParent()); }

    inline int getNumInputChannels() const { return state.getChildWithName(IDs::INPUT_CHANNELS).getNumChildren(); }

    inline int getNumOutputChannels() const { return state.getChildWithName(IDs::OUTPUT_CHANNELS).getNumChildren(); }

    inline bool acceptsMidi() const { return state[IDs::acceptsMidi]; }

    inline bool producesMidi() const { return state[IDs::producesMidi]; }

    void paint(Graphics &g) override {
        g.setColour(findColour(Label::backgroundColourId));
        g.fillRect(getBoxBounds());
    }

    void mouseDown(const MouseEvent &e) override {
        bool isTrackSelected = state.getParent()[IDs::selected];
        project.setTrackSelected(state, !(isTrackSelected && e.mods.isCommandDown()), !(isTrackSelected || e.mods.isCommandDown()));
    }

    void mouseUp(const MouseEvent &e) override {
    }

    bool hitTest(int x, int y) override {
        for (auto *child : getChildren())
            if (child->getBounds().contains(x, y))
                return true;

        return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
    }

    void resized() override {
        if (auto *processor = getAudioProcessor()) {
            auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();
            for (auto *pin : pins) {
                const bool isInput = pin->isInput();
                auto channelIndex = pin->getChannel();
                int busIdx = 0;
                processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIdx);

                int total = isInput ? getNumInputChannels() : getNumOutputChannels();

                const int index = pin->isMidi() ? (total - 1) : channelIndex;

                auto totalSpaces = static_cast<float> (total) +
                                   (static_cast<float> (jmax(0, processor->getBusCount(isInput) - 1)) * 0.5f);
                auto indexPos = static_cast<float> (index) + (static_cast<float> (busIdx) * 0.5f);

                int centerX = proportionOfWidth((1.0f + indexPos) / (totalSpaces + 1.0f));
                pin->setBounds(centerX - pinSize / 2, pin->isInput() ? 0 : (getHeight() - pinSize), pinSize, pinSize);
            }

            if (parametersPanel != nullptr) {
                parametersPanel->setBounds(boxBoundsFloat.toNearestInt());
            }
            repaint();
            connectorDragListener.update();
        }
    }

    juce::Point<float> getPinPos(int index, bool isInput) const {
        for (auto *pin : pins)
            if (pin->getChannel() == index && isInput == pin->isInput())
                return pin->getBounds().getCentre().toFloat();

        return {};
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) {
        auto *comp = getComponentAt(e.getEventRelativeTo(this).position.toInt());
        return dynamic_cast<GraphEditorPin *> (comp);
    }

    StatefulAudioProcessorWrapper *getProcessorWrapper() const {
        return audioProcessorContainer.getProcessorWrapperForState(state);
    }

    AudioProcessor *getAudioProcessor() const {
        if (auto *processorWrapper = getProcessorWrapper())
            return processorWrapper->processor;

        return {};
    }

    void showWindow(PluginWindow::Type type) {
        state.setProperty(IDs::pluginWindowType, int(type),  &project.getUndoManager());
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ValueTree state;
    std::unique_ptr<ParametersPanel> parametersPanel;
    ConnectorDragListener &connectorDragListener;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    OwnedArray<GraphEditorPin> pins;
    int pinSize = 16;

    Rectangle<int> getBoxBounds() {
        auto r = getLocalBounds().reduced(1);
        if (getNumInputChannels() > 0)
            r.setTop(pinSize);
        if (getNumOutputChannels() > 0)
            r.setBottom(getHeight() - pinSize);
        return r;
    }

    GraphEditorPin *findPinWithState(const ValueTree &state) {
        for (auto *pin : pins)
            if (pin->getState() == state)
                return pin;

        return nullptr;
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (parametersPanel == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                if (auto *defaultProcessor = dynamic_cast<DefaultAudioProcessor *>(processorWrapper->processor)) {
                    if (defaultProcessor->showInlineEditor()) {
                        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1, processorWrapper->getNumParameters())).get());
                        parametersPanel->addMouseListener(this, true);
                        parametersPanel->addParameter(processorWrapper->getParameter(0));
                        parametersPanel->addParameter(processorWrapper->getParameter(1));
                        resized();
                    }
                }
            }
        }

        resized();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pin = new GraphEditorPin(child, connectorDragListener);
            addAndMakeVisible(pin);
            pins.add(pin);
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pinToRemove = findPinWithState(child);
            pins.removeObject(pinToRemove);
        }
    }
};
