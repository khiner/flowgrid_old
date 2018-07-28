#pragma once

#include "JuceHeader.h"

struct ConnectorDragListener {
    virtual ~ConnectorDragListener() {};
    virtual void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel s, AudioProcessorGraph::NodeAndChannel d, const MouseEvent &) = 0;
    virtual void dragConnector(const MouseEvent &) = 0;
    virtual void endDraggingConnector(const MouseEvent &) = 0;
    virtual void update() = 0;
};
