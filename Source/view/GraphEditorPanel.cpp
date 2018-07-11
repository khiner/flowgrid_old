#include "../JuceLibraryCode/JuceHeader.h"
#include "GraphEditorPanel.h"

struct GraphEditorPanel::PinComponent : public Component,
                                        public SettableTooltipClient {
    PinComponent(GraphEditorPanel &p, AudioProcessorGraph::NodeAndChannel pinToUse, bool isIn)
            : panel(p), graph(p.graph), pin(pinToUse), isInput(isIn) {
        if (auto node = graph.getNodeForId(pin.nodeID)) {
            String tip;

            if (pin.isMIDI()) {
                tip = isInput ? "MIDI Input"
                              : "MIDI Output";
            } else {
                auto &processor = *node->getProcessor();
                auto channel = processor.getOffsetInBusBufferForAbsoluteChannelIndex(isInput, pin.channelIndex, busIdx);

                if (auto *bus = processor.getBus(isInput, busIdx))
                    tip = bus->getName() + ": " + AudioChannelSet::getAbbreviatedChannelTypeName(
                            bus->getCurrentLayout().getTypeOfChannel(channel));
                else
                    tip = (isInput ? "Main Input: "
                                   : "Main Output: ") + String(pin.channelIndex + 1);

            }

            setTooltip(tip);
        }

        setSize(16, 16);
    }

    void paint(Graphics &g) override {
        auto w = (float) getWidth();
        auto h = (float) getHeight();

        Path p;
        p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
        p.addRectangle(w * 0.4f, isInput ? (0.5f * h) : 0.0f, w * 0.2f, h * 0.5f);

        auto colour = (pin.isMIDI() ? Colours::red : Colours::green);

        g.setColour(colour.withRotatedHue(busIdx / 5.0f));
        g.fillPath(p);
    }

    void mouseDown(const MouseEvent &e) override {
        AudioProcessorGraph::NodeAndChannel dummy{0, 0};

        panel.beginConnectorDrag(isInput ? dummy : pin,
                                 isInput ? pin : dummy,
                                 e);
    }

    void mouseDrag(const MouseEvent &e) override {
        panel.dragConnector(e);
    }

    void mouseUp(const MouseEvent &e) override {
        panel.endDraggingConnector(e);
    }

    GraphEditorPanel &panel;
    ProcessorGraph &graph;
    AudioProcessorGraph::NodeAndChannel pin;
    const bool isInput;
    int busIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
};

