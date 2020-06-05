#pragma once

#include "JuceHeader.h"

struct ConnectorDragListener {
    virtual ~ConnectorDragListener() {};

    virtual void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source,
                                    AudioProcessorGraph::NodeAndChannel destination,
                                    const MouseEvent &) = 0;

    virtual void dragConnector(const MouseEvent &) = 0;

    virtual void endDraggingConnector(const MouseEvent &) = 0;

    virtual void update() = 0;
};
