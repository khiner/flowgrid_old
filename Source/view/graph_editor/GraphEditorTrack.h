#pragma once

#include <Utilities.h>
#include <state/Identifiers.h>
#include "JuceHeader.h"
#include "GraphEditorProcessors.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer, private ChangeListener {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(project.getView()), state(state),
              connectorDragListener(connectorDragListener), processors(project, state, connectorDragListener) {
        nameLabel.setJustificationType(Justification::centred);
        temporaryOutputLabel.setJustificationType(Justification::centred);
        onColourChanged();
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(temporaryOutputLabel);
        addAndMakeVisible(processors);
        if (!isMasterTrack()) {
            nameLabel.setText(getTrackName(), dontSendNotification);
            nameLabel.setEditable(false, true);
            nameLabel.onTextChange = [this] { this->tracks.setTrackName(this->state, nameLabel.getText(false)); };
        } else {
            masterTrackName.setText(getTrackName());
            masterTrackName.setColour(findColour(TextEditor::textColourId));
            masterTrackName.setJustification(Justification::centred);
            addAndMakeVisible(masterTrackName);
        }
        nameLabel.addMouseListener(this, false);
        this->state.addListener(this);
        view.addListener(this);
    }

    ~GraphEditorTrack() override {
        nameLabel.removeMouseListener(this);
        view.removeListener(this);
        this->state.removeListener(this);
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mods.isPopupMenu()) {
                auto *colourSelector = new ColourSelector();
                colourSelector->setName("background");
                colourSelector->setCurrentColour(findColour(TextButton::buttonColourId));
                colourSelector->addChangeListener(this);
                colourSelector->setColour(ColourSelector::backgroundColourId, Colours::transparentBlack);
                colourSelector->setSize(300, 400);

                CallOutBox::launchAsynchronously(colourSelector, getScreenBounds(), nullptr);
            } else {
                bool isTrackSelected = isSelected();
                project.setTrackSelected(state, !(isTrackSelected && e.mods.isCommandDown()), !(isTrackSelected || e.mods.isCommandDown()));
            }
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
                // single click deselects other tracks/processors
                project.setTrackSelected(state, true);
            }
        }
    }

    const ValueTree &getState() const { return state; }

    bool isMasterTrack() const { return TracksState::isMasterTrack(state); }

    int getTrackIndex() const { return state.getParent().indexOf(state); }

    String getTrackName() const { return state[IDs::name].toString(); }

    Colour getColour() const {
        const Colour &trackColour = TracksState::getTrackColour(state);
        return isSelected() ? trackColour.brighter(0.25) : trackColour;
    }

    bool isSelected() const { return state.getProperty(IDs::selected); }

    void resized() override {
        auto r = getLocalBounds();
        processors.setBounds(r);
        const auto &nameLabelBounds = isMasterTrack()
                                      ? r.removeFromLeft(ViewState::TRACK_LABEL_HEIGHT)
                                      : r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT);
        nameLabel.setBounds(nameLabelBounds);
        nameLabel.toFront(false);
        const auto &temporaryOutputLabelBounds = isMasterTrack()
                                                 ? r.removeFromRight(ViewState::TRACK_OUTPUT_HEIGHT)
                                                 : r.removeFromBottom(ViewState::TRACK_OUTPUT_HEIGHT);
        temporaryOutputLabel.setBounds(temporaryOutputLabelBounds);
        if (isMasterTrack()) {
            const auto &labelBoundsFloat = nameLabelBounds.toFloat();
            masterTrackName.setBoundingBox(Parallelogram<float>(labelBoundsFloat.getBottomLeft(), labelBoundsFloat.getTopLeft(), labelBoundsFloat.getBottomRight()));
            masterTrackName.setFontHeight(3 * ViewState::TRACK_LABEL_HEIGHT / 4);
            masterTrackName.toFront(false);
        }
    }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (auto *currentlyMovingProcessor = processors.getCurrentlyMovingProcessor()) {
            if (currentlyMovingProcessor->getNodeId() == nodeId) {
                return currentlyMovingProcessor;
            }
        }
        return processors.getProcessorForNodeId(nodeId);
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        return processors.findPinAt(e);
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        processors.setCurrentlyMovingProcessor(currentlyMovingProcessor);
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ValueTree state;

    Label nameLabel, temporaryOutputLabel;
    DrawableText masterTrackName;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessors processors;

    void onColourChanged() {
        nameLabel.setColour(Label::backgroundColourId, getColour());
        temporaryOutputLabel.setColour(Label::backgroundColourId, getColour());
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::gridViewSlotOffset || ((i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) && isMasterTrack()))
            resized();

        if (tree != state)
            return;

        if (i == IDs::name) {
            nameLabel.setText(tree[IDs::name].toString(), dontSendNotification);
        } else if (i == IDs::colour || i == IDs::selected) {
            onColourChanged();
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (auto *cs = dynamic_cast<ColourSelector *> (source)) {
            tracks.setTrackColour(state, cs->getCurrentColour());
        }
    }
};