//==============================================================================
struct GraphEditorPanel::FilterComponent : public Component,
                                           private AudioProcessorParameter::Listener {
    FilterComponent(GraphEditorPanel &p, uint32 id) : panel(p), graph(p.graph), nodeId(id) {
        if (auto f = graph.getNodeForId(nodeId)) {
            if (auto *processor = f->getProcessor()) {
                if (auto *bypassParam = processor->getBypassParameter())
                    bypassParam->addListener(this);
            }
        }

        setSize(150, 60);
    }

    FilterComponent(const FilterComponent &) = delete;

    FilterComponent &operator=(const FilterComponent &) = delete;

    ~FilterComponent() override {
        if (auto f = graph.getNodeForId(nodeId)) {
            if (auto *processor = f->getProcessor()) {
                if (auto *bypassParam = processor->getBypassParameter())
                    bypassParam->removeListener(this);
            }
        }
    }

    void mouseDown(const MouseEvent &e) override {
        originalPos = localPointToGlobal(Point < int > ());

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
            //showPopupMenu(PluginWindow::Type::normal);
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
        const AudioProcessorGraph::Node::Ptr f(graph.getNodeForId(nodeId));
        jassert (f != nullptr);

        numIns = f->getProcessor()->getTotalNumInputChannels();
        if (f->getProcessor()->acceptsMidi())
            ++numIns;

        numOuts = f->getProcessor()->getTotalNumOutputChannels();
        if (f->getProcessor()->producesMidi())
            ++numOuts;

        int w = getParentWidth() / NUM_COLUMNS;
        int h = getParentHeight() / NUM_ROWS;

        //w = jmax(w, (jmax(numIns, numOuts) + 1) * 20);

        //const int textWidth = font.getStringWidth(f->getProcessor()->getName());

        setSize(w, h);

        setName(f->getProcessor()->getName());

        {
            auto p = graph.getNodePosition(nodeId);
            setCentreRelative((float) p.x, (float) p.y);
        }

        if (numIns != numInputs || numOuts != numOutputs) {
            numInputs = numIns;
            numOutputs = numOuts;

            pins.clear();

            for (int i = 0; i < f->getProcessor()->getTotalNumInputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(panel, {nodeId, i}, true)));

            if (f->getProcessor()->acceptsMidi())
                addAndMakeVisible(
                        pins.add(new PinComponent(panel, {nodeId, AudioProcessorGraph::midiChannelIndex}, true)));

            for (int i = 0; i < f->getProcessor()->getTotalNumOutputChannels(); ++i)
                addAndMakeVisible(pins.add(new PinComponent(panel, {nodeId, i}, false)));

            if (f->getProcessor()->producesMidi())
                addAndMakeVisible(
                        pins.add(new PinComponent(panel, {nodeId, AudioProcessorGraph::midiChannelIndex}, false)));

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
        menu.reset(new PopupMenu);
        menu->addItem(1, "Delete this filter");
        menu->addItem(2, "Disconnect all pins");
        menu->addItem(3, "Toggle Bypass");

        if (getProcessor()->hasEditor()) {
            menu->addSeparator();
            menu->addItem(10, "Show plugin GUI");
            menu->addItem(11, "Show all programs");
            menu->addItem(12, "Show all parameters");
        }

        menu->addSeparator();
        menu->addItem(20, "Configure Audio I/O");
        menu->addItem(21, "Test state save/load");

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
                            //showWindow(PluginWindow::Type::normal);
                            break;
                        case 11:
                            //showWindow(PluginWindow::Type::programs);
                            break;
                        case 12:
                            //showWindow(PluginWindow::Type::generic);
                            break;
                        case 20:
                            //showWindow(PluginWindow::Type::audioIO);
                            break;
                        case 21:
                            testStateSaveLoad();
                            break;

                        default:
                            break;
                    }
                }));
    }

    void testStateSaveLoad() {
        if (auto *processor = getProcessor()) {
            MemoryBlock state;
            processor->getStateInformation(state);
            processor->setStateInformation(state.getData(), (int) state.getSize());
        }
    }

//    void showWindow(PluginWindow::Type type) {
//        if (auto node = graph.graph.getNodeForId(nodeId))
//            if (auto *w = graph.getOrCreateWindowFor(node, type))
//                w->toFront(true);
//    }

    void parameterValueChanged(int, float) override {
        repaint();
    }

    void parameterGestureChanged(int, bool) override {}

    GraphEditorPanel &panel;
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


