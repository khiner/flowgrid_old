#pragma once

#include <Utilities.h>
#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "ProcessorGraph.h"
#include "PinComponent.h"

class GraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    GraphEditorProcessor(const ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : state(state), connectorDragListener(connectorDragListener), graph(graph) {
        this->state.addListener(this);
        valueTreePropertyChanged(this->state, IDs::name);
        if (this->state.hasProperty(IDs::deviceName))
            valueTreePropertyChanged(this->state, IDs::deviceName);
        drawableText.setColour(findColour(TextEditor::textColourId));
        drawableText.setFont(font, true);
        drawableText.setJustification(Justification::centred);
        addAndMakeVisible(drawableText);

        for (auto child : state) {
            if (child.hasType(IDs::INPUT_CHANNELS) || child.hasType(IDs::OUTPUT_CHANNELS)) {
                for (auto channel : child) {
                    valueTreeChildAdded(child, channel);
                }
            }
        }
    }

    ~GraphEditorProcessor() override {
        state.removeListener(this);
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (!state.isValid())
            return ProcessorGraph::NA_NODE_ID;
        return ProcessorGraph::getNodeIdForState(state);
    }

    int getSlot() const {
        return state[IDs::processorSlot];
    }

    int getNumInputChannels() const {
        return state[IDs::numInputChannels];
    }

    int getNumOutputChannels() const {
        return state[IDs::numOutputChannels];
    }

    bool acceptsMidi() const {
        return state[IDs::acceptsMidi];
    }

    bool producesMidi() const {
        return state[IDs::producesMidi];
    }

    bool isIoProcessor() const {
        return InternalPluginFormat::isIoProcessorName(state[IDs::name]);
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
        g.fillRect(getLocalBounds().reduced(1, pinSize).toFloat());
    }

    void mouseDown(const MouseEvent &e) override {
        toFront(true);
        if (e.mods.isPopupMenu())
            showPopupMenu();
    }

    void mouseUp(const MouseEvent &e) override {
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
                const bool isInput = pin->isInput();
                auto channelIndex = pin->getChannel();
                int busIdx = 0;
                processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIdx);

                int total = isInput ? getNumInputChannels() : getNumOutputChannels();
                if (isInput && acceptsMidi())
                    total += 1;
                else if (!isInput && producesMidi())
                    total += 1;

                const int index = pin->isMidi() ? (total - 1) : channelIndex;

                auto totalSpaces = static_cast<float> (total) +
                                   (static_cast<float> (jmax(0, processor->getBusCount(isInput) - 1)) * 0.5f);
                auto indexPos = static_cast<float> (index) + (static_cast<float> (busIdx) * 0.5f);

                pin->setBounds(proportionOfWidth((1.0f + indexPos) / (totalSpaces + 1.0f)) - pinSize / 2,
                               pin->isInput() ? 0 : (getHeight() - pinSize),
                               pinSize, pinSize);
            }

            auto boxArea = getLocalBounds().reduced(1, pinSize).toFloat();
            const auto &textArea = boxArea.getWidth() > boxArea.getHeight() ?
                    boxArea.toFloat() : // Rotate text to draw vertically if the box is taller than it is wide.
                    Parallelogram<float>(boxArea.getBottomLeft(), boxArea.getTopLeft(), boxArea.getBottomRight());
            drawableText.setBoundingBox(textArea);
        }
    }

    Point<float> getPinPos(int index, bool isInput) const {
        for (auto *pin : pins)
            if (pin->getChannel() == index && isInput == pin->isInput())
                return pin->getBounds().getCentre().toFloat();

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
        resized();
        repaint();
    }

    AudioProcessorGraph::Node::Ptr getNode() const {
        return graph.getNodeForId(getNodeId());
    }

    AudioProcessor *getProcessor() const {
        if (auto node = getNode())
            return node->getProcessor();

        return {};
    }

    bool isSelected() {
        return state[IDs::selected];
    }

    void showPopupMenu() {
        menu = std::make_unique<PopupMenu>();

        if (isIoProcessor()) {
            menu->addItem(CONFIGURE_AUDIO_MIDI_MENU_ID, "Configure audio/MIDI IO");
        } else if (state[IDs::name] == MidiKeyboardProcessor::name()) {
            menu->addItem(SHOW_MIDI_KEYBOARD_MENU_ID, "Show onscreen MIDI keyboard");
        } else {
            menu->addItem(DELETE_MENU_ID, "Delete this processor");
            menu->addItem(TOGGLE_BYPASS_MENU_ID, "Toggle Bypass");
        }
        menu->addSeparator();
        menu->addItem(CONNECT_DEFAULTS_MENU_ID, "Connect all defaults");
        menu->addItem(DISCONNECT_ALL_MENU_ID, "Disconnect all");
        menu->addItem(DISCONNECT_DEFAULTS_MENU_ID, "Disconnect all defaults");
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
                        case CONNECT_DEFAULTS_MENU_ID:
                            graph.connectDefaults(getNodeId());
                            break;
                        case DISCONNECT_ALL_MENU_ID:
                            graph.disconnectNode(getNodeId());
                            break;
                        case DISCONNECT_DEFAULTS_MENU_ID:
                            graph.disconnectDefaults(getNodeId());
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
                        case SHOW_MIDI_KEYBOARD_MENU_ID:
                            if (auto* midiKeyboardProcessor = dynamic_cast<MidiKeyboardProcessor *>(getProcessor())) {
                                auto *keyboardComponent = midiKeyboardProcessor->createKeyboard();
                                keyboardComponent->setSize(800, 1);
                                DialogWindow::LaunchOptions o;
                                o.content.setOwned(keyboardComponent);
                                o.dialogTitle = "Keyboard";
                                o.componentToCentreAround = this;
                                o.dialogBackgroundColour = findColour(ResizableWindow::backgroundColourId);
                                o.escapeKeyTriggersCloseButton = true;
                                o.useNativeTitleBar = false;
                                o.resizable = true;

                                o.create()->enterModalState(true, nullptr, true);
                                keyboardComponent->grabKeyboardFocus();
                            }
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

    ValueTree state;

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    OwnedArray<PinComponent> pins;
    int pinSize = 16;
    Font font{13.0f, Font::bold};
    std::unique_ptr<PopupMenu> menu;

    class ElementComparator {
    public:
        static int compareElements(GraphEditorProcessor *first, GraphEditorProcessor *second) {
            return first->getName().compare(second->getName());
        }
    };
private:
    DrawableText drawableText;

    static constexpr int
            DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, CONNECT_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
            DISCONNECT_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
            SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12,
            SHOW_MIDI_KEYBOARD_MENU_ID = 13;

    PinComponent* findPinWithState(const ValueTree& state) {
        for (auto* pin : pins) {
            if (pin->state == state)
                return pin;
        }
        return nullptr;
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (i == IDs::deviceName) {
            setName(v[IDs::deviceName]);
            drawableText.setText(getName());
        } else if (i == IDs::name) {
            setName(v[IDs::name]);
            drawableText.setText(getName());
        }

        repaint();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *pin = new PinComponent(child, connectorDragListener);
            addAndMakeVisible(pin);
            pins.add(pin);
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override {
        if (child.hasType(IDs::CHANNEL)) {
            pins.removeObject(findPinWithState(child));
        }
    }

    void valueTreeChildOrderChanged(ValueTree &, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}
};
