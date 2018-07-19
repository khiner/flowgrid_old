#pragma once

#include "JuceHeader.h"
#include "PinComponent.h"
#include "ProcessorGraph.h"

struct ProcessorComponent : public Component, private AudioProcessorParameter::Listener {
    ProcessorComponent(ConnectorDragListener &connectorDragListener, ProcessorGraph& graph, AudioProcessorGraph::NodeID nodeId)
            : connectorDragListener(connectorDragListener), graph(graph), nodeId(nodeId) {
        if (auto f = graph.getNodeForId(nodeId)) {
            if (auto *processor = f->getProcessor()) {
                if (auto *bypassParam = processor->getBypassParameter())
                    bypassParam->addListener(this);
            }
        }

        setSize(150, 60);
    }

    ProcessorComponent(const ProcessorComponent &) = delete;

    ProcessorComponent &operator=(const ProcessorComponent &) = delete;

    ~ProcessorComponent() override {
        if (auto f = graph.getNodeForId(nodeId)) {
            if (auto *processor = f->getProcessor()) {
                if (auto *bypassParam = processor->getBypassParameter())
                    bypassParam->removeListener(this);
            }
        }
    }

    void mouseDown(const MouseEvent &e) override {
        originalPos = localPointToGlobal(juce::Point<int>());

        toFront(true);
        graph.beginDraggingNode(nodeId);

        if (e.mods.isPopupMenu())
            showPopupMenu();
    }

    void mouseDrag(const MouseEvent &e) override {
        if (!e.mods.isPopupMenu()) {
            auto pos = originalPos + e.getOffsetFromDragStart();

            if (getParentComponent() != nullptr)
                pos = getParentComponent()->getLocalPoint(nullptr, pos);

            pos += getLocalBounds().getCentre();

            graph.setNodePosition(nodeId,
                                  {pos.x / (double) getParentWidth(),
                                   pos.y / (double) getParentHeight()});
        }
    }

    void mouseUp(const MouseEvent &e) override {
        graph.endDraggingNode(nodeId);
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

    void paint(Graphics &g) override {
        auto boxArea = getLocalBounds().reduced(1, pinSize);
        bool isBypassed = false;

        if (auto *f = graph.getNodeForId(nodeId))
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

    void resized() override {
        if (auto f = graph.getNodeForId(nodeId)) {
            if (auto *processor = f->getProcessor()) {
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
            }
        }
    }

    Point<float> getPinPos(int index, bool isInput) const {
        for (auto *pin : pins)
            if (pin->pin.channelIndex == index && isInput == pin->isInput)
                return getPosition().toFloat() + pin->getBounds().getCentre().toFloat();

        return {};
    }

    void update() {
        const AudioProcessorGraph::Node::Ptr node(graph.getNodeForId(nodeId));
        jassert (node != nullptr);

        numIns = node->getProcessor()->getTotalNumInputChannels();
        if (node->getProcessor()->acceptsMidi())
            ++numIns;

        numOuts = node->getProcessor()->getTotalNumOutputChannels();
        if (node->getProcessor()->producesMidi())
            ++numOuts;

        int w = getParentWidth() / Project::NUM_VISIBLE_TRACKS;
        int h = getParentHeight() / Project::NUM_VISIBLE_PROCESSOR_SLOTS;

        //w = jmax(w, (jmax(numIns, numOuts) + 1) * 20);

        //const int textWidth = font.getStringWidth(f->getProcessor()->getName());

        setSize(w, h);

        setName(node->getProcessor()->getName());

        {
            auto p = graph.getNodePosition(nodeId);
            setCentreRelative((float) p.x, (float) p.y);
        }

        if (numIns != numInputs || numOuts != numOutputs) {
            numInputs = numIns;
            numOutputs = numOuts;

            pins.clear();

            for (int i = 0; i < node->getProcessor()->getTotalNumInputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, node, i, true)));

            if (node->getProcessor()->acceptsMidi())
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, node, AudioProcessorGraph::midiChannelIndex, true)));

            for (int i = 0; i < node->getProcessor()->getTotalNumOutputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, node, i, false)));

            if (node->getProcessor()->producesMidi())
                addAndMakeVisible(pins.add(new PinComponent(connectorDragListener, node, AudioProcessorGraph::midiChannelIndex, false)));

            resized();
        }
    }

    AudioProcessor *getProcessor() const {
        if (auto node = graph.getNodeForId(nodeId))
            return node->getProcessor();

        return {};
    }

    bool isSelected() {
        return graph.isSelected(nodeId);
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
                            graph.removeNode(nodeId);
                            break;
                        case 2:
                            graph.disconnectNode(nodeId);
                            break;
                        case 3: {
                            if (auto *node = graph.getNodeForId(nodeId))
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
        if (auto node = graph.getNodeForId(nodeId))
            if (auto *w = graph.getOrCreateWindowFor(node, type))
                w->toFront(true);
    }

    void parameterValueChanged(int, float) override {
        repaint();
    }

    void parameterGestureChanged(int, bool) override {}

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    const AudioProcessorGraph::NodeID nodeId;
    OwnedArray<PinComponent> pins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 16;
    Point<int> originalPos;
    Font font{13.0f, Font::bold};
    int numIns = 0, numOuts = 0;
    std::unique_ptr<PopupMenu> menu;
};