//==============================================================================
struct GraphEditorPanel::ConnectorComponent : public Component,
                                              public SettableTooltipClient {
    ConnectorComponent(GraphEditorPanel &p) : panel(p), graph(p.graph) {
        setAlwaysOnTop(true);
    }

    void setInput(AudioProcessorGraph::NodeAndChannel newSource) {
        if (connection.source != newSource) {
            connection.source = newSource;
            update();
        }
    }

    void setOutput(AudioProcessorGraph::NodeAndChannel newDest) {
        if (connection.destination != newDest) {
            connection.destination = newDest;
            update();
        }
    }

    void dragStart(Point<float> pos) {
        lastInputPos = pos;
        resizeToFit();
    }

    void dragEnd(Point<float> pos) {
        lastOutputPos = pos;
        resizeToFit();
    }

    void update() {
        resizeToFit();
    }

    void resizeToFit() {
        Point<float> p1, p2;
        getPoints(p1, p2);

        auto newBounds = Rectangle<float>(p1, p2).expanded(4.0f).getSmallestIntegerContainer();

        if (newBounds != getBounds())
            setBounds(newBounds);
        else
            resized();

        repaint();
    }

    void getPoints(Point<float> &p1, Point<float> &p2) const {
        p1 = lastInputPos;
        p2 = lastOutputPos;

        if (auto *src = panel.getComponentForFilter(connection.source.nodeID))
            p1 = src->getPinPos(connection.source.channelIndex, false);

        if (auto *dest = panel.getComponentForFilter(connection.destination.nodeID))
            p2 = dest->getPinPos(connection.destination.channelIndex, true);
    }

    void paint(Graphics &g) override {
        if (connection.source.isMIDI() || connection.destination.isMIDI())
            g.setColour(Colours::red);
        else
            g.setColour(Colours::green);

        g.fillPath(linePath);
    }

    bool hitTest(int x, int y) override {
        auto pos = Point < int > (x, y).toFloat();

        if (hitPath.contains(pos)) {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(pos, distanceFromStart, distanceFromEnd);

            // avoid clicking the connector when over a pin
            return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
        }

        return false;
    }

    void mouseDown(const MouseEvent &) override {
        dragging = false;
    }

    void mouseDrag(const MouseEvent &e) override {
        if (dragging) {
            panel.dragConnector(e);
        } else if (e.mouseWasDraggedSinceMouseDown()) {
            dragging = true;

            graph.removeConnection(connection);

            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            AudioProcessorGraph::NodeAndChannel dummy{0, 0};

            panel.beginConnectorDrag(isNearerSource ? dummy : connection.source,
                                     isNearerSource ? connection.destination : dummy,
                                     e);
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (dragging)
            panel.endDraggingConnector(e);
    }

    void resized() override {
        Point<float> p1, p2;
        getPoints(p1, p2);

        lastInputPos = p1;
        lastOutputPos = p2;

        p1 -= getPosition().toFloat();
        p2 -= getPosition().toFloat();

        linePath.clear();
        linePath.startNewSubPath(p1);
        linePath.cubicTo(p1.x, p1.y + (p2.y - p1.y) * 0.33f,
                         p2.x, p1.y + (p2.y - p1.y) * 0.66f,
                         p2.x, p2.y);

        PathStrokeType wideStroke(8.0f);
        wideStroke.createStrokedPath(hitPath, linePath);

        PathStrokeType stroke(2.5f);
        stroke.createStrokedPath(linePath, linePath);

        auto arrowW = 5.0f;
        auto arrowL = 4.0f;

        Path arrow;
        arrow.addTriangle(-arrowL, arrowW,
                          -arrowL, -arrowW,
                          arrowL, 0.0f);

        arrow.applyTransform(AffineTransform()
                                     .rotated(MathConstants<float>::halfPi - (float) atan2(p2.x - p1.x, p2.y - p1.y))
                                     .translated((p1 + p2) * 0.5f));

        linePath.addPath(arrow);
        linePath.setUsingNonZeroWinding(true);
    }

    void getDistancesFromEnds(Point<float> p, double &distanceFromStart, double &distanceFromEnd) const {
        Point<float> p1, p2;
        getPoints(p1, p2);

        distanceFromStart = p1.getDistanceFrom(p);
        distanceFromEnd = p2.getDistanceFrom(p);
    }

    GraphEditorPanel &panel;
    ProcessorGraph &graph;
    AudioProcessorGraph::Connection connection{{0, 0},
                                               {0, 0}};
    Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};


//==============================================================================
GraphEditorPanel::GraphEditorPanel(ProcessorGraph &g) : graph(g) {
    graph.addChangeListener(this);
    setOpaque(true);
}

GraphEditorPanel::~GraphEditorPanel() {
    graph.removeChangeListener(this);
    draggingConnector = nullptr;
    nodes.clear();
    connectors.clear();
}

void GraphEditorPanel::paint(Graphics &g) {
    const Colour &bgColor = findColour(ResizableWindow::backgroundColourId);
    g.fillAll(bgColor);
    g.setColour(bgColor.brighter(0.15));
    for (int row = 0; row < NUM_ROWS; row++) {
        for (int column = 0; column < NUM_COLUMNS; column++) {
            float rowOffset = float(row) / float(NUM_ROWS);
            float columnOffset = float(column) / float(NUM_ROWS);
            g.drawHorizontalLine(int(rowOffset * getHeight()), 0, getWidth());
            g.drawVerticalLine(int(columnOffset * getWidth()), 0, getHeight());
        }
    }
}

void GraphEditorPanel::mouseDown(const MouseEvent &e) {
    if (e.mods.isPopupMenu())
        showPopupMenu(e.position.toInt());
}

void GraphEditorPanel::mouseUp(const MouseEvent &) {
}

void GraphEditorPanel::mouseDrag(const MouseEvent &e) {
}

void GraphEditorPanel::createNewPlugin(const PluginDescription &desc, Point<int> position) {
    //graph.addPlugin(desc, position.toDouble() / Point < double > ((double) getWidth(), (double) getHeight()));
}

GraphEditorPanel::FilterComponent *GraphEditorPanel::getComponentForFilter(const uint32 filterID) const {
    for (auto *fc : nodes)
        if (fc->nodeId == filterID)
            return fc;

    return nullptr;
}

GraphEditorPanel::ConnectorComponent *
GraphEditorPanel::getComponentForConnection(const AudioProcessorGraph::Connection &conn) const {
    for (auto *cc : connectors)
        if (cc->connection == conn)
            return cc;

    return nullptr;
}

GraphEditorPanel::PinComponent *GraphEditorPanel::findPinAt(Point<float> pos) const {
    for (auto *fc : nodes) {
        // NB: A Visual Studio optimiser error means we have to put this Component* in a local
        // variable before trying to cast it, or it gets mysteriously optimised away..
        auto *comp = fc->getComponentAt(pos.toInt() - fc->getPosition());

        if (auto *pin = dynamic_cast<PinComponent *> (comp))
            return pin;
    }

    return nullptr;
}

void GraphEditorPanel::resized() {
    updateComponents();
}

void GraphEditorPanel::changeListenerCallback(ChangeBroadcaster *) {
    updateComponents();
}

void GraphEditorPanel::updateComponents() {
    repaint();
    for (int i = nodes.size(); --i >= 0;)
        if (graph.getNodeForId(nodes.getUnchecked(i)->nodeId) == nullptr)
            nodes.remove(i);

    for (int i = connectors.size(); --i >= 0;)
        if (!graph.isConnectedUi(connectors.getUnchecked(i)->connection))
            connectors.remove(i);

    for (auto *fc : nodes)
        fc->update();

    for (auto *cc : connectors)
        cc->update();

    for (auto *f : graph.getNodes()) {
        FilterComponent *component = getComponentForFilter(f->nodeID);
        if (component == nullptr) {
            auto *comp = nodes.add(new FilterComponent(*this, f->nodeID));
            addAndMakeVisible(comp);
            comp->update();
        } else {
            component->repaint();
        }
    }

    for (auto &c : graph.getConnectionsUi()) {
        if (getComponentForConnection(c) == 0) {
            auto *comp = connectors.add(new ConnectorComponent(*this));
            addAndMakeVisible(comp);

            comp->setInput(c.source);
            comp->setOutput(c.destination);
        }
    }
}

void GraphEditorPanel::showPopupMenu(Point<int> mousePos) {
    menu.reset(new PopupMenu);

//    if (auto *mainWindow = findParentComponentOfClass<MainHostWindow>()) {
//        mainWindow->addPluginsToMenu(*menu);
//
//        menu->showMenuAsync({},
//                            ModalCallbackFunction::create([this, mousePos](int r) {
//                                if (auto *mainWindow = findParentComponentOfClass<MainHostWindow>())
//                                    if (auto *desc = mainWindow->getChosenType(r))
//                                        createNewPlugin(*desc, mousePos);
//                            }));
//    }
}

void GraphEditorPanel::beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source,
                                          AudioProcessorGraph::NodeAndChannel dest,
                                          const MouseEvent &e) {
    auto *c = dynamic_cast<ConnectorComponent *> (e.originalComponent);
    connectors.removeObject(c, false);
    draggingConnector.reset(c);

    if (draggingConnector == nullptr)
        draggingConnector.reset(new ConnectorComponent(*this));

    draggingConnector->setInput(source);
    draggingConnector->setOutput(dest);

    addAndMakeVisible(draggingConnector.get());
    draggingConnector->toFront(false);

    dragConnector(e);
}

