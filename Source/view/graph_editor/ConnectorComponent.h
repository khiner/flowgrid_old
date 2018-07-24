#pragma once

#include "JuceHeader.h"
#include "ProcessorGraph.h"
#include "GraphEditorProcessor.h"

struct ConnectorComponent : public Component, public SettableTooltipClient {
    explicit ConnectorComponent(ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : connectorDragListener(connectorDragListener), graph(graph) {
        setAlwaysOnTop(true);
    }

    void setInput(AudioProcessorGraph::NodeAndChannel newSource, GraphEditorProcessor *newSourceComponent) {
        if (connection.source != newSource) {
            connection.source = newSource;
            sourceComponent = newSourceComponent;
            update();
        }
    }

    void setOutput(AudioProcessorGraph::NodeAndChannel newDestination, GraphEditorProcessor *newDestinationComponent) {
        if (connection.destination != newDestination) {
            connection.destination = newDestination;
            destinationComponent = newDestinationComponent;
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

        if (sourceComponent != nullptr) {
            p1 = sourceComponent->getPinPos(connection.source.channelIndex, false);
            if (auto* parent = sourceComponent->getParentComponent())
                if (auto* grandParent = parent->getParentComponent())
                    p1.x += grandParent->getX();
        }

        if (destinationComponent != nullptr) {
            p2 = destinationComponent->getPinPos(connection.destination.channelIndex, true);
            if (auto* parent = destinationComponent->getParentComponent())
                if (auto* grandParent = parent->getParentComponent())
                    p2.x += grandParent->getX();
        }
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
            connectorDragListener.dragConnector(e);
        } else if (e.mouseWasDraggedSinceMouseDown()) {
            dragging = true;

            graph.removeConnection(connection);

            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            AudioProcessorGraph::NodeAndChannel dummy{0, 0};

            connectorDragListener.beginConnectorDrag(isNearerSource ? dummy : connection.source,
                                                     isNearerSource ? connection.destination : dummy,
                                                     e);
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (dragging)
            connectorDragListener.endDraggingConnector(e);
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

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    AudioProcessorGraph::Connection connection{{0, 0}, {0, 0}};
    GraphEditorProcessor *sourceComponent{}, *destinationComponent{};

    Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};
