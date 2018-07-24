#pragma once

#include "JuceHeader.h"
#include "GraphEditorProcessor.h"
#include "ConnectorDragListener.h"

struct GraphEditorConnector : public Component, public SettableTooltipClient, public Utilities::ValueTreePropertyChangeListener {
    explicit GraphEditorConnector(const ValueTree& state, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer)
            : state(state), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        setAlwaysOnTop(true);
        if (this->state.isValid()) {
            this->state.addListener(this);
            const ValueTree &source = state.getChildWithName(IDs::SOURCE);
            const ValueTree &destination = state.getChildWithName(IDs::DESTINATION);
            connection = {{AudioProcessorGraph::NodeID(int(source.getProperty(IDs::NODE_ID))), source.getProperty(IDs::CHANNEL)},
                          {AudioProcessorGraph::NodeID(int(destination.getProperty(IDs::NODE_ID))), destination.getProperty(IDs::CHANNEL)}};
        }
    }

    ~GraphEditorConnector() override {
        if (state.isValid()) {
            state.removeListener(this);
        }
    }

    AudioProcessorGraph::Connection getConnection() {
        return connection;
    }

    void setInput(AudioProcessorGraph::NodeAndChannel newSource) {
        if (connection.source != newSource) {
            connection.source = newSource;
            update();
        }
    }

    void setOutput(AudioProcessorGraph::NodeAndChannel newDestination) {
        if (connection.destination != newDestination) {
            connection.destination = newDestination;
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

        auto *sourceComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.source.nodeID);
        if (sourceComponent != nullptr) {
            p1 = sourceComponent->getPinPos(connection.source.channelIndex, false);
            p1.x += sourceComponent->getParentComponent()->getParentComponent()->getX();
        }

        auto *destinationComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.destination.nodeID);
        if (destinationComponent != nullptr) {
            p2 = destinationComponent->getPinPos(connection.destination.channelIndex, true);
            p2.x += destinationComponent->getParentComponent()->getParentComponent()->getX();
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

    ValueTree state;

    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;

    Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    bool dragging = false;

private:
    AudioProcessorGraph::Connection connection{{0, 0}, {0, 0}};

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v == state.getChildWithName(IDs::SOURCE)) {
            if (i == IDs::NODE_ID) {
                connection.source.nodeID = AudioProcessorGraph::NodeID(int(v.getProperty(i)));
            } else if (i == IDs::CHANNEL) {
                connection.source.channelIndex = int(v.getProperty(i));
            }
        } else if (v == state.getChildWithName(IDs::DESTINATION)) {
            if (i == IDs::NODE_ID) {
                connection.destination.nodeID = AudioProcessorGraph::NodeID(int(v.getProperty(i)));
            } else if (i == IDs::CHANNEL) {
                connection.destination.channelIndex = int(v.getProperty(i));
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorConnector)
};
