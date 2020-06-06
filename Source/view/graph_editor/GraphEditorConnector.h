#pragma once

#include "JuceHeader.h"
#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ConnectorDragListener.h"
#include "GraphEditorProcessorContainer.h"

struct GraphEditorConnector : public Component, public SettableTooltipClient {
    explicit GraphEditorConnector(const ValueTree &state, ConnectorDragListener &connectorDragListener,
                                  GraphEditorProcessorContainer &graphEditorProcessorContainer)
            : state(state), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        setAlwaysOnTop(true);
        if (this->state.isValid()) {
            const ValueTree &source = state.getChildWithName(IDs::SOURCE);
            const ValueTree &destination = state.getChildWithName(IDs::DESTINATION);
            connection = {{ProcessorGraph::getNodeIdForState(source),      source[IDs::channel]},
                          {ProcessorGraph::getNodeIdForState(destination), destination[IDs::channel]}};
        }
    }

    AudioProcessorGraph::Connection getConnection() {
        return connection;
    }

    const ValueTree &getState() const {
        return state;
    }

    bool isCustom() const {
        return !state.isValid() || state[IDs::isCustomConnection];
    }

    void setSource(AudioProcessorGraph::NodeAndChannel newSource) {
        if (connection.source != newSource) {
            connection.source = newSource;
            update();
        }
    }

    void setDestination(AudioProcessorGraph::NodeAndChannel newDestination) {
        if (connection.destination != newDestination) {
            connection.destination = newDestination;
            update();
        }
    }

    void dragStart(const juce::Point<float> &pos) {
        connection.source = {};
        lastInputPos = pos - getPosition().toFloat();
        resizeToFit();
    }

    void dragEnd(const juce::Point<float> &pos) {
        connection.destination = {};
        lastOutputPos = pos - getPosition().toFloat();
        resizeToFit();
    }

    void update() {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        if (lastInputPos != p1 || lastOutputPos != p2)
            resizeToFit();
    }

    void resizeToFit() {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        auto newBounds = Rectangle<float>(p1 + getPosition().toFloat(), p2 + getPosition().toFloat()).expanded(20.0f).getSmallestIntegerContainer();

        if (newBounds != getBounds())
            setBounds(newBounds);
        else
            resized();

        repaint();
    }

    void getPoints(juce::Point<float> &p1, juce::Point<float> &p2) const {
        p1 = lastInputPos;
        p2 = lastOutputPos;

        const Component &rootComponent = dynamic_cast<Component &>(connectorDragListener);

        auto *sourceComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.source.nodeID);
        if (sourceComponent != nullptr) {
            p1 = rootComponent.getLocalPoint(sourceComponent,
                                             sourceComponent->getChannelConnectPosition(connection.source.channelIndex, false)) - getPosition().toFloat();
        }

        auto *destinationComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.destination.nodeID);
        if (destinationComponent != nullptr) {
            p2 = rootComponent.getLocalPoint(destinationComponent,
                                             destinationComponent->getChannelConnectPosition(connection.destination.channelIndex, true)) - getPosition().toFloat();
        }
    }

    void paint(Graphics &g) override {
        auto pathColour = connection.source.isMIDI() || connection.destination.isMIDI()
                            ? findColour(isCustom() ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                            : findColour(isCustom() ? customAudioConnectionColourId : defaultAudioConnectionColourId);

        bool mouseOver = isMouseOver(false);
        g.setColour(mouseOver ? pathColour.brighter(0.1f) : pathColour);

        if (bothInView && linePath.getLength() > 200) {
            ColourGradient colourGradient = ColourGradient(pathColour, lastInputPos, pathColour, lastOutputPos, false);
            colourGradient.addColour(0.25f, pathColour.withAlpha(0.2f));
            colourGradient.addColour(0.75f, pathColour.withAlpha(0.2f));
            g.setGradientFill(colourGradient);
        }

        g.fillPath(mouseOver ? hoverPath : linePath);
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

    void mouseEnter(const MouseEvent & e) override {
        repaint();
    }

    void mouseExit(const MouseEvent & e) override {
        repaint();
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

            AudioProcessorGraph::NodeAndChannel dummy{ProcessorGraph::NodeID(0), 0};

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
        const static auto arrowW = 5.0f;
        const static auto arrowL = 4.0f;

        juce::Point<float> sourcePos, destinationPos;
        getPoints(sourcePos, destinationPos);

        lastInputPos = sourcePos;
        lastOutputPos = destinationPos;

        const auto toDestinationVec = destinationPos - sourcePos;
        const auto toSourceVec = sourcePos - destinationPos;

        auto *sourceComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.source.nodeID);
        auto *destinationComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.destination.nodeID);
        bool isSourceInView = sourceComponent == nullptr || sourceComponent->isInView();
        bool isDestinationInView = destinationComponent == nullptr || destinationComponent->isInView();

        linePath.clear();
        bothInView = isSourceInView && isDestinationInView;
        if (bothInView) {
            linePath.startNewSubPath(sourcePos);
            auto outgoingDirection = sourceComponent != nullptr ? sourceComponent->getConnectorDirectionVector(false) : juce::Point<int>(0, 1);
            auto incomingDirection = destinationComponent != nullptr ? destinationComponent->getConnectorDirectionVector(true) : juce::Point<int>(0, -1);
            static const float controlHeight = 30.0f; // ensure the "cable" comes straight out a bit before curving back
            linePath.cubicTo(sourcePos + outgoingDirection.toFloat() * controlHeight, destinationPos + incomingDirection.toFloat() * controlHeight, destinationPos);
        } else if (isSourceInView && !isDestinationInView) {
            const auto &toDestinationUnitVec = toDestinationVec / toDestinationVec.getDistanceFromOrigin();
            Line line(sourcePos, sourcePos + 24.0f * toDestinationUnitVec);
            linePath.addArrow(line, 0.0f, arrowW, arrowL);
        } else if (isDestinationInView && !isSourceInView) {
            const auto &toSourceUnitVec = toSourceVec / toSourceVec.getDistanceFromOrigin();
            Line line(destinationPos + 24.0f * toSourceUnitVec, destinationPos + 5.0f * toSourceUnitVec);
            linePath.addArrow(line, 0.0f, arrowW, arrowL);
        }

        // TODO don't think I need all these
        PathStrokeType(8.0f).createStrokedPath(hitPath, linePath);
        PathStrokeType(5.0f).createStrokedPath(hoverPath, linePath);
        PathStrokeType(3.0f).createStrokedPath(linePath, linePath);

        linePath.setUsingNonZeroWinding(true);
    }

    void getDistancesFromEnds(const juce::Point<float> &p, double &distanceFromStart, double &distanceFromEnd) const {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        distanceFromStart = p1.getDistanceFrom(p);
        distanceFromEnd = p2.getDistanceFrom(p);
    }

private:
    ValueTree state;

    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;

    juce::Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath, hoverPath;
    bool dragging = false, bothInView = false;

    AudioProcessorGraph::Connection connection{
            {ProcessorGraph::NodeID(0), 0},
            {ProcessorGraph::NodeID(0), 0}
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorConnector)
};
