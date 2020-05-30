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
    }

    ~ContextPane() override {
        view.removeListener(this);
        tracks.removeListener(this);
    }

    void resized() override {
        if (gridCells.isEmpty() && masterTrackGridCells.isEmpty())
            return;

        int cellWidth = 20;
        int cellHeight = 20;
        int newWidth = cellWidth * jmax(tracks.getNumNonMasterTracks(), view.getNumMasterProcessorSlots() + view.getGridViewTrackOffset());
        int newHeight = cellHeight * (view.getNumTrackProcessorSlots() + 2);
        setSize(newWidth, newHeight);

        auto r = getLocalBounds();

        auto trackViewOffset = view.getGridViewTrackOffset();
        auto masterViewSlotOffset = view.getMasterViewSlotOffset();

        auto masterRowBounds = r.removeFromBottom(cellHeight);
        masterRowBounds.removeFromLeft(jmax(0, trackViewOffset - masterViewSlotOffset) * cellWidth);

        auto tracksBounds = r;
        tracksBounds.removeFromLeft(jmax(0, masterViewSlotOffset - trackViewOffset) * cellWidth);

        if (!masterTrackGridCells.isEmpty()) {
            for (auto *processorGridCell : masterTrackGridCells) {
                processorGridCell->setRectangle(masterRowBounds.removeFromLeft(cellWidth).reduced(2).toFloat());
            }
        }

        if (!gridCells.isEmpty()) {
            for (auto *trackGridCells : gridCells) {
                auto column = tracksBounds.removeFromLeft(cellWidth);
                for (int i = trackGridCells->size() - 1; i >= 0; i--) {
                    trackGridCells->getUnchecked(i)->setRectangle(column.removeFromBottom(cellHeight).reduced(2).toFloat());
                }
            }
        }
    }

private:
    class GridCell : public DrawableRectangle {
    public:
        explicit GridCell(Component *parent) {
            setTrackAndProcessor({}, {}, true, false, false);
            setCornerSize({3, 3});
            parent->addAndMakeVisible(this);
        }

        void setTrackAndProcessor(const ValueTree &track, const ValueTree &processor, bool inView, bool slotSelected, bool slotFocused) {
            const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4);

            // this is the only part different than GraphEditorProcessors
            auto colour = processor.isValid() ? findColour(TextEditor::backgroundColourId) : baseColour;

            if (TracksState::doesTrackHaveSelections(track))
                colour = colour.brighter(processor.isValid() ? 0.04 : 0.15);
            if (slotSelected)
                colour = TracksState::getTrackColour(track);
            if (slotFocused)
                colour = colour.brighter(0.16);
            if (!inView)
                colour = colour.darker(0.3);

            setFill(colour);
        }
    };

    TracksState &tracks;
    ViewState &view;

    // indexed by [column(track)][row(processorSlot)] rather than the usual [row][column] matrix convention.
    // Just makes more sense in this track-first world!
    OwnedArray<OwnedArray<GridCell> > gridCells;
    OwnedArray<GridCell> masterTrackGridCells;

    void updateGridCellColours() {
        if (gridCells.isEmpty())
            return;

        for (const auto &track : tracks.getState()) {
            auto trackIndex = tracks.indexOf(track);
            const auto &trackGridCells = tracks.isMasterTrack(track) ? &masterTrackGridCells : gridCells.getUnchecked(trackIndex);
            for (auto processorSlot = 0; processorSlot < trackGridCells->size(); processorSlot++) {
                auto *cell = trackGridCells->getUnchecked(processorSlot);
                const auto &processor = TracksState::getProcessorAtSlot(track, processorSlot);
                cell->setTrackAndProcessor(track, processor,
                                           view.isProcessorSlotInView(track, processorSlot),
                                           TracksState::isSlotSelected(track, processorSlot),
                                           tracks.isSlotFocused(track, processorSlot));
            }
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK)) {
            if (TracksState::isMasterTrack(child)) {
                return valueTreePropertyChanged(view.getState(), IDs::numMasterProcessorSlots);
            } else {
                auto *newGridCellColumn = new OwnedArray<GridCell>();
                int numProcessorSlots = view.getNumTrackProcessorSlots();
                for (int processorSlot = 0; processorSlot < numProcessorSlots; processorSlot++) {
                    newGridCellColumn->add(new GridCell(this));
                }
                gridCells.add(newGridCellColumn);
            }
            resized();
        } else if (child.hasType(IDs::PROCESSOR)) {
            updateGridCellColours();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) override {
        if (child.hasType(IDs::TRACK)) {
            if (TracksState::isMasterTrack(child)) {
                masterTrackGridCells.clear();
            } else {
                gridCells.remove(index);
            }
            resized();
            updateGridCellColours();
        } else if (child.hasType(IDs::PROCESSOR)) {
            updateGridCellColours();
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::VIEW_STATE)) {
            if (i == IDs::numProcessorSlots) {
                if (gridCells.isEmpty())
                    return;
                int numProcessorSlots = tree[IDs::numProcessorSlots];
                while (gridCells.getFirst()->size() < numProcessorSlots) {
                    for (auto trackIndex = 0; trackIndex < gridCells.size(); trackIndex++) {
                        auto *trackGridCells = gridCells.getUnchecked(trackIndex);
                        trackGridCells->add(new GridCell(this));
                    }
                }
                while (gridCells.getFirst()->size() > numProcessorSlots) {
                    for (auto trackIndex = 0; trackIndex < gridCells.size(); trackIndex++) {
                        auto *trackGridCells = gridCells.getUnchecked(trackIndex);
                        trackGridCells->removeLast(1, true);
                    }
                }
                resized();
                updateGridCellColours();
            } else if (i == IDs::numMasterProcessorSlots) {
                int numProcessorSlots = tree[IDs::numMasterProcessorSlots];
                while (masterTrackGridCells.size() < numProcessorSlots) {
                    masterTrackGridCells.add(new GridCell(this));
                }
                masterTrackGridCells.removeLast(masterTrackGridCells.size() - numProcessorSlots, true);
                resized();
                updateGridCellColours();
            } else if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
                resized();
                updateGridCellColours();
            } else if (i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot || i == IDs::gridViewSlotOffset) {
                updateGridCellColours();
            }
        } else if (i == IDs::selectedSlotsMask) {
            updateGridCellColours();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}
};