void GraphEditorPanel::dragConnector(const MouseEvent &e) {
    auto e2 = e.getEventRelativeTo(this);

    if (draggingConnector != nullptr) {
        draggingConnector->setTooltip({});

        auto pos = e2.position;

        if (auto *pin = findPinAt(pos)) {
            auto connection = draggingConnector->connection;

            if (connection.source.nodeID == 0 && !pin->isInput) {
                connection.source = pin->pin;
            } else if (connection.destination.nodeID == 0 && pin->isInput) {
                connection.destination = pin->pin;
            }

            if (graph.canConnect(connection)) {
                pos = (pin->getParentComponent()->getPosition() + pin->getBounds().getCentre()).toFloat();
                draggingConnector->setTooltip(pin->getTooltip());
            }
        }

        if (draggingConnector->connection.source.nodeID == 0)
            draggingConnector->dragStart(pos);
        else
            draggingConnector->dragEnd(pos);
    }
}

void GraphEditorPanel::endDraggingConnector(const MouseEvent &e) {
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip({});

    auto e2 = e.getEventRelativeTo(this);
    auto connection = draggingConnector->connection;

    draggingConnector = nullptr;

    if (auto *pin = findPinAt(e2.position)) {
        if (connection.source.nodeID == 0) {
            if (pin->isInput)
                return;

            connection.source = pin->pin;
        } else {
            if (!pin->isInput)
                return;

            connection.destination = pin->pin;
        }

        graph.addConnection(connection);
    }
}

