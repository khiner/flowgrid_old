#pragma once

#include <Identifiers.h>
#include "JuceHeader.h"
#include "Project.h"

class ContextPane : public Component, private ValueTree::Listener {
public:
    explicit ContextPane(Project &project) : project(project) {
        project.getState().addListener(this);
    }

    ~ContextPane() override {
        project.getState().removeListener(this);
    }

    void resized() override {
        if (gridCells.isEmpty() && masterTrackGridCells.isEmpty())
            return;

        int cellWidth = 20;
        int cellHeight = 20;

        auto r = getLocalBounds();

        if (!masterTrackGridCells.isEmpty()) {
            auto bottom = r.removeFromBottom(cellHeight);
            for (auto* processorGridCell : masterTrackGridCells) {
                processorGridCell->setRectangle(bottom.removeFromLeft(cellWidth).reduced(2).toFloat());
            }
        }

        if (!gridCells.isEmpty()) {
            for (auto *trackGridCells : gridCells) {
                auto column = r.removeFromLeft(cellWidth);
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
            setTrackAndProcessor({}, {}, true, false);
            setCornerSize({3, 3});
            parent->addAndMakeVisible(this);
        }
        
        void setTrackAndProcessor(const ValueTree &track, const ValueTree &processor, bool inView, bool selected) {
            const static Colour baseColour = findColour(TextEditor::backgroundColourId);

            Colour colour;
            if (selected && inView && !processor.isValid())
                colour = Colour::fromString(track[IDs::colour].toString()).brighter();
            else if (selected && inView && processor.isValid())
                colour = Colour::fromString(track[IDs::colour].toString()).darker();
            else if (inView && processor.isValid())
                colour = baseColour.darker(0.2f);
            else if (!inView && processor.isValid())
                colour = baseColour.darker(0.05f);
            else if (inView)
                colour = baseColour.brighter(0.6f);
            else
                colour = baseColour.brighter(0.2f);

            setFill(colour);
        }
    };

    Project &project;

    // indexed by [column(track)][row(processorSlot)] rather than the usual [row][column] matrix convention.
    // Just makes more sense in this track-first world!
    OwnedArray<OwnedArray<GridCell> > gridCells;
    OwnedArray<GridCell> masterTrackGridCells;

    void updateGridCellColours() {
        if (gridCells.isEmpty())
            return;

        auto trackViewOffset = project.getGridViewTrackOffset();
        auto slotViewOffset = project.getGridViewSlotOffset();
        auto masterViewSlotOffset = project.getMasterViewSlotOffset();
        auto numTrackSlots = project.getNumTrackProcessorSlots();
        auto numMasterTrackSlots = project.getNumMasterProcessorSlots();

        const auto& masterTrack = project.getMasterTrack();
        for (auto processorCellIndex = 0; processorCellIndex < masterTrackGridCells.size(); processorCellIndex++) {
            auto processorSlot = processorCellIndex == masterTrackGridCells.size() - 1 ? Project::MIXER_CHANNEL_SLOT : processorCellIndex;
            auto *cell = masterTrackGridCells.getUnchecked(processorCellIndex);
            const auto &processor = masterTrack.getChildWithProperty(IDs::processorSlot, processorSlot);

            bool inView = processorCellIndex >= masterViewSlotOffset &&
                          processorCellIndex < masterViewSlotOffset + numMasterTrackSlots &&
                          slotViewOffset + Project::NUM_VISIBLE_TRACK_PROCESSOR_SLOTS >= numTrackSlots;
            cell->setTrackAndProcessor(masterTrack, processor, inView, project.isTrackSelected(masterTrack));
            auto rectangle = cell->getRectangle().getBoundingBox().withX((trackViewOffset + processorCellIndex) * (cell->getWidth() + 4) + 2);
            cell->setRectangle(rectangle);
        }

        for (auto trackIndex = 0; trackIndex < gridCells.size(); trackIndex++) {
            auto* trackGridCells = gridCells.getUnchecked(trackIndex);
            const auto& track = project.getTrack(trackIndex);
            for (auto processorCellIndex = 0; processorCellIndex < trackGridCells->size(); processorCellIndex++) {
                auto processorSlot = processorCellIndex == trackGridCells->size() - 1 ? Project::MIXER_CHANNEL_SLOT : processorCellIndex;
                auto *cell = trackGridCells->getUnchecked(processorCellIndex);
                const auto& processor = track.getChildWithProperty(IDs::processorSlot, processorSlot);
                bool inView = trackIndex >= trackViewOffset &&
                              trackIndex < trackViewOffset + Project::NUM_VISIBLE_TRACKS &&
                              processorCellIndex >= slotViewOffset &&
                              processorCellIndex <= slotViewOffset + Project::NUM_VISIBLE_TRACK_PROCESSOR_SLOTS;
                cell->setTrackAndProcessor(track, processor, inView, project.isTrackSelected(track));
            }
        }
    }
    
    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK)) {
            if (child.hasProperty(IDs::isMasterTrack)) {
                int numProcessorSlots = project.getNumMasterProcessorSlots();
                for (int processorSlot = 0; processorSlot < numProcessorSlots; processorSlot++) {
                    masterTrackGridCells.add(new GridCell(this));
                }
            } else {
                auto *newGridCellColumn = new OwnedArray<GridCell>();
                int numProcessorSlots = project.getNumTrackProcessorSlots();
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
            if (child.hasProperty(IDs::isMasterTrack)) {
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
            } else if (i == IDs::gridViewTrackOffset || i == IDs::gridViewSlotOffset || i == IDs::masterViewSlotOffset) {
                updateGridCellColours();
            }
        } else if ((tree.hasType(IDs::TRACK) || tree.hasType(IDs::PROCESSOR)) && i == IDs::selected && tree[IDs::selected]) {
            updateGridCellColours();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}
};
