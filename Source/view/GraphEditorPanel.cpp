#include "../JuceLibraryCode/JuceHeader.h"
#include "GraphEditorPanel.h"
#include <memory>

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
                            //showWindow(PluginWindow::Type::normal);
                            break;
                        case 11:
                            //showWindow(PluginWindow::Type::programs);
                            break;
                        case 12:
                            //showWindow(PluginWindow::Type::generic);
                            break;
                        default:
                            break;
                    }
                }));
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
    explicit ConnectorComponent(GraphEditorPanel &p) : panel(p), graph(p.graph) {
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

    void dragStart(const Point<float> &pos) {
        lastInputPos = pos;
        resizeToFit();
    }

    void dragEnd(const Point<float> &pos) {
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
        Colour pathColour = connection.source.isMIDI() || connection.destination.isMIDI()
                ? Colours::red : Colours::green;
        g.setColour(pathColour.withAlpha(0.75f));
        g.fillPath(linePath);
    }

    bool hitTest(int x, int y) override {
        auto pos = juce::Point<int>(x, y).toFloat();

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
                                     .rotated(MathConstants<float>::halfPi - atan2(p2.x - p1.x, p2.y - p1.y))
                                     .translated((p1 + p2) * 0.5f));

        linePath.addPath(arrow);
        linePath.setUsingNonZeroWinding(true);
    }

    void getDistancesFromEnds(const Point<float> &p, double &distanceFromStart, double &distanceFromEnd) const {
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
GraphEditorPanel::GraphEditorPanel(ProcessorGraph &g, Project &project)
        : graph(g), project(project) {
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
    auto cellWidth = int(float(getWidth()) / float(NUM_COLUMNS));
    auto cellHeight = int(float(getHeight()) / float(NUM_ROWS));
    auto horizontalLineLength = cellWidth * project.getNumTracks();
    auto verticalLineLength = cellHeight * Project::NUM_AVAILABLE_PROCESSOR_SLOTS;
    for (int row = 0; row < Project::NUM_AVAILABLE_PROCESSOR_SLOTS + 1; row++) {
        for (int column = 0; column < project.getNumTracks() + 1; column++) {
            g.drawHorizontalLine(row * cellHeight, 0, horizontalLineLength);
            g.drawVerticalLine(column * cellWidth, 0, verticalLineLength);
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
    graph.addPlugin(desc, position.toDouble() / juce::Point<double>((double) getWidth(), (double) getHeight()));
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
        if (getComponentForConnection(c) == nullptr) {
            auto *comp = connectors.add(new ConnectorComponent(*this));
            addAndMakeVisible(comp);

            comp->setInput(c.source);
            comp->setOutput(c.destination);
        }
    }
}

void GraphEditorPanel::showPopupMenu(Point<int> mousePos) {
    menu = std::make_unique<PopupMenu>();
    project.addPluginsToMenu(*menu);
    menu->showMenuAsync({}, ModalCallbackFunction::create([this, mousePos](int r) {
        if (auto *description = project.getChosenType(r)) {
            createNewPlugin(*description, mousePos);
        }
    }));
}

void GraphEditorPanel::beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source,
                                          AudioProcessorGraph::NodeAndChannel dest,
                                          const MouseEvent &e) {
    auto *c = dynamic_cast<ConnectorComponent *> (e.originalComponent);
    connectors.removeObject(c, false);
    draggingConnector.reset(c);

    if (draggingConnector == nullptr)
        draggingConnector = std::make_unique<ConnectorComponent>(*this);

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
GraphDocumentComponent::GraphDocumentComponent(ProcessorGraph &graph,
                                               Project &project,
                                               AudioPluginFormatManager &fm,
                                               AudioDeviceManager &dm,
                                               KnownPluginList &kpl)
        : graph(graph),
          project(project),
          deviceManager(dm),
          pluginList(kpl) {
    init();

    deviceManager.addChangeListener(graphPanel.get());
}

void GraphDocumentComponent::init() {
    graphPanel = std::make_unique<GraphEditorPanel>(graph, project);
    addAndMakeVisible(graphPanel.get());

    statusBar = std::make_unique<TooltipBar>();
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
    auto pluginTypeIndex = details.description.toString()
            .fromFirstOccurrenceOf("PLUGIN: ", false, false)
            .getIntValue();

    // must be a valid index!
    jassert (isPositiveAndBelow(pluginTypeIndex, pluginList.getNumTypes()));

    createNewPlugin(*pluginList.getType(pluginTypeIndex), details.localPosition);
}

bool GraphDocumentComponent::closeAnyOpenPluginWindows() {
    //return graphPanel->graph.closeAnyOpenPluginWindows();
    return true;
}
