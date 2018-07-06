#include "ProcessorGraph.h"

ProcessorGraph::ProcessorGraph(AudioPluginFormatManager &fm)
        : FileBasedDocument(getFilenameSuffix(),
                            getFilenameWildcard(),
                            "Load a filter graph",
                            "Save a filter graph"),
          formatManager(fm) {
    newDocument();

    graph.addListener(this);
    graph.addChangeListener(this);

    setChangedFlag(false);
}

ProcessorGraph::~ProcessorGraph() {
    graph.removeListener(this);
    graph.removeChangeListener(this);
    graph.clear();
}

ProcessorGraph::NodeID ProcessorGraph::getNextUID() noexcept {
    return ++lastUID;
}

//==============================================================================
void ProcessorGraph::changeListenerCallback(ChangeBroadcaster *) {
    changed();
}

AudioProcessorGraph::Node::Ptr ProcessorGraph::getNodeForName(const String &name) const {
    for (auto *node : graph.getNodes())
        if (auto p = node->getProcessor())
            if (p->getName().equalsIgnoreCase(name))
                return node;

    return nullptr;
}

void ProcessorGraph::addPlugin(const PluginDescription &desc, Point<double> p) {
    struct AsyncCallback : public AudioPluginFormat::InstantiationCompletionCallback {
        AsyncCallback(ProcessorGraph &g, Point<double> pos) : owner(g), position(pos) {}

        void completionCallback(AudioPluginInstance *instance, const String &error) override {
            owner.addFilterCallback(instance, error, position);
        }

        ProcessorGraph &owner;
        Point<double> position;
    };

    formatManager.createPluginInstanceAsync(desc,
                                            graph.getSampleRate(),
                                            graph.getBlockSize(),
                                            new AsyncCallback(*this, p));
}

void ProcessorGraph::addFilterCallback(AudioPluginInstance *instance, const String &error, Point<double> pos) {
    if (instance == nullptr) {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                         TRANS("Couldn't create filter"),
                                         error);
    } else {
        instance->enableAllBuses();

        if (auto node = graph.addNode(instance)) {
            node->properties.set("x", pos.x);
            node->properties.set("y", pos.y);
            changed();
        }
    }
}

void ProcessorGraph::setNodePosition(NodeID nodeID, Point<double> pos) {
    if (auto *n = graph.getNodeForId(nodeID)) {
        n->properties.set("x", jlimit(0.0, 1.0, pos.x));
        n->properties.set("y", jlimit(0.0, 1.0, pos.y));
    }
}

Point<double> ProcessorGraph::getNodePosition(NodeID nodeID) const {
    if (auto *n = graph.getNodeForId(nodeID))
        return {static_cast<double> (n->properties["x"]),
                static_cast<double> (n->properties["y"])};

    return {};
}

//==============================================================================
void ProcessorGraph::clear() {
    //closeAnyOpenPluginWindows();
    graph.clear();
    changed();
}

//==============================================================================
String ProcessorGraph::getDocumentTitle() {
    if (!getFile().exists())
        return "Unnamed";

    return getFile().getFileNameWithoutExtension();
}

void ProcessorGraph::newDocument() {
    clear();
    setFile({});
//
//    InternalPluginFormat internalFormat;
//
//    addPlugin(internalFormat.audioInDesc, {0.5, 0.1});
//    addPlugin(internalFormat.midiInDesc, {0.25, 0.1});
//    addPlugin(internalFormat.audioOutDesc, {0.5, 0.9});
//
//    setChangedFlag(false);
}

Result ProcessorGraph::loadDocument(const File &file) {
    XmlDocument doc(file);
    std::unique_ptr<XmlElement> xml(doc.getDocumentElement());

    if (xml == nullptr || !xml->hasTagName("ProcessorGraph"))
        return Result::fail("Not a valid processor graph file");

    restoreFromXml(*xml);
    return Result::ok();
}

Result ProcessorGraph::saveDocument(const File &file) {
    std::unique_ptr<XmlElement> xml(createXml());

    if (!xml->writeToFile(file, {}))
        return Result::fail("Couldn't write to the file");

    return Result::ok();
}

File ProcessorGraph::getLastDocumentOpened() {
    RecentlyOpenedFilesList recentFiles;
    //recentFiles.restoreFromString(getAppProperties().getUserSettings()->getValue("recentProcessorGraphFiles"));

    return recentFiles.getFile(0);
}

void ProcessorGraph::setLastDocumentOpened(const File &file) {
    //RecentlyOpenedFilesList recentFiles;
    //recentFiles.restoreFromString(getAppProperties().getUserSettings()->getValue("recentProcessorGraphFiles"));

    //recentFiles.addFile(file);

    //getAppProperties().getUserSettings()->setValue("recentProcessorGraphFiles", recentFiles.toString());
}

