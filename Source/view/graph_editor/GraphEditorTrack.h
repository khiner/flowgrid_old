#pragma once

#include <Utilities.h>
#include <Identifiers.h>
#include <view/ColourChangeButton.h>
#include "JuceHeader.h"
#include "GraphEditorProcessors.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTrack(Project& project, ValueTree v, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : state(std::move(v)), connectorDragListener(connectorDragListener), graph(graph) {
        nameLabel.setText(getTrackName(), dontSendNotification);
        nameLabel.setColour(Label::backgroundColourId, getColour());
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(*(processors = std::make_unique<GraphEditorProcessors>(project, state, connectorDragListener, graph)));
        state.addListener(this);
    }

    String getTrackName() const {
        return state[IDs::name].toString();
    }

    Colour getColour() const {
        return Colour::fromString(state[IDs::colour].toString());
    }

    void resized() override {
        auto r = getLocalBounds();
        nameLabel.setBounds(r.removeFromTop(int(getHeight() / 20.0)));
        processors->setBounds(r);
    }

    void paint(Graphics &g) override {
//        g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::defaultText, Colours::black));
//        g.setFont(jlimit(8.0f, 15.0f, getHeight() * 0.9f));
//        g.drawText(state[IDs::name].toString(), r.removeFromLeft(clipX).reduced(4, 0), Justification::centredLeft, true);
    }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (auto *currentlyMovingProcessor = processors->getCurrentlyMovingProcessor()) {
            if (currentlyMovingProcessor->getNodeId() == nodeId) {
                return currentlyMovingProcessor;
            }
        }
        return processors->getProcessorForNodeId(nodeId);
    }

    PinComponent *findPinAt(const MouseEvent &e) const {
        return processors->findPinAt(e);
    }

    void updateProcessors() {
        processors->update();
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        processors->setCurrentlyMovingProcessor(currentlyMovingProcessor);
    }

    ValueTree state;
private:
    Label nameLabel;
    std::unique_ptr<GraphEditorProcessors> processors;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
        if (v != state)
            return;
        if (i == IDs::name) {
            nameLabel.setText(v[IDs::name].toString(), dontSendNotification);
        } else if (i == IDs::colour) {
            nameLabel.setColour(Label::backgroundColourId, getColour());
        }
    }
};
