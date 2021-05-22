#include "GraphEditorConnector.h"

GraphEditorConnector::GraphEditorConnector(ValueTree state, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer,
                                           AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination)
        : state(std::move(state)), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
    setAlwaysOnTop(true);
    if (this->state.isValid()) {
        connection.source = {fg::Connection::getSourceNodeId(this->state), fg::Connection::getSourceChannel(this->state)};
        connection.destination = {fg::Connection::getDestinationNodeId(this->state), fg::Connection::getDestinationChannel(this->state)};
    } else {
        connection.source = source;
        connection.destination = destination;
        if (connection.source.nodeID.uid != 0)
            dragAnchor = source;
        else if (connection.destination.nodeID.uid != 0)
            dragAnchor = destination;
    }
}


void GraphEditorConnector::dragTo(const juce::Point<float> &position) {
    if (dragAnchor == connection.source) {
        connection.destination = {};
        lastDestinationPos = position - getPosition().toFloat();
    } else {
        connection.source = {};
        lastSourcePos = position - getPosition().toFloat();
    }
    resizeToFit(lastSourcePos, lastDestinationPos);
}

void GraphEditorConnector::dragTo(AudioProcessorGraph::NodeAndChannel nodeAndChannel, bool isInput) {
    if (dragAnchor == nodeAndChannel) return;

    if (dragAnchor == connection.source && isInput)
        setDestination(nodeAndChannel);
    else if (dragAnchor == connection.destination && !isInput)
        setSource(nodeAndChannel);
}

void GraphEditorConnector::update() {
    juce::Point<float> p1, p2;
    getPoints(p1, p2);

    if (lastSourcePos != p1 || lastDestinationPos != p2)
        resizeToFit(p1, p2);
}

void GraphEditorConnector::getPoints(juce::Point<float> &p1, juce::Point<float> &p2) const {
    p1 = lastSourcePos;
    p2 = lastDestinationPos;

    const Component &rootComponent = dynamic_cast<Component &>(connectorDragListener);
    auto *sourceComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.source.nodeID);
    if (sourceComponent != nullptr) {
        p1 = rootComponent.getLocalPoint(sourceComponent, sourceComponent->getChannelConnectPosition(connection.source.channelIndex, false)) - getPosition().toFloat();
    }

    auto *destinationComponent = graphEditorProcessorContainer.getProcessorForNodeId(connection.destination.nodeID);
    if (destinationComponent != nullptr) {
        p2 = rootComponent.getLocalPoint(destinationComponent, destinationComponent->getChannelConnectPosition(connection.destination.channelIndex, true)) - getPosition().toFloat();
    }
}

void GraphEditorConnector::paint(Graphics &g) {
    auto pathColour = connection.source.isMIDI() || connection.destination.isMIDI()
                      ? findColour(Connection::isCustom(state) ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                      : findColour(Connection::isCustom(state) ? customAudioConnectionColourId : defaultAudioConnectionColourId);

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

void GraphEditorConnector::resized() {
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
        static const float controlHeight = 30.0f; // ensure the "cable" comes straight out a bit before curving back
        const auto &outgoingDirection = sourceComponent != nullptr ? sourceComponent->getConnectorDirectionVector(false) : juce::Point<float>(0, 0);
        const auto &incomingDirection = destinationComponent != nullptr ? destinationComponent->getConnectorDirectionVector(true) : juce::Point<float>(0, 0);
        const auto &sourceControlPoint = sourcePos + outgoingDirection * controlHeight;
        const auto &destinationControlPoint = destinationPos + incomingDirection * controlHeight;
        linePath.startNewSubPath(sourcePos);
        // To bevel the line ends with processor edges exactly, need to go directly for 1px here before curving.
        linePath.lineTo(sourcePos + outgoingDirection);
        linePath.cubicTo(sourceControlPoint, destinationControlPoint, destinationPos + incomingDirection);
        linePath.lineTo(destinationPos);
    } else if (isSourceInView && !isDestinationInView) {
        const auto &toDestinationUnitVec = toDestinationVec / toDestinationVec.getDistanceFromOrigin();
        Line line(sourcePos, sourcePos + 24.0f * toDestinationUnitVec);
        linePath.addArrow(line, 0.0f, arrowW, arrowL);
    } else if (isDestinationInView && !isSourceInView) {
        const auto &toSourceUnitVec = toSourceVec / toSourceVec.getDistanceFromOrigin();
        Line line(destinationPos + 24.0f * toSourceUnitVec, destinationPos + 5.0f * toSourceUnitVec);
        linePath.addArrow(line, 0.0f, arrowW, arrowL);
    }

    PathStrokeType(5.0f).createStrokedPath(hoverPath, linePath);
    PathStrokeType(3.0f).createStrokedPath(linePath, linePath);

    linePath.setUsingNonZeroWinding(true);
}