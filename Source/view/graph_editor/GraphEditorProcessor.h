#pragma once

#include <Utilities.h>
#include <BasicWindow.h>
#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "ProcessorGraph.h"
#include "GraphEditorPin.h"
#include "view/processor_editor/ParametersPanel.h"

class GraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    GraphEditorProcessor(const ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph, bool showChannelLabels=false)
            : state(state), connectorDragListener(connectorDragListener), graph(graph), showChannelLabels(showChannelLabels) {
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

    const ValueTree& getState() const {
        return state;
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (!state.isValid())
            return ProcessorGraph::NA_NODE_ID;
        return ProcessorGraph::getNodeIdForState(state);
    }

    inline int getSlot() const {
        return state[IDs::processorSlot];
    }

    inline int getNumInputChannels() const {
        return state.getChildWithName(IDs::INPUT_CHANNELS).getNumChildren();
    }

    inline int getNumOutputChannels() const {
        return state.getChildWithName(IDs::OUTPUT_CHANNELS).getNumChildren();
    }

    inline bool acceptsMidi() const {
        return state[IDs::acceptsMidi];
    }

    inline bool producesMidi() const {
        return state[IDs::producesMidi];
    }

    inline bool isIoProcessor() const {
        return InternalPluginFormat::isIoProcessorName(state[IDs::name]);
    }

    inline bool isSelected() {
        return state[IDs::selected];
    }

    inline void setSelected(bool selected, ValueTree::Listener *listenerToExclude=nullptr) {
        if (isSelected() && selected && listenerToExclude == nullptr)
            state.sendPropertyChangeMessage(IDs::selected);
        else
            state.setPropertyExcludingListener(listenerToExclude, IDs::selected, selected, nullptr);
    }

    void paint(Graphics &g) override {
        bool selected = isSelected();
        if (selected) {
            g.setColour(Colours::white.withAlpha(0.15f));
            g.fillRect(getLocalBounds());
        }

        auto boxColour = findColour(TextEditor::backgroundColourId);

        if (state[IDs::bypassed])
            boxColour = boxColour.brighter();
        else if (selected)
            boxColour = boxColour.brighter(0.02);

        g.setColour(boxColour);
        g.fillRect(getBoxBounds());
    }

    void mouseDown(const MouseEvent &e) override {
        if (state.getParent().hasType(IDs::TRACK))
            setSelected(true);
        if (e.mods.isPopupMenu())
            showPopupMenu();
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.mouseWasDraggedSinceMouseDown()) {
        } else if (e.getNumberOfClicks() == 2) {
            if (getProcessor()->hasEditor()) {
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
        auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();
        if (auto *processor = getProcessor()) {
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
                    auto& channelLabel = pin->channelLabel;
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
        }
    }

    Point<float> getPinPos(int index, bool isInput) const {
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

    void update() {
        resized();
        repaint();
    }

    AudioProcessorGraph::Node::Ptr getNode() const {
        return graph.getNodeForState(state);
    }

    StatefulAudioProcessorWrapper *getProcessorWrapper() {
        return graph.getProcessorWrapperForState(state);
    }

    AudioProcessor *getProcessor() const {
        if (auto node = getNode())
            return node->getProcessor();

        return {};
    }

    void showPopupMenu() {
        menu = std::make_unique<PopupMenu>();

        if (isIoProcessor()) {
            menu->addItem(CONFIGURE_AUDIO_MIDI_MENU_ID, "Configure audio/MIDI IO");
        } else {
            menu->addItem(DELETE_MENU_ID, "Delete this processor");
            menu->addItem(TOGGLE_BYPASS_MENU_ID, "Toggle Bypass");
        }
        menu->addSeparator();
        menu->addItem(ALLOW_DEFAULTS_MENU_ID, "Allow default connections");
        menu->addItem(DISALLOW_DEFAULTS_MENU_ID, "Disallow default connections");
        menu->addItem(DISCONNECT_ALL_MENU_ID, "Disconnect all");
        menu->addItem(DISCONNECT_CUSTOM_MENU_ID, "Disconnect all custom");

        if (getProcessor()->hasEditor()) {
            menu->addSeparator();
            menu->addItem(SHOW_PLUGIN_GUI_MENU_ID, "Show plugin GUI");
            menu->addItem(SHOW_ALL_PROGRAMS_MENU_ID, "Show all programs");
        }

        menu->showMenuAsync({}, ModalCallbackFunction::create
                ([this](int r) {
                    switch (r) {
                        case DELETE_MENU_ID:
                            graph.removeNode(getNodeId());
                            break;
                        case TOGGLE_BYPASS_MENU_ID:
                            state.setProperty(IDs::bypassed, !state.getProperty(IDs::bypassed), &graph.undoManager);
                            break;
                        case ALLOW_DEFAULTS_MENU_ID:
                            graph.setDefaultConnectionsAllowed(getNodeId(), true);
                            break;
                        case DISCONNECT_ALL_MENU_ID:
                            graph.disconnectNode(getNodeId());
                            break;
                        case DISALLOW_DEFAULTS_MENU_ID:
                            graph.setDefaultConnectionsAllowed(getNodeId(), false);
                            break;
                        case DISCONNECT_CUSTOM_MENU_ID:
                            graph.disconnectCustom(getNodeId());
                            break;
                        case SHOW_PLUGIN_GUI_MENU_ID:
                            showWindow(PluginWindow::Type::normal);
                            break;
                        case SHOW_ALL_PROGRAMS_MENU_ID:
                            showWindow(PluginWindow::Type::programs);
                            break;
                        case CONFIGURE_AUDIO_MIDI_MENU_ID:
                            getCommandManager().invokeDirectly(CommandIDs::showAudioMidiSettings, false);
                            break;
                        default:
                            break;
                    }
                }));
    }

    void showWindow(PluginWindow::Type type) {
        if (auto node = getNode())
            if (auto *w = graph.getOrCreateWindowFor(node, type))
                w->toFront(true);
    }


    class ElementComparator {
    public:
        static int compareElements(GraphEditorProcessor *first, GraphEditorProcessor *second) {
            return first->getName().compare(second->getName());
        }
    };
    ValueTree state;

private:
    DrawableText nameLabel;
    std::unique_ptr<ParametersPanel> parametersPanel;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    OwnedArray<GraphEditorPin> pins;
    const bool showChannelLabels;
    int pinSize = 16;
    float largeFontHeight = 18.0f;
    float smallFontHeight = 15.0f;
    std::unique_ptr<PopupMenu> menu;

    static constexpr int
            DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ALLOW_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
            DISALLOW_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
            SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12;

    Rectangle<int> getBoxBounds() {
        auto r = getLocalBounds().reduced(1);
        if (getNumInputChannels() > 0)
            r.setTop(pinSize);
        if (getNumOutputChannels() > 0)
            r.setBottom(getHeight() - pinSize);
        return r;
    }

    // Rotate text to draw vertically if the box is taller than it is wide.
    static Parallelogram<float> rotateRectIfNarrow(Rectangle<float>& rectangle) {
        if (rectangle.getWidth() > rectangle.getHeight())
            return rectangle;
        else
            return Parallelogram<float>(rectangle.getBottomLeft(), rectangle.getTopLeft(), rectangle.getBottomRight());
    }

    GraphEditorPin* findPinWithState(const ValueTree& state) {
        for (auto* pin : pins) {
            if (pin->state == state)
                return pin;
        }
        return nullptr;
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v.hasType(IDs::PARAM)) {
            if (!isSelected()) {
                // If we're looking at something else, change the focus so we know what's changing.
                setSelected(true);
            }
        }
        if (v != state)
            return;

        if (parametersPanel == nullptr) {
            if (auto* processorWrapper = getProcessorWrapper()) {
                if (auto* defaultProcessor = dynamic_cast<DefaultAudioProcessor *>(processorWrapper->processor)) {
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

        repaint();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pin = new GraphEditorPin(child, connectorDragListener);
            addAndMakeVisible(pin);
            pins.add(pin);
            if (showChannelLabels) {
                auto& channelLabel = pin->channelLabel;
                channelLabel.setColour(findColour(TextEditor::textColourId));
                channelLabel.setFontHeight(smallFontHeight);
                channelLabel.setJustification(Justification::centred);
                addAndMakeVisible(channelLabel);
            }
            if (auto* component = dynamic_cast<Component *>(&connectorDragListener)) {
                component->resized();
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pinToRemove = findPinWithState(child);
            if (showChannelLabels)
                removeChildComponent(&pinToRemove->channelLabel);
            pins.removeObject(pinToRemove);
            if (auto* component = dynamic_cast<Component *>(&connectorDragListener)) {
                component->resized();
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}
};
