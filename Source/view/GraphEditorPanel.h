#pragma once

#include <AudioGraphBuilder.h>

class GraphEditorPanel : public Component,
                         public ChangeListener {
public:
    GraphEditorPanel(AudioGraphBuilder &graph);

    ~GraphEditorPanel();

    void createNewPlugin(const PluginDescription &, Point<int> position);

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
    AudioGraphBuilder &graph;

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
class GraphDocumentComponent : public Component,
                               public DragAndDropTarget,
                               public DragAndDropContainer {
public:
    GraphDocumentComponent(AudioGraphBuilder &graph,
                           AudioPluginFormatManager &formatManager,
                           AudioDeviceManager &deviceManager,
                           KnownPluginList &pluginList);

    ~GraphDocumentComponent();

    //==============================================================================
    void createNewPlugin(const PluginDescription &, Point<int> position);

    bool closeAnyOpenPluginWindows();

    void resized() override;

    void releaseGraph();

    //==============================================================================
    bool isInterestedInDragSource(const SourceDetails &) override;

    void itemDropped(const SourceDetails &) override;


    std::unique_ptr<GraphEditorPanel> graphPanel;

    //==============================================================================
    void showSidePanel(bool isSettingsPanel);

    void hideLastSidePanel();

    BurgerMenuComponent burgerMenu;

private:
    AudioGraphBuilder &graph;
    AudioDeviceManager &deviceManager;
    KnownPluginList &pluginList;

    struct TooltipBar;
    std::unique_ptr<TooltipBar> statusBar;

    class TitleBarComponent;

    std::unique_ptr<TitleBarComponent> titleBarComponent;

    //==============================================================================
    struct PluginListBoxModel;
    std::unique_ptr<PluginListBoxModel> pluginListBoxModel;

    ListBox pluginListBox;

    SidePanel mobileSettingsSidePanel{"Settings", 300, true};
    SidePanel pluginListSidePanel{"Plugins", 250, false};
    SidePanel *lastOpenedSidePanel = nullptr;

    //==============================================================================
    void init();

    void checkAvailableWidth();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphDocumentComponent)
};
