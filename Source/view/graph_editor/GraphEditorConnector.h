#pragma once

#include "JuceHeader.h"
#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ConnectorDragListener.h"
#include "GraphEditorProcessorContainer.h"

struct GraphEditorConnector : public Component, public SettableTooltipClient {
    explicit GraphEditorConnector(const ValueTree &state, ConnectorDragListener &connectorDragListener,
                                  GraphEditorProcessorContainer &graphEditorProcessorContainer,
                                  AudioProcessorGraph::NodeAndChannel source = {},
                                  AudioProcessorGraph::NodeAndChannel destination = {})
            : state(state), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        setAlwaysOnTop(true);
        if (this->state.isValid()) {
            const auto &sourceState = this->state.getChildWithName(IDs::SOURCE);
            connection.source = {ProcessorGraph::getNodeIdForState(sourceState), sourceState[IDs::channel]};
            const auto &destinationState = this->state.getChildWithName(IDs::DESTINATION);
            connection.destination = {ProcessorGraph::getNodeIdForState(destinationState), destinationState[IDs::channel]};
        } else {
            connection.source = source;
            connection.destination = destination;
            if (connection.source.nodeID.uid != 0)
                dragAnchor = source;
            else if (connection.destination.nodeID.uid != 0)
                dragAnchor = destination;
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

    // TODO might be able to make this private
    void setDestination(AudioProcessorGraph::NodeAndChannel newDestination) {
        if (connection.destination != newDestination) {
            connection.destination = newDestination;
            update();
        }
    }

    void dragTo(const juce::Point<float> &position) {
        if (dragAnchor == connection.source) {
            connection.destination = {};
            lastDestinationPos = position - getPosition().toFloat();
        } else {
            connection.source = {};
            lastSourcePos = position - getPosition().toFloat();
        }
        resizeToFit();
    }

    void dragTo(AudioProcessorGraph::NodeAndChannel nodeAndChannel) {
        if (dragAnchor == nodeAndChannel) return;

        if (dragAnchor == connection.source)
            setDestination(nodeAndChannel);
        else
            setSource(nodeAndChannel);
    }

    void update() {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        if (lastSourcePos != p1 || lastDestinationPos != p2)
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
        p1 = lastSourcePos;
        p2 = lastDestinationPos;

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

    bool isDragging() {
        return dragAnchor.nodeID.uid != 0;
    }

    void paint(Graphics &g) override {
        auto pathColour = connection.source.isMIDI() || connection.destination.isMIDI()
                            ? findColour(isCustom() ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                            : findColour(isCustom() ? customAudioConnectionColourId : defaultAudioConnectionColourId);

        bool mouseOver = isMouseOver(false);
        g.setColour(mouseOver ? pathColour.brighter(0.1f) : pathColour);

        if (bothInView && linePath.getLength() > 200) {
            ColourGradient colourGradient = ColourGradient(pathColour, lastSourcePos, pathColour, lastDestinationPos, false);
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
        dragAnchor.nodeID.uid = 0;
    }

    void mouseDrag(const MouseEvent &e) override {
        if (isDragging()) {
            connectorDragListener.dragConnector(e);
        } else if (e.mouseWasDraggedSinceMouseDown()) {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            static const AudioProcessorGraph::NodeAndChannel dummy{ProcessorGraph::NodeID(), 0};
            dragAnchor = isNearerSource ? connection.destination : connection.source;

            connectorDragListener.beginConnectorDrag(isNearerSource ? dummy : connection.source,
                                                     isNearerSource ? connection.destination : dummy,
                                                     e);
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (isDragging()) {
            dragAnchor.nodeID.uid = 0;
            connectorDragListener.endDraggingConnector(e);
        }
    }

    void resized() override {
        const static auto arrowW = 5.0f;
        const static auto arrowL = 4.0f;

        juce::Point<float> sourcePos, destinationPos;
        getPoints(sourcePos, destinationPos);

        lastSourcePos = sourcePos;
        lastDestinationPos = destinationPos;

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

    juce::Point<float> lastSourcePos, lastDestinationPos;
    Path linePath, hitPath, hoverPath;
    bool bothInView = false;
    AudioProcessorGraph::NodeAndChannel dragAnchor{ProcessorGraph::NodeID(), 0};

    AudioProcessorGraph::Connection connection{
            {ProcessorGraph::NodeID(0), 0},
            {ProcessorGraph::NodeID(0), 0}
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorConnector)
};
