#include "GraphEditorChannel.h"

#include "StatefulAudioProcessorContainer.h"

GraphEditorChannel::GraphEditorChannel(ValueTree state, ConnectorDragListener &connectorDragListener, bool showChannelText)
        : state(std::move(state)), connectorDragListener(connectorDragListener),
          channelLabel(isMidi(), showChannelText) {
    valueTreePropertyChanged(this->state, IDs::abbreviatedName);
    this->state.addListener(this);

    addAndMakeVisible(channelLabel);
    channelLabel.addMouseListener(this, true);

    setSize(16, 16);
}

GraphEditorChannel::~GraphEditorChannel() {
    channelLabel.removeMouseListener(this);
}

AudioProcessorGraph::NodeAndChannel GraphEditorChannel::getNodeAndChannel() const {
    return {StatefulAudioProcessorContainer::getNodeIdForState(getProcessor()), getChannelIndex()};
}

juce::Point<float> GraphEditorChannel::getConnectPosition() const {
    const auto &centre = getBounds().getCentre().toFloat();
    bool isLeftToRight = TracksState::isProcessorLeftToRightFlowing(getProcessor());

    if (isInput()) {
        if (isLeftToRight)
            return centre.withX(getX());
        else
            return centre.withY(getY());
    } else {
        if (isLeftToRight)
            return centre.withX(getRight());
        else
            return centre.withY(getBottom());
    }
}

void GraphEditorChannel::resized() {
    auto channelLabelBounds = getLocalBounds();

// TODO:
//  make master track label into a folder-tab style & remove special casing around master TrackInputProcessor view
//  remove TRACK_VERTICAL_MARGIN stuff inside track input/output views - now that pins don't extend beyond track processor
//    bounds, only TRACKS/TRACK should be aware of this margin
//  make IO processors smaller, left-align input processors and right-align output processors
//  fix master trackoutput bottom channel label positioning
    if (channelLabel.getText().length() <= 1 || isMidi()) {
        bool isLeftToRight = TracksState::isProcessorLeftToRightFlowing(getProcessor());
        if (isInput()) {
            if (isLeftToRight)
                channelLabelBounds.setWidth(getHeight());
            else
                channelLabelBounds.setHeight(getWidth());
        } else {
            if (isLeftToRight)
                channelLabelBounds = channelLabelBounds.removeFromRight(getHeight());
            else
                channelLabelBounds = channelLabelBounds.removeFromBottom(getWidth());
        }
        channelLabel.setBounds(channelLabelBounds);
    } else {
        channelLabel.rotate = true;
        channelLabel.setBounds(channelLabelBounds);
    }
}

void GraphEditorChannel::updateColour() {
    bool mouseOver = isMouseOver(true);
    auto colour = isMidi()
                  ? findColour(!allowDefaultConnections() ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                  : findColour(!allowDefaultConnections() ? customAudioConnectionColourId : defaultAudioConnectionColourId);

    auto pathColour = colour.withRotatedHue(busIdx / 5.0f);
    if (mouseOver)
        pathColour = pathColour.brighter(0.15f);
}

void GraphEditorChannel::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != state) return;

    if (i == IDs::abbreviatedName) {
        const String &name = v[IDs::abbreviatedName];
        channelLabel.setText(name);
        setName(name);
        setTooltip(name);
    }
}
