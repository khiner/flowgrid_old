#pragma once

#include "JuceHeader.h"
#include "ConnectorDragListener.h"
#include "Utilities.h"
#include "Identifiers.h"
#include "ProcessorGraph.h"

struct GraphEditorPin : public Component, public SettableTooltipClient, private Utilities::ValueTreePropertyChangeListener {
    GraphEditorPin(const ValueTree& state, ConnectorDragListener &connectorDragListener)
            : state(state), connectorDragListener(connectorDragListener) {
        setSize(16, 16);
        valueTreePropertyChanged(this->state, IDs::name);
        this->state.addListener(this);
    }

    ~GraphEditorPin() override {
        state.removeListener(this);
    }

    bool isInput() {
        return state.getParent().hasType(IDs::INPUT_CHANNELS);
    }

    int getChannel() {
        return getName().contains("MIDI") ? AudioProcessorGraph::midiChannelIndex : state.getParent().indexOf(state);
    }

    bool isMidi() {
        return getChannel() == AudioProcessorGraph::midiChannelIndex;
    }

    bool allowDefaultConnections() {
        return state.getParent().getParent()[IDs::allowDefaultConnections];
    }

    AudioProcessorGraph::NodeAndChannel getPin() {
        return {ProcessorGraph::getNodeIdForState(state.getParent().getParent()), getChannel()};
    }

    void paint(Graphics &g) override {
        auto w = (float) getWidth();
        auto h = (float) getHeight();

        Path p;
        p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
        p.addRectangle(w * 0.4f, isInput() ? (0.5f * h) : 0.0f, w * 0.2f, h * 0.5f);

        auto colour = (isMidi() ? Colours::red : (allowDefaultConnections() ? Colours::green : Colours::yellowgreen));

        g.setColour(colour.withRotatedHue(busIdx / 5.0f));
        g.fillPath(p);
    }

    void mouseDown(const MouseEvent &e) override {
        AudioProcessorGraph::NodeAndChannel dummy{0, 0};
        AudioProcessorGraph::NodeAndChannel pin = getPin();
        connectorDragListener.beginConnectorDrag(isInput() ? dummy : pin, isInput() ? pin : dummy, e);
    }

    void mouseDrag(const MouseEvent &e) override {
        connectorDragListener.dragConnector(e);
    }

    void mouseUp(const MouseEvent &e) override {
        connectorDragListener.endDraggingConnector(e);
    }

    ValueTree state;

    DrawableText channelLabel;
    
private:
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