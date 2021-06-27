#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ConnectorDragListener.h"
#include "GraphEditorProcessorContainer.h"

struct GraphEditorConnector : public Component, public SettableTooltipClient {
    explicit GraphEditorConnector(fg::Connection *, ConnectorDragListener &, GraphEditorProcessorContainer &);

    ~GraphEditorConnector() override {
        setTooltip({});
    }

    AudioProcessorGraph::Connection getAudioConnection() { return audioConnection; }

    fg::Connection *getConnection() const { return connection; }

    void update();

    void dragTo(const juce::Point<float> &position);
    void dragTo(AudioProcessorGraph::NodeAndChannel nodeAndChannel, bool isInput);
    bool isDragging() const { return dragAnchor.nodeID.uid != 0; }

    void getPoints(juce::Point<float> &p1, juce::Point<float> &p2) const;

    void resized() override;
    void paint(Graphics &g) override;
    bool hitTest(int x, int y) override { return hoverPath.contains({float(x), float(y)}); }
    void mouseEnter(const MouseEvent &e) override { repaint(); }
    void mouseExit(const MouseEvent &e) override { repaint(); }
    void mouseDown(const MouseEvent &) override { dragAnchor.nodeID.uid = 0; }
    void mouseDrag(const MouseEvent &e) override {
        if (isDragging()) {
            connectorDragListener.dragConnector(e);
        } else if (e.mouseWasDraggedSinceMouseDown()) {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            static const AudioProcessorGraph::NodeAndChannel dummy{AudioProcessorGraph::NodeID(), 0};
            dragAnchor = isNearerSource ? audioConnection.destination : audioConnection.source;

            connectorDragListener.beginConnectorDrag(isNearerSource ? dummy : audioConnection.source,
                                                     isNearerSource ? audioConnection.destination : dummy,
                                                     e);
        }
    }
    void mouseUp(const MouseEvent &e) override {
        if (isDragging()) {
            dragAnchor.nodeID.uid = 0;
            connectorDragListener.endDraggingConnector(e);
        }
    }

private:
    fg::Connection *connection;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;

    juce::Point<float> lastSourcePos, lastDestinationPos;
    Path linePath, hoverPath;
    bool bothInView = false;
    AudioProcessorGraph::NodeAndChannel dragAnchor{AudioProcessorGraph::NodeID(), 0};

    AudioProcessorGraph::Connection audioConnection{
            {AudioProcessorGraph::NodeID(0), 0},
            {AudioProcessorGraph::NodeID(0), 0}
    };

    void setSource(AudioProcessorGraph::NodeAndChannel newSource) {
        if (audioConnection.source != newSource) {
            audioConnection.source = newSource;
            update();
        }
    }

    void setDestination(AudioProcessorGraph::NodeAndChannel newDestination) {
        if (audioConnection.destination != newDestination) {
            audioConnection.destination = newDestination;
            update();
        }
    }

    void resizeToFit(juce::Point<float> p1, juce::Point<float> p2) {
        auto newBounds = Rectangle<float>(p1 + getPosition().toFloat(), p2 + getPosition().toFloat()).expanded(20.0f).getSmallestIntegerContainer();
        if (newBounds != getBounds())
            setBounds(newBounds);
    }

    void getDistancesFromEnds(const juce::Point<float> &p, double &distanceFromStart, double &distanceFromEnd) const {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        distanceFromStart = p1.getDistanceFrom(p);
        distanceFromEnd = p2.getDistanceFrom(p);
    }
};
