#pragma once

#include <Utilities.h>
#include <state/Identifiers.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLane.h"
#include "TrackOutputGraphEditorProcessor.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer, private ChangeListener {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(project.getView()), state(state),
              connectorDragListener(connectorDragListener), lane(project, TracksState::getProcessorLaneForTrack(state), connectorDragListener) {
        nameLabel.setJustificationType(Justification::centred);
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(lane);
        trackOutputProcessorChanged();

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
                project.beginDragging({getTrackIndex(), -1});
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
        const auto &nameLabelBounds = isMasterTrack()
                                      ? r.removeFromLeft(ViewState::TRACK_LABEL_HEIGHT)
                                      : r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT);
        nameLabel.setBounds(nameLabelBounds);
        nameLabel.toFront(false);
        const auto &trackOutputBounds = isMasterTrack()
                                        ? r.removeFromRight(lane.getProcessorSlotSize())
                                        : r.removeFromBottom(lane.getProcessorSlotSize());
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setBounds(trackOutputBounds);
        if (isMasterTrack()) {
            const auto &labelBoundsFloat = nameLabelBounds.toFloat();
            masterTrackName.setBoundingBox(Parallelogram<float>(labelBoundsFloat.getBottomLeft(), labelBoundsFloat.getTopLeft(), labelBoundsFloat.getBottomRight()));
            masterTrackName.setFontHeight(3 * ViewState::TRACK_LABEL_HEIGHT / 4);
            masterTrackName.toFront(false);
        }
        lane.setBounds(r);
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (trackOutputProcessorView != nullptr && trackOutputProcessorView->getNodeId() == nodeId)
            return trackOutputProcessorView.get();
        if (auto *currentlyMovingProcessor = lane.getCurrentlyMovingProcessor()) {
            if (currentlyMovingProcessor->getNodeId() == nodeId) {
                return currentlyMovingProcessor;
            }
        }
        return lane.getProcessorForNodeId(nodeId);
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) {
        auto *pin = lane.findPinAt(e);
        return pin != nullptr ? pin : (trackOutputProcessorView != nullptr ? trackOutputProcessorView->findPinAt(e) : nullptr);
    }

    void setCurrentlyMovingProcessor(BaseGraphEditorProcessor *currentlyMovingProcessor) {
        lane.setCurrentlyMovingProcessor(currentlyMovingProcessor);
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ValueTree state;

    Label nameLabel;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    DrawableText masterTrackName;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLane lane;

    void onColourChanged() {
        nameLabel.setColour(Label::backgroundColourId, getColour());
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setColour(Label::backgroundColourId, getColour());
    }

    void trackOutputProcessorChanged() {
        const ValueTree &trackOutputProcessor = TracksState::getOutputProcessorForTrack(state);
        if (trackOutputProcessor.isValid()) {
            trackOutputProcessorView = std::make_unique<TrackOutputGraphEditorProcessor>(project, tracks, view, trackOutputProcessor, connectorDragListener);
            addAndMakeVisible(trackOutputProcessorView.get());
            resized();
        } else {
            removeChildComponent(trackOutputProcessorView.get());
            trackOutputProcessorView = nullptr;
        }
        onColourChanged();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::PROCESSOR) && child.getProperty(IDs::name) == TrackOutputProcessor::name()) {
            trackOutputProcessorChanged();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::PROCESSOR) && child.getProperty(IDs::name) == TrackOutputProcessor::name()) {
            trackOutputProcessorChanged();
        }
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
