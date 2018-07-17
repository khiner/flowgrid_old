#pragma once

#include <ProcessorGraph.h>

class GraphEditorPanel : public Component,
                         public ChangeListener {
public:
    GraphEditorPanel(ProcessorGraph &graph, Project& project);

    ~GraphEditorPanel();

    void paint(Graphics &) override;

    void resized() override;

    void mouseDown(const MouseEvent &) override;

    void mouseUp(const MouseEvent &) override;

    void mouseDrag(const MouseEvent &) override;

    void changeListenerCallback(ChangeBroadcaster *) override;

    //==============================================================================
    void updateComponents();

    //==============================================================================
    void showPopupMenu(Point<int> position);

    //==============================================================================
    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source,
                            AudioProcessorGraph::NodeAndChannel dest,
                            const MouseEvent &);

    void dragConnector(const MouseEvent &);

    void endDraggingConnector(const MouseEvent &);

    //==============================================================================
    ProcessorGraph &graph;
    Project &project;

    static const int NUM_ROWS = Project::NUM_VISIBLE_PROCESSOR_SLOTS;
    static const int NUM_COLUMNS = Project::NUM_VISIBLE_TRACKS;

private:
    struct FilterComponent;
    struct ConnectorComponent;
    struct PinComponent;

    OwnedArray<FilterComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;

    FilterComponent *getComponentForFilter(AudioProcessorGraph::NodeID) const;

    ConnectorComponent *getComponentForConnection(const AudioProcessorGraph::Connection &) const;

    PinComponent *findPinAt(Point<float>) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};


//==============================================================================
/**
    A panel that embeds a GraphEditorPanel with a midi keyboard at the bottom.
*/
class GraphDocumentComponent : public Component {
public:
    GraphDocumentComponent(ProcessorGraph &graph,
                           Project &project,
                           AudioPluginFormatManager &formatManager,
                           AudioDeviceManager &deviceManager,
                           KnownPluginList &pluginList);

    ~GraphDocumentComponent() override;

    void resized() override;

    void releaseGraph();

    std::unique_ptr<GraphEditorPanel> graphPanel;
private:
    ProcessorGraph &graph;
    Project &project;
    AudioDeviceManager &deviceManager;
    KnownPluginList &pluginList;

    struct TooltipBar;
    std::unique_ptr<TooltipBar> statusBar;

    void init();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphDocumentComponent)
};