//==============================================================================
struct GraphDocumentComponent::TooltipBar : public Component,
                                            private Timer {
    TooltipBar() {
        startTimer(100);
    }

    void paint(Graphics &g) override {
        g.setFont(Font(getHeight() * 0.7f, Font::bold));
        g.setColour(Colours::black);
        g.drawFittedText(tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
    }

    void timerCallback() override {
        String newTip;

        if (auto *underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto *ttc = dynamic_cast<TooltipClient *> (underMouse))
                if (!(underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                    newTip = ttc->getTooltip();

        if (newTip != tip) {
            tip = newTip;
            repaint();
        }
    }

    String tip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
};

//==============================================================================
class GraphDocumentComponent::TitleBarComponent : public Component,
                                                  private Button::Listener {
public:
    TitleBarComponent(GraphDocumentComponent &graphDocumentComponent)
            : owner(graphDocumentComponent) {
        static const unsigned char burgerMenuPathData[]
                = {110, 109, 0, 0, 128, 64, 0, 0, 32, 65, 108, 0, 0, 224, 65, 0, 0, 32, 65, 98, 254, 212, 232, 65, 0, 0,
                   32, 65, 0, 0, 240, 65, 252,
                   169, 17, 65, 0, 0, 240, 65, 0, 0, 0, 65, 98, 0, 0, 240, 65, 8, 172, 220, 64, 254, 212, 232, 65, 0, 0,
                   192, 64, 0, 0, 224, 65, 0, 0,
                   192, 64, 108, 0, 0, 128, 64, 0, 0, 192, 64, 98, 16, 88, 57, 64, 0, 0, 192, 64, 0, 0, 0, 64, 8, 172,
                   220, 64, 0, 0, 0, 64, 0, 0, 0, 65,
                   98, 0, 0, 0, 64, 252, 169, 17, 65, 16, 88, 57, 64, 0, 0, 32, 65, 0, 0, 128, 64, 0, 0, 32, 65, 99,
                   109, 0, 0, 224, 65, 0, 0, 96, 65, 108,
                   0, 0, 128, 64, 0, 0, 96, 65, 98, 16, 88, 57, 64, 0, 0, 96, 65, 0, 0, 0, 64, 4, 86, 110, 65, 0, 0, 0,
                   64, 0, 0, 128, 65, 98, 0, 0, 0, 64,
                   254, 212, 136, 65, 16, 88, 57, 64, 0, 0, 144, 65, 0, 0, 128, 64, 0, 0, 144, 65, 108, 0, 0, 224, 65,
                   0, 0, 144, 65, 98, 254, 212, 232,
                   65, 0, 0, 144, 65, 0, 0, 240, 65, 254, 212, 136, 65, 0, 0, 240, 65, 0, 0, 128, 65, 98, 0, 0, 240, 65,
                   4, 86, 110, 65, 254, 212, 232,
                   65, 0, 0, 96, 65, 0, 0, 224, 65, 0, 0, 96, 65, 99, 109, 0, 0, 224, 65, 0, 0, 176, 65, 108, 0, 0, 128,
                   64, 0, 0, 176, 65, 98, 16, 88, 57,
                   64, 0, 0, 176, 65, 0, 0, 0, 64, 2, 43, 183, 65, 0, 0, 0, 64, 0, 0, 192, 65, 98, 0, 0, 0, 64, 254,
                   212, 200, 65, 16, 88, 57, 64, 0, 0, 208,
                   65, 0, 0, 128, 64, 0, 0, 208, 65, 108, 0, 0, 224, 65, 0, 0, 208, 65, 98, 254, 212, 232, 65, 0, 0,
                   208, 65, 0, 0, 240, 65, 254, 212,
                   200, 65, 0, 0, 240, 65, 0, 0, 192, 65, 98, 0, 0, 240, 65, 2, 43, 183, 65, 254, 212, 232, 65, 0, 0,
                   176, 65, 0, 0, 224, 65, 0, 0, 176,
                   65, 99, 101, 0, 0};

        static const unsigned char pluginListPathData[]
                = {110, 109, 193, 202, 222, 64, 80, 50, 21, 64, 108, 0, 0, 48, 65, 0, 0, 0, 0, 108, 160, 154, 112, 65,
                   80, 50, 21, 64, 108, 0, 0, 48, 65, 80,
                   50, 149, 64, 108, 193, 202, 222, 64, 80, 50, 21, 64, 99, 109, 0, 0, 192, 64, 251, 220, 127, 64, 108,
                   160, 154, 32, 65, 165, 135, 202,
                   64, 108, 160, 154, 32, 65, 250, 220, 47, 65, 108, 0, 0, 192, 64, 102, 144, 10, 65, 108, 0, 0, 192,
                   64, 251, 220, 127, 64, 99, 109, 0, 0,
                   128, 65, 251, 220, 127, 64, 108, 0, 0, 128, 65, 103, 144, 10, 65, 108, 96, 101, 63, 65, 251, 220, 47,
                   65, 108, 96, 101, 63, 65, 166, 135,
                   202, 64, 108, 0, 0, 128, 65, 251, 220, 127, 64, 99, 109, 96, 101, 79, 65, 148, 76, 69, 65, 108, 0, 0,
                   136, 65, 0, 0, 32, 65, 108, 80,
                   77, 168, 65, 148, 76, 69, 65, 108, 0, 0, 136, 65, 40, 153, 106, 65, 108, 96, 101, 79, 65, 148, 76,
                   69, 65, 99, 109, 0, 0, 64, 65, 63, 247,
                   95, 65, 108, 80, 77, 128, 65, 233, 161, 130, 65, 108, 80, 77, 128, 65, 125, 238, 167, 65, 108, 0, 0,
                   64, 65, 51, 72, 149, 65, 108, 0, 0, 64,
                   65, 63, 247, 95, 65, 99, 109, 0, 0, 176, 65, 63, 247, 95, 65, 108, 0, 0, 176, 65, 51, 72, 149, 65,
                   108, 176, 178, 143, 65, 125, 238, 167, 65,
                   108, 176, 178, 143, 65, 233, 161, 130, 65, 108, 0, 0, 176, 65, 63, 247, 95, 65, 99, 109, 12, 86, 118,
                   63, 148, 76, 69, 65, 108, 0, 0, 160,
                   64, 0, 0, 32, 65, 108, 159, 154, 16, 65, 148, 76, 69, 65, 108, 0, 0, 160, 64, 40, 153, 106, 65, 108,
                   12, 86, 118, 63, 148, 76, 69, 65, 99,
                   109, 0, 0, 0, 0, 63, 247, 95, 65, 108, 62, 53, 129, 64, 233, 161, 130, 65, 108, 62, 53, 129, 64, 125,
                   238, 167, 65, 108, 0, 0, 0, 0, 51,
                   72, 149, 65, 108, 0, 0, 0, 0, 63, 247, 95, 65, 99, 109, 0, 0, 32, 65, 63, 247, 95, 65, 108, 0, 0, 32,
                   65, 51, 72, 149, 65, 108, 193, 202, 190,
                   64, 125, 238, 167, 65, 108, 193, 202, 190, 64, 233, 161, 130, 65, 108, 0, 0, 32, 65, 63, 247, 95, 65,
                   99, 101, 0, 0};

        {
            Path p;
            p.loadPathFromData(burgerMenuPathData, sizeof(burgerMenuPathData));
            burgerButton.setShape(p, true, true, false);
        }

        {
            Path p;
            p.loadPathFromData(pluginListPathData, sizeof(pluginListPathData));
            pluginButton.setShape(p, true, true, false);
        }

        burgerButton.addListener(this);
        addAndMakeVisible(burgerButton);

        pluginButton.addListener(this);
        addAndMakeVisible(pluginButton);

        titleLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        setOpaque(true);
    }

private:
    void paint(Graphics &g) override {
        auto titleBarBackgroundColour = getLookAndFeel().findColour(ResizableWindow::backgroundColourId).darker();

        g.setColour(titleBarBackgroundColour);
        g.fillRect(getLocalBounds());
    }

    void resized() override {
        auto r = getLocalBounds();

        burgerButton.setBounds(r.removeFromLeft(40).withSizeKeepingCentre(20, 20));

        pluginButton.setBounds(r.removeFromRight(40).withSizeKeepingCentre(20, 20));

        titleLabel.setFont(Font(static_cast<float> (getHeight()) * 0.5f, Font::plain));
        titleLabel.setBounds(r);
    }

    void buttonClicked(Button *b) override {
        owner.showSidePanel(b == &burgerButton);
    }

    GraphDocumentComponent &owner;

    Label titleLabel{"titleLabel", "Plugin Host"};
    ShapeButton burgerButton{"burgerButton", Colours::lightgrey, Colours::lightgrey, Colours::white};
    ShapeButton pluginButton{"pluginButton", Colours::lightgrey, Colours::lightgrey, Colours::white};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TitleBarComponent)
};

//==============================================================================
struct GraphDocumentComponent::PluginListBoxModel : public ListBoxModel,
                                                    public ChangeListener,
                                                    public MouseListener {
    PluginListBoxModel(ListBox &lb, KnownPluginList &kpl)
            : owner(lb),
              knownPlugins(kpl) {
        knownPlugins.addChangeListener(this);
        owner.addMouseListener(this, true);
    }

    int getNumRows() override {
        return knownPlugins.getNumTypes();
    }

    void paintListBoxItem(int rowNumber, Graphics &g,
                          int width, int height, bool rowIsSelected) override {
        g.fillAll(rowIsSelected ? Colour(0xff42A2C8)
                                : Colour(0xff263238));

        g.setColour(rowIsSelected ? Colours::black : Colours::white);

        if (rowNumber < knownPlugins.getNumTypes())
            g.drawFittedText(knownPlugins.getType(rowNumber)->name,
                             {0, 0, width, height - 2},
                             Justification::centred,
                             1);

        g.setColour(Colours::black.withAlpha(0.4f));
        g.drawRect(0, height - 1, width, 1);
    }

    var getDragSourceDescription(const SparseSet<int> &selectedRows) override {
        if (!isOverSelectedRow)
            return var();

        return String("PLUGIN: " + String(selectedRows[0]));
    }

    void changeListenerCallback(ChangeBroadcaster *) override {
        owner.updateContent();
    }

    void mouseDown(const MouseEvent &e) override {
        isOverSelectedRow = owner.getRowPosition(owner.getSelectedRow(), true)
                .contains(e.getEventRelativeTo(&owner).getMouseDownPosition());
    }

    ListBox &owner;
    KnownPluginList &knownPlugins;

    bool isOverSelectedRow = false;
};

//==============================================================================
GraphDocumentComponent::GraphDocumentComponent(ProcessorGraph &graph,
                                               AudioPluginFormatManager &fm,
                                               AudioDeviceManager &dm,
                                               KnownPluginList &kpl)
        : graph(graph),
          deviceManager(dm),
          pluginList(kpl) {
    init();

    deviceManager.addChangeListener(graphPanel.get());
}

void GraphDocumentComponent::init() {
    graphPanel.reset(new GraphEditorPanel(graph));
    addAndMakeVisible(graphPanel.get());

    statusBar.reset(new TooltipBar());
    addAndMakeVisible(statusBar.get());

    graphPanel->updateComponents();
}

GraphDocumentComponent::~GraphDocumentComponent() {
    releaseGraph();
}

void GraphDocumentComponent::resized() {
    auto r = getLocalBounds();
    const int titleBarHeight = 40;
    const int keysHeight = 60;
    const int statusHeight = 20;

    statusBar->setBounds(r.removeFromBottom(statusHeight));
    graphPanel->setBounds(r);

    checkAvailableWidth();
}

void GraphDocumentComponent::createNewPlugin(const PluginDescription &desc, Point<int> pos) {
    graphPanel->createNewPlugin(desc, pos);
}

void GraphDocumentComponent::releaseGraph() {
    if (graphPanel != nullptr) {
        deviceManager.removeChangeListener(graphPanel.get());
        graphPanel = nullptr;
    }

    statusBar = nullptr;
}

bool GraphDocumentComponent::isInterestedInDragSource(const SourceDetails &details) {
    return ((dynamic_cast<ListBox *> (details.sourceComponent.get()) != nullptr)
            && details.description.toString().startsWith("PLUGIN"));
}

void GraphDocumentComponent::itemDropped(const SourceDetails &details) {
    // don't allow items to be dropped behind the sidebar
    if (pluginListSidePanel.getBounds().contains(details.localPosition))
        return;

    auto pluginTypeIndex = details.description.toString()
            .fromFirstOccurrenceOf("PLUGIN: ", false, false)
            .getIntValue();

    // must be a valid index!
    jassert (isPositiveAndBelow(pluginTypeIndex, pluginList.getNumTypes()));

    createNewPlugin(*pluginList.getType(pluginTypeIndex), details.localPosition);
}

void GraphDocumentComponent::showSidePanel(bool showSettingsPanel) {
    if (showSettingsPanel)
        mobileSettingsSidePanel.showOrHide(true);
    else
        pluginListSidePanel.showOrHide(true);

    checkAvailableWidth();

    lastOpenedSidePanel = showSettingsPanel ? &mobileSettingsSidePanel
                                            : &pluginListSidePanel;
}

void GraphDocumentComponent::hideLastSidePanel() {
    if (lastOpenedSidePanel != nullptr)
        lastOpenedSidePanel->showOrHide(false);

    if (mobileSettingsSidePanel.isPanelShowing()) lastOpenedSidePanel = &mobileSettingsSidePanel;
    else if (pluginListSidePanel.isPanelShowing()) lastOpenedSidePanel = &pluginListSidePanel;
    else lastOpenedSidePanel = nullptr;
}

void GraphDocumentComponent::checkAvailableWidth() {
    if (mobileSettingsSidePanel.isPanelShowing() && pluginListSidePanel.isPanelShowing()) {
        if (getWidth() - (mobileSettingsSidePanel.getWidth() + pluginListSidePanel.getWidth()) < 150)
            hideLastSidePanel();
    }
}

bool GraphDocumentComponent::closeAnyOpenPluginWindows() {
    //return graphPanel->graph.closeAnyOpenPluginWindows();
    return true;
}
