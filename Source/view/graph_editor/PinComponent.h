#pragma once

#include "JuceHeader.h"
#include "ConnectorDragListener.h"

struct PinComponent : public Component, public SettableTooltipClient {
    PinComponent(ConnectorDragListener &connectorDragListener, AudioProcessor* processor, AudioProcessorGraph::NodeID nodeId, int channelIndex, bool isIn)
            : connectorDragListener(connectorDragListener), pin({nodeId, channelIndex}), isInput(isIn) {
        String tip;

        if (pin.isMIDI()) {
            tip = isInput ? "MIDI Input" : "MIDI Output";
        } else {
            auto channel = processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, pin.channelIndex, busIdx);

            if (auto *bus = processor->getBus(isInput, busIdx))
                tip = bus->getName() + ": " + AudioChannelSet::getAbbreviatedChannelTypeName(
                        bus->getCurrentLayout().getTypeOfChannel(channel));
            else
                tip = (isInput ? "Main Input: " : "Main Output: ") + String(pin.channelIndex + 1);
        }

        setTooltip(tip);

        setSize(16, 16);
    }

    void paint(Graphics &g) override {
        auto w = (float) getWidth();
        auto h = (float) getHeight();

        Path p;
        p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
        p.addRectangle(w * 0.4f, isInput ? (0.5f * h) : 0.0f, w * 0.2f, h * 0.5f);

        auto colour = (pin.isMIDI() ? Colours::red : Colours::green);

        g.setColour(colour.withRotatedHue(busIdx / 5.0f));
        g.fillPath(p);
    }

    void mouseDown(const MouseEvent &e) override {
        AudioProcessorGraph::NodeAndChannel dummy{0, 0};
        connectorDragListener.beginConnectorDrag(isInput ? dummy : pin, isInput ? pin : dummy, e);
    }

    void mouseDrag(const MouseEvent &e) override {
        connectorDragListener.dragConnector(e);
    }

    void mouseUp(const MouseEvent &e) override {
        connectorDragListener.endDraggingConnector(e);
    }

    ConnectorDragListener &connectorDragListener;
    AudioProcessorGraph::NodeAndChannel pin;
    const bool isInput;
    int busIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
};