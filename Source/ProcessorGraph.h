#pragma once

#include "JuceHeader.h"

class ProcessorGraph : public FileBasedDocument,
                       public AudioProcessorListener,
                       private ChangeListener {
public:
    //==============================================================================
    ProcessorGraph(AudioPluginFormatManager &);

    ~ProcessorGraph();

    //==============================================================================
    typedef AudioProcessorGraph::NodeID NodeID;

    void addPlugin(const PluginDescription &, Point<double>);

    AudioProcessorGraph::Node::Ptr getNodeForName(const String &name) const;

    void setNodePosition(NodeID, Point<double>);

    Point<double> getNodePosition(NodeID) const;

    //==============================================================================
    void clear();

    void closeCurrentlyOpenWindowsFor(AudioProcessorGraph::NodeID);
    //bool closeAnyOpenPluginWindows();

    //==============================================================================
    void audioProcessorParameterChanged(AudioProcessor *, int, float) override {}

    void audioProcessorChanged(AudioProcessor *) override { changed(); }

    //==============================================================================
    XmlElement *createXml() const;

    void restoreFromXml(const XmlElement &xml);

    static const char *getFilenameSuffix() { return ".processorgraph"; }

    static const char *getFilenameWildcard() { return "*.processorgraph"; }

    //==============================================================================
    void newDocument();

    String getDocumentTitle() override;

    Result loadDocument(const File &file) override;

    Result saveDocument(const File &file) override;

    File getLastDocumentOpened() override;

    void setLastDocumentOpened(const File &file) override;

    static File getDefaultGraphDocumentOnMobile();

    //==============================================================================
    AudioProcessorGraph graph;

private:
    //==============================================================================
    AudioPluginFormatManager &formatManager;

    NodeID lastUID = 0;

    NodeID getNextUID() noexcept;

    void createNodeFromXml(const XmlElement &xml);

    void addFilterCallback(AudioPluginInstance *, const String &error, Point<double>);

    void changeListenerCallback(ChangeBroadcaster *) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)
};
