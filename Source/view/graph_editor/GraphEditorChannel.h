#pragma once

#include "state/Identifiers.h"
#include "view/CustomColourIds.h"
#include "ConnectorDragListener.h"
#include "Utilities.h"

struct GraphEditorChannel : public Component, public SettableTooltipClient, private Utilities::ValueTreePropertyChangeListener {
    GraphEditorChannel(ValueTree state, ConnectorDragListener &connectorDragListener, bool showChannelText = false)
            : state(std::move(state)), connectorDragListener(connectorDragListener),
              channelLabel(isMidi(), showChannelText) {
        valueTreePropertyChanged(this->state, IDs::abbreviatedName);
        this->state.addListener(this);

        addAndMakeVisible(channelLabel);
        channelLabel.addMouseListener(this, true);

        setSize(16, 16);
    }

    ~GraphEditorChannel() {
        channelLabel.removeMouseListener(this);
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
        return {ProcessorGraph::getNodeIdForState(getProcessor()), getChannelIndex()};
    }

    juce::Point<float> getConnectPosition() const {
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

    void resized() override {
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
        static const AudioProcessorGraph::NodeAndChannel dummy{ProcessorGraph::NodeID(), 0};
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
private:
    static constexpr float smallFontHeight = 15.0f;

    ValueTree state;
    int busIdx = 0;
    ConnectorDragListener &connectorDragListener;

    // Can be either text or an icon
    struct ChannelLabel : public Component {
        ChannelLabel(bool isMidi, bool showChannelText) :
                isMidi(isMidi), showChannelText(showChannelText) {
            if (showChannelText) {
                text.setColour(findColour(TextEditor::textColourId));
                text.setFontHeight(smallFontHeight);
                text.setJustification(Justification::centred);
                addAndMakeVisible(text);
            }
        }

        void paint(Graphics &g) override {
            if (showChannelText) return;

            const auto &r = getLocalBounds().toFloat();
            g.setColour(findColour(isMidi ? CustomColourIds::defaultMidiConnectionColourId : CustomColourIds::defaultAudioConnectionColourId));
            g.fillEllipse(r.withSizeKeepingCentre(4, 4));
        }

        void resized() override {
            if (showChannelText) {
                const auto &r = getLocalBounds().toFloat().reduced(1);
                const auto &bounds = rotate ? rotateRectIfNarrow(r) : r;
                text.setBoundingBox(bounds);
            }
        }

        const String &getText() {
            return text.getText();
        }

        void setText(const String &text) {
            this->text.setText(text);
        }

        bool isMidi, showChannelText;

        DrawableText text;
        DrawableImage image;
        bool rotate;
    };

    ChannelLabel channelLabel;

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