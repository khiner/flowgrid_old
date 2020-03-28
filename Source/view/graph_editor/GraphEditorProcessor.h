#pragma once

#include <Utilities.h>
#include <view/BasicWindow.h>
#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "GraphEditorPin.h"
#include "view/processor_editor/ParametersPanel.h"

class GraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    GraphEditorProcessor(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener, bool showChannelLabels = false)
            : project(project), tracks(tracks), state(state), connectorDragListener(connectorDragListener),
              audioProcessorContainer(project), pluginManager(project.getPluginManager()), showChannelLabels(showChannelLabels) {
        this->state.addListener(this);
        valueTreePropertyChanged(this->state, IDs::name);
        if (this->state.hasProperty(IDs::deviceName))
            valueTreePropertyChanged(this->state, IDs::deviceName);
        if (!showChannelLabels) {
            nameLabel.setColour(findColour(TextEditor::textColourId));
            nameLabel.setFontHeight(largeFontHeight);
            nameLabel.setJustification(Justification::centred);
            addAndMakeVisible(nameLabel);
        }

        for (auto child : state) {
            if (child.hasType(IDs::INPUT_CHANNELS) || child.hasType(IDs::OUTPUT_CHANNELS)) {
                for (auto channel : child) {
                    valueTreeChildAdded(child, channel);
                }
            }
        }
    }

    ~GraphEditorProcessor() override {
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

    inline int getSlot() const { return state[IDs::processorSlot]; }

    inline int getNumInputChannels() const { return state.getChildWithName(IDs::INPUT_CHANNELS).getNumChildren(); }

    inline int getNumOutputChannels() const { return state.getChildWithName(IDs::OUTPUT_CHANNELS).getNumChildren(); }

    inline bool acceptsMidi() const { return state[IDs::acceptsMidi]; }

    inline bool producesMidi() const { return state[IDs::producesMidi]; }

    inline bool isIoProcessor() const { return InternalPluginFormat::isIoProcessorName(state[IDs::name]); }

    inline bool isSelected() { return tracks.isProcessorSelected(state); }

    void paint(Graphics &g) override {
        bool selected = isSelected();

        auto boxColour = findColour(TextEditor::backgroundColourId);
        if (state[IDs::bypassed])
            boxColour = boxColour.brighter();
        else if (selected)
            boxColour = boxColour.brighter(0.02);

        g.setColour(boxColour);
        g.fillRect(getBoxBounds());
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.mouseWasDraggedSinceMouseDown()) {
        } else if (e.getNumberOfClicks() == 2) {
            if (getAudioProcessor()->hasEditor()) {
                showWindow(PluginWindow::Type::normal);
            }
        }
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
                if (showChannelLabels) {
                    auto &channelLabel = pin->channelLabel;
                    auto textArea = boxBoundsFloat.withWidth(proportionOfWidth(1.0f / totalSpaces)).withCentre({float(centerX), boxBoundsFloat.getCentreY()});
                    channelLabel.setBoundingBox(rotateRectIfNarrow(textArea));
                }
            }

            if (!showChannelLabels) {
                nameLabel.setBoundingBox(rotateRectIfNarrow(boxBoundsFloat));
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
        auto e2 = e.getEventRelativeTo(this);
        auto *comp = getComponentAt(e2.position.toInt());
        if (auto *pin = dynamic_cast<GraphEditorPin *> (comp)) {
            return pin;
        }
        return nullptr;
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

    class ElementComparator {
    public:
        static int compareElements(GraphEditorProcessor *first, GraphEditorProcessor *second) {
            return first->getName().compare(second->getName());
        }
    };

private:
    Project &project;
    TracksState &tracks;
    ValueTree state;
    DrawableText nameLabel;
    std::unique_ptr<ParametersPanel> parametersPanel;
    ConnectorDragListener &connectorDragListener;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    OwnedArray<GraphEditorPin> pins;
    const bool showChannelLabels;
    int pinSize = 16;
    float largeFontHeight = 18.0f;
    float smallFontHeight = 15.0f;

    Rectangle<int> getBoxBounds() {
        auto r = getLocalBounds().reduced(1);
        if (getNumInputChannels() > 0)
            r.setTop(pinSize);
        if (getNumOutputChannels() > 0)
            r.setBottom(getHeight() - pinSize);
        return r;
    }

    // Rotate text to draw vertically if the box is taller than it is wide.
    static Parallelogram<float> rotateRectIfNarrow(Rectangle<float> &rectangle) {
        if (rectangle.getWidth() > rectangle.getHeight())
            return rectangle;
        else
            return Parallelogram<float>(rectangle.getBottomLeft(), rectangle.getTopLeft(), rectangle.getBottomRight());
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
                        removeChildComponent(&nameLabel);
                        resized();
                    }
                }
            }
        }
        if (i == IDs::deviceName) {
            setName(v[IDs::deviceName]);
            nameLabel.setText(getName());
        } else if (i == IDs::name) {
            setName(v[IDs::name]);
            nameLabel.setText(getName());
        }

        resized();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pin = new GraphEditorPin(child, connectorDragListener);
            addAndMakeVisible(pin);
            pins.add(pin);
            if (showChannelLabels) {
                auto &channelLabel = pin->channelLabel;
                channelLabel.setColour(findColour(TextEditor::textColourId));
                channelLabel.setFontHeight(smallFontHeight);
                channelLabel.setJustification(Justification::centred);
                addAndMakeVisible(channelLabel);
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pinToRemove = findPinWithState(child);
            if (showChannelLabels)
                removeChildComponent(&pinToRemove->channelLabel);
            pins.removeObject(pinToRemove);
        }
    }
};