//==============================================================================
static void readBusLayoutFromXml(AudioProcessor::BusesLayout &busesLayout, AudioProcessor *plugin,
                                 const XmlElement &xml, const bool isInput) {
    auto &targetBuses = (isInput ? busesLayout.inputBuses
                                 : busesLayout.outputBuses);
    int maxNumBuses = 0;

    if (auto *buses = xml.getChildByName(isInput ? "INPUTS" : "OUTPUTS")) {
        forEachXmlChildElementWithTagName (*buses, e, "BUS") {
            const int busIdx = e->getIntAttribute("index");
            maxNumBuses = jmax(maxNumBuses, busIdx + 1);

            // the number of buses on busesLayout may not be in sync with the plugin after adding buses
            // because adding an input bus could also add an output bus
            for (int actualIdx = plugin->getBusCount(isInput) - 1; actualIdx < busIdx; ++actualIdx)
                if (!plugin->addBus(isInput))
                    return;

            for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                targetBuses.add(plugin->getChannelLayoutOfBus(isInput, busIdx));

            auto layout = e->getStringAttribute("layout");

            if (layout.isNotEmpty())
                targetBuses.getReference(busIdx) = AudioChannelSet::fromAbbreviatedString(layout);
        }
    }

    // if the plugin has more buses than specified in the xml, then try to remove them!
    while (maxNumBuses < targetBuses.size()) {
        if (!plugin->removeBus(isInput))
            return;

        targetBuses.removeLast();
    }
}

//==============================================================================
static XmlElement *createBusLayoutXml(const AudioProcessor::BusesLayout &layout, const bool isInput) {
    auto &buses = isInput ? layout.inputBuses
                          : layout.outputBuses;

    auto *xml = new XmlElement(isInput ? "INPUTS" : "OUTPUTS");

    for (int busIdx = 0; busIdx < buses.size(); ++busIdx) {
        auto &set = buses.getReference(busIdx);

        auto *bus = xml->createNewChildElement("BUS");
        bus->setAttribute("index", busIdx);
        bus->setAttribute("layout", set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString());
    }

    return xml;
}

static XmlElement *createNodeXml(AudioProcessorGraph::Node *const node) noexcept {
    if (auto *plugin = dynamic_cast<AudioPluginInstance *> (node->getProcessor())) {
        auto e = new XmlElement("FILTER");
        e->setAttribute("uid", (int) node->nodeID);
        e->setAttribute("x", node->properties["x"].toString());
        e->setAttribute("y", node->properties["y"].toString());

        {
            PluginDescription pd;
            plugin->fillInPluginDescription(pd);
            e->addChildElement(pd.createXml());
        }

        {
            MemoryBlock m;
            node->getProcessor()->getStateInformation(m);
            e->createNewChildElement("STATE")->addTextElement(m.toBase64Encoding());
        }

        auto layout = plugin->getBusesLayout();

        auto layouts = e->createNewChildElement("LAYOUT");
        layouts->addChildElement(createBusLayoutXml(layout, true));
        layouts->addChildElement(createBusLayoutXml(layout, false));

        return e;
    }

    jassertfalse;
    return nullptr;
}

void ProcessorGraph::createNodeFromXml(const XmlElement &xml) {
    PluginDescription pd;

    forEachXmlChildElement (xml, e) {
        if (pd.loadFromXml(*e))
            break;
    }

    String errorMessage;

    if (auto *instance = formatManager.createPluginInstance(pd, graph.getSampleRate(),
                                                            graph.getBlockSize(), errorMessage)) {
        if (auto *layoutEntity = xml.getChildByName("LAYOUT")) {
            auto layout = instance->getBusesLayout();

            readBusLayoutFromXml(layout, instance, *layoutEntity, true);
            readBusLayoutFromXml(layout, instance, *layoutEntity, false);

            instance->setBusesLayout(layout);
        }

        if (auto node = graph.addNode(instance, (NodeID) xml.getIntAttribute("uid"))) {
            if (auto *state = xml.getChildByName("STATE")) {
                MemoryBlock m;
                m.fromBase64Encoding(state->getAllSubText());

                node->getProcessor()->setStateInformation(m.getData(), (int) m.getSize());
            }

            node->properties.set("x", xml.getDoubleAttribute("x"));
            node->properties.set("y", xml.getDoubleAttribute("y"));
        }
    }
}

XmlElement *ProcessorGraph::createXml() const {
    auto *xml = new XmlElement("ProcessorGraph");

    for (auto *node : graph.getNodes())
        xml->addChildElement(createNodeXml(node));

    for (auto &connection : graph.getConnections()) {
        auto e = xml->createNewChildElement("CONNECTION");

        e->setAttribute("srcFilter", (int) connection.source.nodeID);
        e->setAttribute("srcChannel", connection.source.channelIndex);
        e->setAttribute("dstFilter", (int) connection.destination.nodeID);
        e->setAttribute("dstChannel", connection.destination.channelIndex);
    }

    return xml;
}

void ProcessorGraph::restoreFromXml(const XmlElement &xml) {
    clear();

    forEachXmlChildElementWithTagName (xml, e, "FILTER") {
        createNodeFromXml(*e);
        changed();
    }

    forEachXmlChildElementWithTagName (xml, e, "CONNECTION") {
        graph.addConnection({{(NodeID) e->getIntAttribute("srcFilter"), e->getIntAttribute("srcChannel")},
                             {(NodeID) e->getIntAttribute("dstFilter"), e->getIntAttribute("dstChannel")}});
    }

    graph.removeIllegalConnections();
}
