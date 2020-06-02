#pragma once

#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "Utilities.h"
#include "state/Identifiers.h"

struct GraphEditorPin : public Component, public SettableTooltipClient, private Utilities::ValueTreePropertyChangeListener {
    GraphEditorPin(ValueTree state, ConnectorDragListener &connectorDragListener)
            : state(std::move(state)), connectorDragListener(connectorDragListener) {
        setSize(16, 16);
        valueTreePropertyChanged(this->state, IDs::name);
        this->state.addListener(this);
    }

    const ValueTree &getState() { return state; }

    int getChannel() {
        return getName().contains("MIDI") ? AudioProcessorGraph::midiChannelIndex : state.getParent().indexOf(state);
    }

    bool isInput() { return state.getParent().hasType(IDs::INPUT_CHANNELS); }

    bool isMidi() { return getChannel() == AudioProcessorGraph::midiChannelIndex; }

    bool allowDefaultConnections() { return state.getParent().getParent()[IDs::allowDefaultConnections]; }

    AudioProcessorGraph::NodeAndChannel getPin() {
        return {ProcessorGraph::getNodeIdForState(state.getParent().getParent()), getChannel()};
    }

    void paint(Graphics &g) override {
        auto w = (float) getWidth();
        auto h = (float) getHeight();
        bool mouseOver = isMouseOver(false);
        auto colour = isMidi() ?
                      (allowDefaultConnections() ? Colours::red : Colours::orange) :
                      (allowDefaultConnections() ? Colours::green : Colours::greenyellow);
        auto pathColour = colour.withRotatedHue(busIdx / 5.0f);
        if (mouseOver)
            pathColour = pathColour.brighter(0.15f);
        g.setColour(pathColour);
        auto circleRatio = mouseOver ? 1.0f : 0.8f;
        g.fillEllipse(w * (0.5f - circleRatio / 2), h * (0.5f - circleRatio / 2), w * circleRatio, h * circleRatio);

        // Half circle
//        Path p;
//        p.addArc(w * (0.5f - circleRatio / 2), h * (0.5f - circleRatio / 2), w * circleRatio, h * circleRatio, juce::float_Pi * 0.5f, juce::float_Pi * 1.5f, true);
//        g.fillPath(p);
    }

    void mouseEnter(const MouseEvent & e) override {
        repaint();
    }

    void mouseExit(const MouseEvent & e) override {
        repaint();
    }

    void mouseDown(const MouseEvent &e) override {
        AudioProcessorGraph::NodeAndChannel dummy{ProcessorGraph::NodeID(0), 0};
        AudioProcessorGraph::NodeAndChannel pin = getPin();
        connectorDragListener.beginConnectorDrag(isInput() ? dummy : pin, isInput() ? pin : dummy, e);
    }

    void mouseDrag(const MouseEvent &e) override {
        connectorDragListener.dragConnector(e);
    }

    void mouseUp(const MouseEvent &e) override {
        connectorDragListener.endDraggingConnector(e);
    }

    DrawableText channelLabel;
private:
    ValueTree state;

    int busIdx = 0;
    ConnectorDragListener &connectorDragListener;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (i == IDs::name) {
            const String &name = v[IDs::name];
            channelLabel.setText(name);
            setName(name);
            setTooltip(name);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPin)
};