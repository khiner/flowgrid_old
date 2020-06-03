#pragma once

#include <Utilities.h>
#include <state/Identifiers.h>
#include <view/graph_editor/processor/TrackInputGraphEditorProcessor.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLane.h"
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer, private ChangeListener {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(project.getView()), state(state),
              connectorDragListener(connectorDragListener), lane(project, TracksState::getProcessorLaneForTrack(state), connectorDragListener) {
        nameLabel.setJustificationType(Justification::centred);
        trackBorder.setFill(Colours::transparentBlack);
        trackBorder.setStrokeThickness(2);
        trackBorder.setCornerSize({8, 8});

        addAndMakeVisible(trackBorder);
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(lane);
        trackInputProcessorChanged();
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

        const auto &borderBounds = isMasterTrack() ? r.toFloat() : r.toFloat().withTrimmedBottom(ViewState::TRACK_BOTTOM_MARGIN);
        trackBorder.setRectangle(borderBounds);

        const int trackInputProcessorSize = ViewState::TRACK_LABEL_HEIGHT;
        auto inputAndLabelBounds = isMasterTrack()
                                          ? r.removeFromLeft(ViewState::TRACK_LABEL_HEIGHT)
                                          : r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT);
        const auto &trackInputBounds = isMasterTrack()
                                       ? inputAndLabelBounds.removeFromTop(trackInputProcessorSize)
                                       : inputAndLabelBounds.removeFromLeft(trackInputProcessorSize);
        const auto &trackOutputBounds = isMasterTrack()
                                        ? r.removeFromRight(lane.getProcessorSlotSize())
                                        : r.removeFromBottom(lane.getProcessorSlotSize());

        nameLabel.setBounds(inputAndLabelBounds);
//        nameLabel.toFront(false);
        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setBounds(trackInputBounds);
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setBounds(trackOutputBounds);
        if (isMasterTrack()) {
            const auto &labelBoundsFloat = inputAndLabelBounds.toFloat();
            masterTrackName.setBoundingBox(Parallelogram<float>(labelBoundsFloat.getBottomLeft(), labelBoundsFloat.getTopLeft(), labelBoundsFloat.getBottomRight()));
            masterTrackName.setFontHeight(3 * ViewState::TRACK_LABEL_HEIGHT / 4);
//            masterTrackName.toFront(false);
        }
        lane.setBounds(r.reduced(2, 0));
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (trackInputProcessorView != nullptr && trackInputProcessorView->getNodeId() == nodeId)
            return trackInputProcessorView.get();
        if (trackOutputProcessorView != nullptr && trackOutputProcessorView->getNodeId() == nodeId)
            return trackOutputProcessorView.get();
        if (auto *currentlyMovingProcessor = lane.getCurrentlyMovingProcessor())
            if (currentlyMovingProcessor->getNodeId() == nodeId)
                return currentlyMovingProcessor;

        return lane.getProcessorForNodeId(nodeId);
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) {
        auto *pin = lane.findPinAt(e);
        if (pin != nullptr)
            return pin;
        if (trackInputProcessorView != nullptr)
             pin = trackInputProcessorView->findPinAt(e);
        if (pin != nullptr)
            return pin;
        if (trackOutputProcessorView != nullptr)
            return trackOutputProcessorView->findPinAt(e);
        return nullptr;
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
    std::unique_ptr<TrackInputGraphEditorProcessor> trackInputProcessorView;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    DrawableText masterTrackName;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLane lane;
    DrawableRectangle trackBorder;

    void onColourChanged() {
        const auto &colour = getColour();
        trackBorder.setStrokeFill(colour);
        nameLabel.setColour(Label::backgroundColourId, colour);
        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
    }

    void trackInputProcessorChanged() {
        const ValueTree &trackInputProcessor = TracksState::getInputProcessorForTrack(state);
        if (trackInputProcessor.isValid()) {
            trackInputProcessorView = std::make_unique<TrackInputGraphEditorProcessor>(project, tracks, view, trackInputProcessor, connectorDragListener);
            addAndMakeVisible(trackInputProcessorView.get());
            resized();
        } else {
            removeChildComponent(trackInputProcessorView.get());
            trackInputProcessorView = nullptr;
        }
        onColourChanged();
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
        if (child.hasType(IDs::PROCESSOR)) {
            if (child.getProperty(IDs::name) == TrackInputProcessor::name())
                trackInputProcessorChanged();
            else if (child.getProperty(IDs::name) == TrackOutputProcessor::name())
                trackOutputProcessorChanged();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child.getProperty(IDs::name) == TrackInputProcessor::name())
                trackInputProcessorChanged();
            else if (child.getProperty(IDs::name) == TrackOutputProcessor::name())
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
