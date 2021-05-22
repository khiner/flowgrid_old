#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "model/Tracks.h"
#include "view/CustomColourIds.h"
#include "ConnectorDragListener.h"

struct GraphEditorChannel : public Component, public SettableTooltipClient, private ValueTree::Listener {
    GraphEditorChannel(ValueTree state, ConnectorDragListener &connectorDragListener, bool showChannelText = false);

    ~GraphEditorChannel() override;

    const ValueTree &getState() { return state; }

    ValueTree getTrack() const { return Tracks::getTrackForProcessor(getProcessor()); }

    ValueTree getProcessor() const { return state.getParent().getParent(); }

    int getChannelIndex() const { return state[ChannelIDs::channelIndex]; }

    bool isInput() const { return state.getParent().hasType(InputChannelsIDs::INPUT_CHANNELS); }

    bool isMidi() const { return getChannelIndex() == AudioProcessorGraph::midiChannelIndex; }

    AudioProcessorGraph::NodeAndChannel getNodeAndChannel() const;

    juce::Point<float> getConnectPosition() const;

    void resized() override;

    void updateColour();

    void mouseEnter(const MouseEvent &e) override { updateColour(); }

    void mouseExit(const MouseEvent &e) override { updateColour(); }

    void mouseDown(const MouseEvent &e) override {
        static const AudioProcessorGraph::NodeAndChannel dummy{juce::AudioProcessorGraph::NodeID(), 0};
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
        if (rectangle.getWidth() > rectangle.getHeight()) return rectangle;
        return Parallelogram<float>(rectangle.getBottomLeft(), rectangle.getTopLeft(), rectangle.getBottomRight());
    }

private:
    static constexpr float smallFontHeight = 15.0f;

    ValueTree state;
    int busIdx = 0;
    ConnectorDragListener &connectorDragListener;

    // Can be either text or an icon
    struct ChannelLabel : public Component {
        ChannelLabel(bool isMidi, bool showChannelText) : isMidi(isMidi), showChannelText(showChannelText), rotate(false) {
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

        const String &getText() const {
            return text.getText();
        }

        void setText(const String &text) {
            this->text.setText(text);
        }

        DrawableText text;
        DrawableImage image;
        bool isMidi, showChannelText, rotate;
    };

    ChannelLabel channelLabel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
