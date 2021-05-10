#include "ContextPane.h"

#include "state/Identifiers.h"

ContextPane::ContextPane(Project &project)
        : tracks(project.getTracks()), view(project.getView()) {
    tracks.addListener(this);
    view.addListener(this);
    cellPath.addRoundedRectangle(Rectangle<int>(cellWidth, cellHeight).reduced(2), 3);
    trackBorderPath.addRoundedRectangle(Rectangle<int>(cellWidth, cellWidth * (ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1)).reduced(1), 3);
    masterTrackBorderPath.addRoundedRectangle(Rectangle<int>(cellWidth * (ViewState::NUM_VISIBLE_MASTER_TRACK_SLOTS + 1), cellHeight).reduced(1), 3);
}

ContextPane::~ContextPane() {
    view.removeListener(this);
    tracks.removeListener(this);
}

void ContextPane::paint(Graphics &g) {
    juce::Point<int> previousCellPosition(0, 0);
    juce::Point<int> previousTrackBorderPosition(0, 0);
    juce::Point<int> previousMasterTrackBorderPosition(0, 0);

    auto masterRowY = getHeight() - cellHeight;
    const int tracksOffset = jmax(0, view.getMasterViewSlotOffset() - view.getGridViewTrackOffset()) * cellWidth;

    for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++) {
        const auto &track = tracks.getTrack(trackIndex);
        bool isMaster = TracksState::isMasterTrack(track);
        const int trackX = tracksOffset + trackIndex * cellWidth;

        const auto &trackColour = TracksState::getTrackColour(track);
        g.setColour(trackColour);
        if (isMaster) {
            const int slotOffsetX = view.getMasterViewSlotOffset() * cellWidth;
            const juce::Point<int> masterTrackBorderPosition(slotOffsetX, masterRowY);
            masterTrackBorderPath.applyTransform(AffineTransform::translation(masterTrackBorderPosition - previousMasterTrackBorderPosition));
            g.strokePath(masterTrackBorderPath, PathStrokeType(1.0));
            previousMasterTrackBorderPosition = masterTrackBorderPosition;
        } else {
            const int slotOffsetY = view.getGridViewSlotOffset() * cellHeight;
            const juce::Point<int> trackBorderPosition(trackX, slotOffsetY);
            trackBorderPath.applyTransform(AffineTransform::translation(trackBorderPosition - previousTrackBorderPosition));
            g.strokePath(trackBorderPath, PathStrokeType(1.0));
            previousTrackBorderPosition = trackBorderPosition;
        }

        const auto &outputProcessor = TracksState::getOutputProcessorForTrack(track);
        auto trackOutputIndex = view.getSlotOffsetForTrack(track) + ViewState::getNumVisibleSlotsForTrack(track);
        for (auto gridCellIndex = 0; gridCellIndex < view.getNumSlotsForTrack(track) + 1; gridCellIndex++) {
            Colour cellColour;
            if (gridCellIndex == trackOutputIndex) {
                cellColour = getFillColour(trackColour, track, outputProcessor, view.isTrackInView(track), true, false);
            } else {
                int slot = gridCellIndex < trackOutputIndex ? gridCellIndex : gridCellIndex - 1;
                const auto &processor = TracksState::getProcessorAtSlot(track, slot);
                cellColour = getFillColour(trackColour, track, processor,
                                           view.isProcessorSlotInView(track, slot),
                                           TracksState::isSlotSelected(track, slot),
                                           tracks.isSlotFocused(track, slot));
            }
            g.setColour(cellColour);
            const auto &cellPosition = isMaster ? juce::Point<int>(gridCellIndex * cellWidth, masterRowY) : juce::Point<int>(trackX, gridCellIndex * cellHeight);
            cellPath.applyTransform(AffineTransform::translation(cellPosition - previousCellPosition));
            g.fillPath(cellPath);
            previousCellPosition = cellPosition;
        }
    }
    // Move paths back to origin for the next draw.
    cellPath.applyTransform(AffineTransform::translation(-previousCellPosition));
    trackBorderPath.applyTransform(AffineTransform::translation(-previousTrackBorderPosition));
    masterTrackBorderPath.applyTransform(AffineTransform::translation(-previousMasterTrackBorderPosition));
}

void ContextPane::resized() {
    int numColumns = std::max(tracks.getNumNonMasterTracks(), view.getNumMasterProcessorSlots() + (view.getMasterViewSlotOffset() - view.getGridViewTrackOffset()) + 1); // + master track output
    int numRows = view.getNumTrackProcessorSlots() + 2;  // + track output row + master track
    setSize(cellWidth * numColumns, cellHeight * numRows);
}

Colour ContextPane::getFillColour(const Colour &trackColour, const ValueTree &track, const ValueTree &processor, bool inView, bool slotSelected, bool slotFocused) {
    const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4f);

    // this is the only part different than GraphEditorProcessorLane
    auto colour = processor.isValid() ? findColour(TextEditor::backgroundColourId) : baseColour;

    if (TracksState::doesTrackHaveSelections(track))
        colour = colour.brighter(processor.isValid() ? 0.04f : 0.15f);
    if (slotSelected)
        colour = trackColour;
    if (slotFocused)
        colour = colour.brighter(0.16f);
    if (!inView)
        colour = colour.darker(0.3f);

    return colour;
}

void ContextPane::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (child.hasType(IDs::TRACK))
        resized();
    else if (child.hasType(IDs::PROCESSOR))
        repaint();
}

void ContextPane::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) {
    if (child.hasType(IDs::TRACK))
        resized();
    else if (child.hasType(IDs::PROCESSOR))
        repaint();
}

void ContextPane::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (tree.hasType(IDs::VIEW_STATE)) {
        if (i == IDs::numProcessorSlots || i == IDs::numMasterProcessorSlots || i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset)
            resized();
        else if (i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot || i == IDs::gridViewSlotOffset)
            repaint();
    } else if (i == IDs::selectedSlotsMask) {
        repaint();
    }
}
