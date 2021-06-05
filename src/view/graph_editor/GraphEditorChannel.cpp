#include "GraphEditorChannel.h"

GraphEditorChannel::GraphEditorChannel(ValueTree state, Track *track, ConnectorDragListener &connectorDragListener, bool showChannelText)
        : state(std::move(state)), track(track), connectorDragListener(connectorDragListener),
          channelLabel(isMidi(), showChannelText) {
    valueTreePropertyChanged(this->state, ChannelIDs::abbreviatedName);
    this->state.addListener(this);

    addAndMakeVisible(channelLabel);
    channelLabel.addMouseListener(this, true);

    setSize(16, 16);
}

GraphEditorChannel::~GraphEditorChannel() {
    channelLabel.removeMouseListener(this);
}

AudioProcessorGraph::NodeAndChannel GraphEditorChannel::getNodeAndChannel() const {
    return {getProcessor()->getNodeId(), fg::Channel::getChannelIndex(state)};
}

juce::Point<float> GraphEditorChannel::getConnectPosition() const {
    const auto &centre = getBounds().getCentre().toFloat();
    bool isLeftToRight = track != nullptr && track->isProcessorLeftToRightFlowing(getProcessor());

    if (isInput()) {
        return isLeftToRight ? centre.withX(getX()) : centre.withY(getY());
    }
    return isLeftToRight ? centre.withX(getRight()) : centre.withY(getBottom());
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
        bool isLeftToRight = track != nullptr && track->isProcessorLeftToRightFlowing(getProcessor());
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
    bool allowsDefaultConnections = getProcessor()->allowsDefaultConnections();
    auto colour = isMidi()
                  ? findColour(!allowsDefaultConnections ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                  : findColour(!allowsDefaultConnections ? customAudioConnectionColourId : defaultAudioConnectionColourId);

    auto pathColour = colour.withRotatedHue(busIdx / 5.0f);
    if (mouseOver)
        pathColour = pathColour.brighter(0.15f);
}

void GraphEditorChannel::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != state) return;

    if (i == ChannelIDs::abbreviatedName) {
        const auto &name = fg::Channel::getAbbreviatedName(v);
        channelLabel.setText(name);
        setName(name);
        setTooltip(name);
    }
}
