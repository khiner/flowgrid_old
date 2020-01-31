#pragma once

#include <state/Identifiers.h>
#include "JuceHeader.h"
#include "state/Project.h"

class ContextPane :
        public Component,
        private ValueTree::Listener {
public:
    explicit ContextPane(Project &project)
            : project(project), tracks(project.getTracksManager()),
              view(project.getViewStateManager()) {
        this->project.getState().addListener(this);
    }

    ~ContextPane() override {
        project.getState().removeListener(this);
    }

    void resized() override {
        if (gridCells.isEmpty() && masterTrackGridCells.isEmpty())
            return;

        int cellWidth = 20;
        int cellHeight = 20;
        int newWidth = cellWidth * jmax(tracks.getNumNonMasterTracks(), view.getNumMasterProcessorSlots());
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
            for (auto* processorGridCell : masterTrackGridCells) {
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
        explicit GridCell(Component* parent) {
            setTrackAndProcessor({}, {}, true, false, false);
            setCornerSize({3, 3});
            parent->addAndMakeVisible(this);
        }
        
        void setTrackAndProcessor(const ValueTree &track, const ValueTree &processor,
                                  bool inView, bool trackSelected, bool processorSelected) {
            const static Colour baseColour = findColour(TextEditor::backgroundColourId);

            Colour colour;
            if (processorSelected && inView)
                colour = Colour::fromString(track[IDs::colour].toString()).darker();
            else if (inView && processor.isValid())
                colour = baseColour.darker(0.2);
            else if (!inView && processor.isValid())
                colour = baseColour;
            else if (inView)
                colour = baseColour.brighter(trackSelected ? 0.9f : 0.55f);
            else
                colour = baseColour.brighter(0.2);

            setFill(colour);
        }
    };

    Project &project;
    TracksState &tracks;
    ViewState &view;

    // indexed by [column(track)][row(processorSlot)] rather than the usual [row][column] matrix convention.
    // Just makes more sense in this track-first world!
    OwnedArray<OwnedArray<GridCell> > gridCells;
    OwnedArray<GridCell> masterTrackGridCells;

    void updateGridCellColours() {
        if (gridCells.isEmpty())
            return;

        const auto& masterTrack = tracks.getMasterTrack();
        bool trackSelected = tracks.isTrackSelected(masterTrack);
        for (auto processorSlot = 0; processorSlot < masterTrackGridCells.size(); processorSlot++) {
            auto *cell = masterTrackGridCells.getUnchecked(processorSlot);
            const auto &processor = TracksState::findProcessorAtSlot(masterTrack, processorSlot);
            bool inView = view.isProcessorSlotInView(masterTrack, processorSlot);
            cell->setTrackAndProcessor(masterTrack, processor, inView, trackSelected, tracks.isSlotSelected(masterTrack, processorSlot));
        }

        for (auto trackIndex = 0; trackIndex < gridCells.size(); trackIndex++) {
            auto* trackGridCells = gridCells.getUnchecked(trackIndex);
            const auto& track = tracks.getTrack(trackIndex);
            trackSelected = tracks.isTrackSelected(track);
            for (auto processorSlot = 0; processorSlot < trackGridCells->size(); processorSlot++) {
                auto *cell = trackGridCells->getUnchecked(processorSlot);
                const auto& processor = TracksState::findProcessorAtSlot(track, processorSlot);
                bool inView = view.isProcessorSlotInView(track, processorSlot);
                cell->setTrackAndProcessor(track, processor, inView, trackSelected, tracks.isSlotSelected(track, processorSlot));
            }
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK)) {
            if (TracksState::isMasterTrack(child)) {
                return valueTreePropertyChanged(project.getView().getState(), IDs::numMasterProcessorSlots);
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
            } else if (i == IDs::gridViewSlotOffset) {
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
