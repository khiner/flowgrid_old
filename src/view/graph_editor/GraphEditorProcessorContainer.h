#pragma once

#include "view/graph_editor/processor/BaseGraphEditorProcessor.h"

// TODO delete
class GraphEditorProcessorContainer {
public:
    virtual BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID) const = 0;

    virtual ~GraphEditorProcessorContainer() = default;
};
