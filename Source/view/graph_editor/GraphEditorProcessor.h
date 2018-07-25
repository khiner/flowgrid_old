#pragma once

#include <Utilities.h>
#include "JuceHeader.h"

class GraphEditorProcessor : public Component, public Utilities::ValueTreePropertyChangeListener, private AudioProcessorParameter::Listener {
public:
    GraphEditorProcessor(const ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : state(state), connectorDragListener(connectorDragListener), graph(graph) {
        if (auto *processor = getProcessor()) {
            if (auto *bypassParam = processor->getBypassParameter())
                bypassParam->addListener(this);
        }

        this->state.addListener(this);
    }

    ~GraphEditorProcessor() override {
        state.removeListener(this);
        if (auto *processor = getProcessor()) {
            if (auto *bypassParam = processor->getBypassParameter())
                bypassParam->removeListener(this);
        }
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (!state.isValid())
            return ProcessorGraph::NA_NODE_ID;
        return AudioProcessorGraph::NodeID(int(state.getProperty(IDs::NODE_ID)));
    }

    int getSlot() const {
        return state.getProperty(IDs::PROCESSOR_SLOT);
    }

    void paint(Graphics &g) override {
        auto boxArea = getLocalBounds().reduced(1, pinSize);
        bool isBypassed = false;

        if (auto *f = graph.getNodeForId(getNodeId()))
            isBypassed = f->isBypassed();

        bool selected = isSelected();
        if (selected) {
            auto bgColour = findColour(ResizableWindow::backgroundColourId).brighter(0.15);
            g.setColour(bgColour);
            g.fillRect(getLocalBounds());
        }

        auto boxColour = findColour(TextEditor::backgroundColourId);

        if (isBypassed)
            boxColour = boxColour.brighter();
        else if (selected)
            boxColour = boxColour.brighter(0.02);

        g.setColour(boxColour);
        g.fillRect(boxArea.toFloat());

        g.setColour(findColour(TextEditor::textColourId));
        g.setFont(font);
        g.drawFittedText(getName(), boxArea, Justification::centred, 2);
    }


    void mouseDown(const MouseEvent &e) override {
        originalPos = e.getPosition();
        toFront(true);
        graph.beginDraggingNode(getNodeId());

        if (e.mods.isPopupMenu())
            showPopupMenu();
    }

    void mouseDrag(const MouseEvent &e) override {
        if (!e.mods.isPopupMenu()) {
            auto pos = originalPos + e.getOffsetFromDragStart();
            graph.setNodePosition(getNodeId(), {pos.x / (double) getWidth(), pos.y / (double) getHeight()});
        }
    }

    void mouseUp(const MouseEvent &e) override {
        graph.endDraggingNode(getNodeId());
        if (e.mouseWasDraggedSinceMouseDown()) {
        } else if (e.getNumberOfClicks() == 2) {
            showPopupMenu();
        }
    }

    bool hitTest(int x, int y) override {
        for (auto *child : getChildren())
            if (child->getBounds().contains(x, y))
                return true;

        return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
    }

    void resized() override {
        if (auto *processor = getProcessor()) {
            for (auto *pin : pins) {
                const bool isInput = pin->isInput;
                auto channelIndex = pin->pin.channelIndex;
                int busIdx = 0;
                processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIdx);

                const int total = isInput ? numIns : numOuts;
                const int index = pin->pin.isMIDI() ? (total - 1) : channelIndex;

                auto totalSpaces = static_cast<float> (total) +
                                   (static_cast<float> (jmax(0, processor->getBusCount(isInput) - 1)) * 0.5f);
                auto indexPos = static_cast<float> (index) + (static_cast<float> (busIdx) * 0.5f);

                pin->setBounds(proportionOfWidth((1.0f + indexPos) / (totalSpaces + 1.0f)) - pinSize / 2,
                               pin->isInput ? 0 : (getHeight() - pinSize),
                               pinSize, pinSize);
            }
            repaint();
        }
    }

    Point<float> getPinPos(int index, bool isInput) const {
        for (auto *pin : pins)
            if (pin->pin.channelIndex == index && isInput == pin->isInput)
                return getPosition().toFloat() + pin->getBounds().getCentre().toFloat();

        return {};
    }

    PinComponent *findPinAt(const MouseEvent &e) {
        auto e2 = e.getEventRelativeTo(this);
        auto *comp = getComponentAt(e2.position.toInt());
        if (auto *pin = dynamic_cast<PinComponent *> (comp)) {
            return pin;
        }
        return nullptr;
    }

    void update() {
        AudioProcessor *processor = getProcessor();
        if (processor == nullptr)
            return;

        numIns = processor->getTotalNumInputChannels();
        if (processor->acceptsMidi())
            ++numIns;

        numOuts = processor->getTotalNumOutputChannels();
        if (processor->producesMidi())
            ++numOuts;

        setName(processor->getName());

        if (numIns != numInputs || numOuts != numOutputs) {
            numInputs = numIns;
            numOutputs = numOuts;

            pins.clear();

            auto nodeId = getNodeId();
            for (int i = 0; i < processor->getTotalNumInputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, processor, nodeId, i, true)));

            if (processor->acceptsMidi())
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, processor, nodeId, AudioProcessorGraph::midiChannelIndex, true)));

            for (int i = 0; i < processor->getTotalNumOutputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, processor, nodeId, i, false)));

            if (processor->producesMidi())
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, processor, nodeId, AudioProcessorGraph::midiChannelIndex, false)));

            resized();
        }
    }

    AudioProcessor *getProcessor() const {
        if (auto node = graph.getNodeForId(getNodeId()))
            return node->getProcessor();

        return {};
    }

    bool isSelected() {
        return graph.isSelected(getNodeId());
    }

    void showPopupMenu() {
        menu = std::make_unique<PopupMenu>();
        menu->addItem(1, "Delete this processor");
        menu->addItem(2, "Disconnect all pins");
        menu->addItem(3, "Toggle Bypass");

        if (getProcessor()->hasEditor()) {
            menu->addSeparator();
            menu->addItem(10, "Show plugin GUI");
            menu->addItem(11, "Show all programs");
            menu->addItem(12, "Show all parameters");
        }

        menu->showMenuAsync({}, ModalCallbackFunction::create
                ([this](int r) {
                    switch (r) {
                        case 1:
                            graph.removeNode(getNodeId());
                            break;
                        case 2:
                            graph.disconnectNode(getNodeId());
                            break;
                        case 3: {
                            if (auto *node = graph.getNodeForId(getNodeId()))
                                node->setBypassed(!node->isBypassed());
                            repaint();
                            break;
                        }
                        case 10:
                            showWindow(PluginWindow::Type::normal);
                            break;
                        case 11:
                            showWindow(PluginWindow::Type::programs);
                            break;
                        case 12:
                            showWindow(PluginWindow::Type::generic);
                            break;
                        default:
                            break;
                    }
                }));
    }

    void showWindow(PluginWindow::Type type) {
        if (auto node = graph.getNodeForId(getNodeId()))
            if (auto *w = graph.getOrCreateWindowFor(node, type))
                w->toFront(true);
    }

    void parameterValueChanged(int, float) override {
        repaint();
    }

    void parameterGestureChanged(int, bool) override {}

    ValueTree state;

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    OwnedArray<PinComponent> pins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 16;
    Point<int> originalPos;
    Font font{13.0f, Font::bold};
    int numIns = 0, numOuts = 0;
    std::unique_ptr<PopupMenu> menu;

private:
    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v == state)
            if (i == IDs::PROCESSOR_SLOT)
                repaint();
    }
};
