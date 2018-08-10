#pragma once

#include <Utilities.h>
#include <Identifiers.h>
#include "JuceHeader.h"
#include "GraphEditorProcessors.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer, private ChangeListener {
public:
    explicit GraphEditorTrack(Project& project, ValueTree v, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : project(project), state(std::move(v)), connectorDragListener(connectorDragListener), graph(graph) {
        nameLabel.setText(isMasterTrack() ? "M\nA\nS\nT\nE\nR" : getTrackName(), dontSendNotification);
        nameLabel.setJustificationType(Justification::centred);
        nameLabel.setColour(Label::backgroundColourId, getColour());
        if (!isMasterTrack()) {
            nameLabel.setEditable(false, true);
            nameLabel.onTextChange = [this] { state.setProperty(IDs::name, nameLabel.getText(false), &this->graph.undoManager); };
        }

        nameLabel.addMouseListener(this, false);

        addAndMakeVisible(nameLabel);
        addAndMakeVisible(*(processors = std::make_unique<GraphEditorProcessors>(project, state, connectorDragListener, graph)));
        state.addListener(this);
    }

    const ValueTree& getState() const {
        return state;
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mods.isRightButtonDown()) {
                auto *colourSelector = new ColourSelector();
                colourSelector->setName("background");
                colourSelector->setCurrentColour(findColour(TextButton::buttonColourId));
                colourSelector->addChangeListener(this);
                colourSelector->setColour(ColourSelector::backgroundColourId, Colours::transparentBlack);
                colourSelector->setSize(300, 400);

                CallOutBox::launchAsynchronously(colourSelector, getScreenBounds(), nullptr);
            } else {
                select();
            }
        }
    }

    bool isMasterTrack() const {
        return state.hasType(IDs::MASTER_TRACK);
    }

    int getTrackIndex() const {
        return state.getParent().indexOf(state);
    }

    String getTrackName() const {
        return state[IDs::name].toString();
    }

    Colour getColour() const {
        return Colour::fromString(state[IDs::colour].toString());
    }

    void setColour(const Colour& colour) {
        project.setTrackColour(state, colour, &graph.undoManager);
    }

    bool isSelected() const {
        return state.getProperty(IDs::selected);
    }

    void select() {
        state.setProperty(IDs::selected, true, nullptr);
    }

    const Component *getDragControlComponent() const {
        return &nameLabel;
    }

    void resized() override {
        auto r = getLocalBounds();
        nameLabel.setBounds(isMasterTrack() ? r.removeFromLeft(getWidth() / 24) : r.removeFromTop(getHeight() / 24));
        processors->setBounds(r);
    }

    void paint(Graphics &g) override {
        if (isSelected() || processors->anySelected()) {
            g.fillAll(getColour().withAlpha(0.10f).withMultipliedBrightness(1.5f));
        }
    }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (auto *currentlyMovingProcessor = processors->getCurrentlyMovingProcessor()) {
            if (currentlyMovingProcessor->getNodeId() == nodeId) {
                return currentlyMovingProcessor;
            }
        }
        return processors->getProcessorForNodeId(nodeId);
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        return processors->findPinAt(e);
    }

    int findSlotAt(const MouseEvent &e) {
        return processors->findSlotAt(e);
    }

    void update() {
        repaint();
        processors->update();
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        processors->setCurrentlyMovingProcessor(currentlyMovingProcessor);
    }

private:
    Project& project;
    ValueTree state;

    Label nameLabel;
    std::unique_ptr<GraphEditorProcessors> processors;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v.hasType(IDs::PROCESSOR) && i == IDs::selected)
            repaint();

        if (v != state)
            return;
        if (i == IDs::name) {
            nameLabel.setText(v[IDs::name].toString(), dontSendNotification);
        } else if (i == IDs::colour) {
            nameLabel.setColour(Label::backgroundColourId, getColour());
            repaint();
        } else if (i == IDs::selected) {
            if (v[IDs::selected]) {
                processors->select();
                // selecting a processor deselects everything else.
                state.setPropertyExcludingListener(this, IDs::selected, true, nullptr);
            }
            nameLabel.setColour(Label::backgroundColourId, isSelected() ? getColour().brighter(0.25) : getColour());
            repaint();
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (auto *cs = dynamic_cast<ColourSelector *> (source)) {
            setColour(cs->getCurrentColour());
        }
    }
};
