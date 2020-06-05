#pragma once

#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "Utilities.h"
#include "state/Identifiers.h"
#include "view/CustomColourIds.h"

struct GraphEditorChannel : public Component, public SettableTooltipClient, private Utilities::ValueTreePropertyChangeListener {
    GraphEditorChannel(ValueTree state, ConnectorDragListener &connectorDragListener)
            : state(std::move(state)), connectorDragListener(connectorDragListener) {
        valueTreePropertyChanged(this->state, IDs::abbreviatedName);
        this->state.addListener(this);

        channelLabel.setColour(findColour(TextEditor::textColourId));
        channelLabel.setFontHeight(smallFontHeight);
        channelLabel.setJustification(Justification::centred);
        addAndMakeVisible(channelLabel);

        setSize(16, 16);
    }

    const ValueTree &getState() { return state; }

    ValueTree getTrack() const { return TracksState::getTrackForProcessor(getProcessor()); }

    ValueTree getProcessor() const { return state.getParent().getParent(); }

    int getChannelIndex() const {
        return state[IDs::channelIndex];
    }

    inline bool isMasterTrack() const { return TracksState::isMasterTrack(getTrack()); }

    bool isInput() const { return state.getParent().hasType(IDs::INPUT_CHANNELS); }

    bool isMidi() const { return getChannelIndex() == AudioProcessorGraph::midiChannelIndex; }

    bool allowDefaultConnections() const { return getProcessor()[IDs::allowDefaultConnections]; }

    AudioProcessorGraph::NodeAndChannel getNodeAndChannel() const {
        return {ProcessorGraph::getNodeIdForState(state.getParent().getParent()), getChannelIndex()};
    }

    juce::Point<float> getConnectPosition() const {
        const auto &centre = getBounds().getCentre().toFloat();

        if (isInput()) {
            if (!isMasterTrack() || TracksState::isTrackInputProcessor(getProcessor()))
                return centre.withY(getY());
            else
                return centre.withX(getX());
        } else {
            if (!isMasterTrack() || TracksState::isTrackOutputProcessor(getProcessor()))
                return centre.withY(getBottom());
            else
                return centre.withX(getRight());
        }
    }

    void resized() override {
        auto channelLabelBounds = getLocalBounds();

        // TODO:
        //  show midi icon in Channel instead of label
        //  make master track label into a folder-tab style & remove special casing around master TrackInputProcessor view
        //  remove TRACK_VERTICAL_MARGIN stuff inside track input/output views - now that pins don't extend beyond track processor
        //    bounds, only TRACKS/TRACK should be aware of this margin
        //  make IO processors smaller, left-align input processors and right-align output processors
        //  fix master trackoutput bottom channel label positioning
        if (channelLabel.getText().length() <= 1 || isMidi()) {
            if (isInput()) {
                if (isMasterTrack())
                    channelLabelBounds.setWidth(getHeight());
                else
                    channelLabelBounds.setHeight(getWidth());
            } else {
                if (isMasterTrack())
                    channelLabelBounds = channelLabelBounds.removeFromRight(getHeight());
                else
                    channelLabelBounds = channelLabelBounds.removeFromBottom(getWidth());
            }
            channelLabel.setBoundingBox(channelLabelBounds.toFloat());
        } else {
            channelLabel.setBoundingBox(rotateRectIfNarrow(channelLabelBounds.toFloat()));
        }
    }

    void updateColour() {
        bool mouseOver = isMouseOver(true);
        auto colour = isMidi()
                      ? findColour(!allowDefaultConnections() ? customMidiConnectionColourId : defaultMidiConnectionColourId)
                      : findColour(!allowDefaultConnections() ? customAudioConnectionColourId : defaultAudioConnectionColourId);

        auto pathColour = colour.withRotatedHue(busIdx / 5.0f);
        if (mouseOver)
            pathColour = pathColour.brighter(0.15f);
    }

    void mouseEnter(const MouseEvent & e) override {
        updateColour();
    }

    void mouseExit(const MouseEvent & e) override {
        updateColour();
    }

    void mouseDown(const MouseEvent &e) override {
        AudioProcessorGraph::NodeAndChannel dummy{ProcessorGraph::NodeID(0), 0};
        AudioProcessorGraph::NodeAndChannel nodeAndChannel = getNodeAndChannel();
        connectorDragListener.beginConnectorDrag(isInput() ? dummy : nodeAndChannel, isInput() ? nodeAndChannel : dummy, e);
    }

    void mouseDrag(const MouseEvent &e) override {
        connectorDragListener.dragConnector(e);
    }

    void mouseUp(const MouseEvent &e) override {
        connectorDragListener.endDraggingConnector(e);
    }

    // Rotate text to draw vertically if the box is taller than it is wide.
    static Parallelogram<float> rotateRectIfNarrow(const Rectangle<float> &rectangle) {
        if (rectangle.getWidth() > rectangle.getHeight())
            return rectangle;
        else
            return Parallelogram<float>(rectangle.getBottomLeft(), rectangle.getTopLeft(), rectangle.getBottomRight());
    }

    DrawableText channelLabel;
private:
    static constexpr float smallFontHeight = 15.0f;

    ValueTree state;
    int busIdx = 0;
    ConnectorDragListener &connectorDragListener;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (i == IDs::abbreviatedName) {
            const String &name = v[IDs::abbreviatedName];
            channelLabel.setText(name);
            setName(name);
            setTooltip(name);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorChannel)
};