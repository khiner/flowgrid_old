#pragma once

#include <state/Identifiers.h>
#include "JuceHeader.h"
#include "state/Project.h"

class ContextPane :
        public Component,
        private ValueTree::Listener {
public:
    explicit ContextPane(Project &project)
            : tracks(project.getTracks()), view(project.getView()) {
        tracks.addListener(this);
        view.addListener(this);
        cellPath.addRoundedRectangle(Rectangle<int>(cellWidth, cellHeight).reduced(2), 3);
    }

    ~ContextPane() override {
        view.removeListener(this);
        tracks.removeListener(this);
    }

    void paint(Graphics &g) override {
        juce::Point<int> previousCellPosition(0, 0);

        auto r = getLocalBounds();
        auto masterRowBounds = r.removeFromBottom(cellHeight).withTrimmedLeft(jmax(0, view.getGridViewTrackOffset() - view.getMasterViewSlotOffset()) * cellWidth);
        auto tracksBounds = r.withTrimmedLeft(jmax(0, view.getMasterViewSlotOffset() - view.getGridViewTrackOffset()) * cellWidth);

        for (const auto &track : tracks.getState()) {
            bool isMaster = TracksState::isMasterTrack(track);
            auto trackBounds = isMaster ? masterRowBounds : tracksBounds.removeFromLeft(cellWidth);
            const auto &outputProcessor = TracksState::getOutputProcessorForTrack(track);
            auto trackOutputIndex = view.getSlotOffsetForTrack(track) + ViewState::getNumVisibleSlotsForTrack(track);
            for (auto gridCellIndex = 0; gridCellIndex < view.getNumSlotsForTrack(track) + 1; gridCellIndex++) {
                Colour cellColour;
                if (gridCellIndex == trackOutputIndex) {
                    cellColour = getFillColour(track, outputProcessor, view.isTrackInView(track), true, false);
                } else {
                    int slot = gridCellIndex < trackOutputIndex ? gridCellIndex : gridCellIndex - 1;
                    const auto &processor = TracksState::getProcessorAtSlot(track, slot);
                    cellColour = getFillColour(track, processor,
                                               view.isProcessorSlotInView(track, slot),
                                               TracksState::isSlotSelected(track, slot),
                                               tracks.isSlotFocused(track, slot));
                }
                g.setColour(cellColour);
                const auto &rectangle = isMaster ?
                        trackBounds.removeFromLeft(cellWidth).reduced(2) :
                        trackBounds.removeFromTop(cellHeight).reduced(2);
                cellPath.applyTransform(AffineTransform::translation(rectangle.getPosition() - previousCellPosition));
                g.fillPath(cellPath);
                previousCellPosition = rectangle.getPosition();
            }
        }
        // Move cell path back to origin for the next draw.
        cellPath.applyTransform(AffineTransform::translation(-previousCellPosition));
    }

    void resized() override {
        int numColumns = jmax(tracks.getNumNonMasterTracks(), view.getNumMasterProcessorSlots() + view.getGridViewTrackOffset() + 1); // + master track output
        int numRows = view.getNumTrackProcessorSlots() + 2;  // + track output row + master track
        setSize(cellWidth * numColumns, cellHeight * numRows);
    }

    Colour getFillColour(const ValueTree &track, const ValueTree &processor, bool inView, bool slotSelected, bool slotFocused) {
        const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4);

        // this is the only part different than GraphEditorProcessorLane
        auto colour = processor.isValid() ? findColour(TextEditor::backgroundColourId) : baseColour;

        if (TracksState::doesTrackHaveSelections(track))
            colour = colour.brighter(processor.isValid() ? 0.04 : 0.15);
        if (slotSelected)
            colour = TracksState::getTrackColour(track);
        if (slotFocused)
            colour = colour.brighter(0.16);
        if (!inView)
            colour = colour.darker(0.3);

        return colour;
    }

private:
    static constexpr int cellWidth = 20, cellHeight = 20;

    TracksState &tracks;
    ViewState &view;
    Path cellPath;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK))
            resized();
        else if (child.hasType(IDs::PROCESSOR))
            repaint();
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) override {
        if (child.hasType(IDs::TRACK))
            resized();
        else if (child.hasType(IDs::PROCESSOR))
            repaint();
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::VIEW_STATE)) {
            if (i == IDs::numProcessorSlots || i == IDs::numMasterProcessorSlots || i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset)
                resized();
            else if (i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot || i == IDs::gridViewSlotOffset)
                repaint();
        } else if (i == IDs::selectedSlotsMask) {
            repaint();
        }
    }
};
