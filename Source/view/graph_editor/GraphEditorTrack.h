#pragma once

#include <Utilities.h>
#include <Identifiers.h>
#include "JuceHeader.h"
#include "GraphEditorProcessors.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer, private ChangeListener {
public:
    explicit GraphEditorTrack(Project& project, ValueTree v, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : project(project), state(std::move(v)), connectorDragListener(connectorDragListener), graph(graph) {
        nameLabel.setJustificationType(Justification::centred);
        nameLabel.setColour(Label::backgroundColourId, getColour());
        addAndMakeVisible(nameLabel);
        if (!isMasterTrack()) {
            nameLabel.setText(getTrackName(), dontSendNotification);
            nameLabel.setEditable(false, true);
            nameLabel.onTextChange = [this] { state.setProperty(IDs::name, nameLabel.getText(false), &this->graph.undoManager); };
        } else {
            masterTrackName.setText(getTrackName());
            masterTrackName.setColour(findColour(TextEditor::textColourId));
            masterTrackName.setJustification(Justification::centred);
            addAndMakeVisible(masterTrackName);
        }
        nameLabel.addMouseListener(this, false);
        
        addAndMakeVisible(*(processors = std::make_unique<GraphEditorProcessors>(project, state, connectorDragListener, graph)));
        state.addListener(this);
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
                setSelected(true);
            }
        }
    }

    const ValueTree& getState() const { return state; }

    bool isMasterTrack() const { return state.hasProperty(IDs::isMasterTrack); }

    int getTrackIndex() const { return state.getParent().indexOf(state); }

    int getTrackViewIndex() const { return project.getViewIndexForTrack(state); }

    String getTrackName() const { return state[IDs::name].toString(); }

    Colour getColour() const { return Colour::fromString(state[IDs::colour].toString()); }

    void setColour(const Colour& colour) { state.setProperty(IDs::colour, colour.toString(), &graph.undoManager); }

    bool isSelected() const { return state.getProperty(IDs::selected); }

    void setSelected(bool selected) { state.setProperty(IDs::selected, selected, nullptr); }

    const Label *getNameLabel() const { return &nameLabel; }

    const Component *getDragControlComponent() const { return getNameLabel(); }

    void resized() override {
        auto r = getLocalBounds();
        processors->setBounds(r);
        const auto &nameLabelBounds = isMasterTrack()
                                      ? r.removeFromLeft(LABEL_HEIGHT)
                                         .withX(project.getTrackWidth() * processors->getSlotOffset())
                                      : r.removeFromTop(LABEL_HEIGHT)
                                         .withY(project.getProcessorHeight() * processors->getSlotOffset());
        nameLabel.setBounds(nameLabelBounds);
        nameLabel.toFront(false);
        if (isMasterTrack()) {
            const auto& labelBoundsFloat = nameLabelBounds.toFloat();
            masterTrackName.setBoundingBox(Parallelogram<float>(labelBoundsFloat.getBottomLeft(), labelBoundsFloat.getTopLeft(), labelBoundsFloat.getBottomRight()));
            masterTrackName.setFontHeight(3 * LABEL_HEIGHT / 4);
            masterTrackName.toFront(false);
        }
    }

    void paint(Graphics &g) override {
        if (isSelected() || processors->anySelected()) {
            g.fillAll(getColour().withAlpha(0.10f).withMultipliedBrightness(1.5f));
        }
    }

    void slotOffsetChanged() {
        resized();
        processors->slotOffsetChanged();
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

    static constexpr int LABEL_HEIGHT = 32;
private:
    Project& project;
    ValueTree state;

    Label nameLabel;
    DrawableText masterTrackName;
    std::unique_ptr<GraphEditorProcessors> processors;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v.hasType(IDs::PROCESSOR) && i == IDs::selected) {
            if (v[IDs::selected]) {
                state.setPropertyExcludingListener(this, IDs::selected, false, nullptr);
                nameLabel.setColour(Label::backgroundColourId, getColour());
            }
            repaint();
        }

        if (v != state)
            return;
        if (i == IDs::name) {
            nameLabel.setText(v[IDs::name].toString(), dontSendNotification);
        } else if (i == IDs::colour) {
            nameLabel.setColour(Label::backgroundColourId, getColour());
            repaint();
        } else if (i == IDs::selected) {
            processors->setSelected(v[IDs::selected], this);
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
