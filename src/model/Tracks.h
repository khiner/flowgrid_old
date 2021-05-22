#pragma once

#include "model/Track.h"
#include "model/View.h"
#include "view/PluginWindow.h"
#include "Stateful.h"
#include "PluginManager.h"

namespace TracksIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACKS)
#undef ID
}

struct Tracks : public Stateful<Tracks> {
    Tracks(View &view, PluginManager &pluginManager, UndoManager &undoManager);

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }

    void loadFromState(const ValueTree &fromState) override;

    static ValueTree getProcessorLanesForTrack(const ValueTree &track) {
        return track.isValid() ? track.getChildWithName(ProcessorLanesIDs::PROCESSOR_LANES) : ValueTree();
    }

    static ValueTree getProcessorLaneForTrack(const ValueTree &track) {
        const auto &lanes = getProcessorLanesForTrack(track);
        return lanes.isValid() ? lanes.getChild(0) : ValueTree();
    }

    static ValueTree getProcessorLaneForProcessor(const ValueTree &processor) {
        const auto &parent = processor.getParent();
        return parent.hasType(ProcessorLaneIDs::PROCESSOR_LANE) ? parent : getProcessorLaneForTrack(parent);
    }

    static ValueTree getTrackForProcessor(const ValueTree &processor) {
        return Track::isType(processor.getParent()) ? processor.getParent() : processor.getParent().getParent().getParent();
    }

    static ValueTree getInputProcessorForTrack(const ValueTree &track) {
        return track.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackInputProcessorName());
    }

    static ValueTree getOutputProcessorForTrack(const ValueTree &track) {
        return track.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }

    static Array<ValueTree> getAllProcessorsForTrack(const ValueTree &track);

    static bool isProcessorLeftToRightFlowing(const ValueTree &processor) {
        return Track::isMaster(getTrackForProcessor(processor)) && !Processor::isTrackIOProcessor(processor);
    }

    int getNumTracks() const { return state.getNumChildren(); }

    int indexOf(const ValueTree &track) const { return state.indexOf(track); }

    ValueTree getTrack(int trackIndex) const { return state.getChild(trackIndex); }

    int getViewIndexForTrack(const ValueTree &track) const { return indexOf(track) - view.getGridViewTrackOffset(); }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return getTrack(trackViewIndex + view.getGridViewTrackOffset());
    }

    ValueTree getMasterTrack() const { return state.getChildWithProperty(TrackIDs::isMasterTrack, true); }

    int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? state.getNumChildren() - 1 : state.getNumChildren();
    }

    static bool doesTrackHaveSelections(const ValueTree &track) {
        return Track::isSelected(track) || trackHasAnySlotSelected(track);
    }

    bool anyTrackSelected() const {
        for (const auto &track : state)
            if (Track::isSelected(track))
                return true;
        return false;
    }

    bool anyTrackHasSelections() const {
        for (const auto &track : state)
            if (doesTrackHaveSelections(track))
                return true;
        return false;
    }

    bool moreThanOneTrackHasSelections() const {
        bool foundOne = false;
        for (const auto &track : state) {
            if (doesTrackHaveSelections(track)) {
                if (foundOne) return true; // found a second one
                else foundOne = true;
            }
        }
        return false;
    }

    ValueTree findTrackWithUuid(const String &uuid) {
        for (const auto &track : state)
            if (Track::getUuid(track) == uuid)
                return track;
        return {};
    }

    static int firstSelectedSlotForTrack(const ValueTree &track) { return getSlotMask(track).getHighestBit(); }

    static bool trackHasAnySlotSelected(const ValueTree &track) { return firstSelectedSlotForTrack(track) != -1; }

    void setTrackColour(ValueTree &track, const Colour &colour) {
        track.setProperty(TrackIDs::colour, colour.toString(), &undoManager);
    }

    static bool isSlotSelected(const ValueTree &track, int slot) { return getSlotMask(track)[slot]; }

    static bool isProcessorSelected(const ValueTree &processor) {
        return processor.hasType(ProcessorIDs::PROCESSOR) && isSlotSelected(getTrackForProcessor(processor), Processor::getSlot(processor));
    }

    ValueTree getFocusedTrack() const { return getTrack(view.getFocusedTrackAndSlot().x); }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree &track = getTrack(trackAndSlot.x);
        return getProcessorAtSlot(track, trackAndSlot.y);
    }

    bool isProcessorFocused(const ValueTree &processor) const { return getFocusedProcessor() == processor; }

    bool isSlotFocused(const ValueTree &track, int slot) const {
        auto trackAndSlot = view.getFocusedTrackAndSlot();
        return indexOf(track) == trackAndSlot.x && slot == trackAndSlot.y;
    }

    ValueTree findFirstTrackWithSelections() {
        for (const auto &track : state)
            if (doesTrackHaveSelections(track))
                return track;
        return {};
    }

    ValueTree findLastTrackWithSelections() const {
        for (int i = getNumTracks() - 1; i >= 0; i--) {
            const auto &track = getTrack(i);
            if (doesTrackHaveSelections(track))
                return track;
        }
        return {};
    }

    static ValueTree findFirstSelectedProcessor(const ValueTree &track) {
        for (const auto &processor : getProcessorLaneForTrack(track))
            if (isSlotSelected(track, Processor::getSlot(processor)))
                return processor;
        return {};
    }

    static ValueTree findLastSelectedProcessor(const ValueTree &track) {
        const auto &lane = getProcessorLaneForTrack(track);
        for (int i = lane.getNumChildren() - 1; i >= 0; i--) {
            const auto &processor = lane.getChild(i);
            if (isSlotSelected(track, Processor::getSlot(processor)))
                return processor;
        }
        return {};
    }

    Array<String> getSelectedSlotsMasks() const {
        Array<String> selectedSlotMasks;
        for (const auto &track : state) {
            const auto &lane = getProcessorLaneForTrack(track);
            selectedSlotMasks.add(lane[ProcessorLaneIDs::selectedSlotsMask]);
        }
        return selectedSlotMasks;
    }

    Array<bool> getTrackSelections() const {
        Array<bool> trackSelections;
        for (const auto &track : state) {
            trackSelections.add(Track::isSelected(track));
        }
        return trackSelections;
    }

    static BigInteger getSlotMask(const ValueTree &track) {
        const auto &lane = getProcessorLaneForTrack(track);

        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(lane[ProcessorLaneIDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    String createFullSelectionBitmask(const ValueTree &track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, getNumSlotsForTrack(track), true);
        return selectedSlotsMask.toString(2);
    }

    static ValueTree getProcessorAtSlot(const ValueTree &track, int slot) {
        const auto &lane = getProcessorLaneForTrack(track);
        return track.isValid() ? lane.getChildWithProperty(ProcessorIDs::processorSlot, slot) : ValueTree();
    }

    static int getInsertIndexForProcessor(const ValueTree &track, const ValueTree &processor, int insertSlot);

    Array<ValueTree> findAllSelectedItems() const;

    static Array<ValueTree> findSelectedProcessorsForTrack(const ValueTree &track) {
        const auto &lane = getProcessorLaneForTrack(track);

        Array<ValueTree> selectedProcessors;
        auto selectedSlotsMask = getSlotMask(track);
        for (const auto &processor : lane)
            if (selectedSlotsMask[Processor::getSlot(processor)])
                selectedProcessors.add(processor);
        return selectedProcessors;
    }

    static ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot);

    void setTrackName(ValueTree track, const String &name) {
        undoManager.beginNewTransaction();
        track.setProperty(TrackIDs::name, name, &undoManager);
    }

    void showWindow(ValueTree processor, PluginWindow::Type type) {
        processor.setProperty(ProcessorIDs::pluginWindowType, int(type), &undoManager);
    }

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const ValueTree &track) {
        for (const auto &processor : getProcessorLaneForTrack(track))
            if (auto existingDescription = pluginManager.getDescriptionForIdentifier(Processor::getId(processor)))
                if (PluginManager::isGeneratorOrInstrument(existingDescription.get()))
                    return true;
        return false;
    }

    juce::Point<int> trackAndSlotWithLeftRightDelta(int delta) const {
        if (view.isGridPaneFocused()) return trackAndSlotWithGridDelta(delta, 0);

        return selectionPaneTrackAndSlotWithLeftRightDelta(delta);
    }

    juce::Point<int> trackAndSlotWithUpDownDelta(int delta) const {
        if (view.isGridPaneFocused())
            return trackAndSlotWithGridDelta(0, delta);
        return selectionPaneTrackAndSlotWithUpDownDelta(delta);
    }

    juce::Point<int> trackAndSlotWithGridDelta(int xDelta, int yDelta) const {
        auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (Track::isSelected(focusedTrack))
            focusedTrackAndSlot.y = -1;

        const auto fromGridPosition = trackAndSlotToGridPosition(focusedTrackAndSlot);
        return gridPositionToTrackAndSlot(fromGridPosition + juce::Point(xDelta, yDelta), Track::isMaster(focusedTrack));
    }

    juce::Point<int> selectionPaneTrackAndSlotWithUpDownDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid()) return INVALID_TRACK_AND_SLOT;

        const ValueTree &focusedProcessor = Tracks::getProcessorAtSlot(focusedTrack, focusedTrackAndSlot.y);
        // down when track is selected deselects the track
        if (delta > 0 && Track::isSelected(focusedTrack)) return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};

        ValueTree siblingProcessorToSelect;
        if (focusedProcessor.isValid()) {
            siblingProcessorToSelect = focusedProcessor.getSibling(delta);
        } else { // no focused processor - selection is on empty slot
            for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < getNumSlotsForTrack(focusedTrack)); slot += delta) {
                siblingProcessorToSelect = Tracks::getProcessorAtSlot(focusedTrack, slot);
                if (siblingProcessorToSelect.isValid())
                    break;
            }
        }
        if (siblingProcessorToSelect.isValid()) return {focusedTrackAndSlot.x, Processor::getSlot(siblingProcessorToSelect)};
        if (delta < 0) return {focusedTrackAndSlot.x, -1};
        return INVALID_TRACK_AND_SLOT;
    }

    juce::Point<int> selectionPaneTrackAndSlotWithLeftRightDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid()) return INVALID_TRACK_AND_SLOT;

        const auto &siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid()) return INVALID_TRACK_AND_SLOT;

        int siblingTrackIndex = indexOf(siblingTrackToSelect);
        const auto &focusedProcessorLane = getProcessorLaneForTrack(focusedTrack);
        if (Track::isSelected(focusedTrack) || focusedProcessorLane.getNumChildren() == 0)
            return {siblingTrackIndex, -1};

        if (focusedTrackAndSlot.y != -1) {
            const auto &processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect, focusedTrackAndSlot.y);
            if (processorToSelect.isValid())
                return {siblingTrackIndex, focusedTrackAndSlot.y};
            return {siblingTrackIndex, -1};
        }

        return INVALID_TRACK_AND_SLOT;
    }

    juce::Point<int> trackAndSlotToGridPosition(const juce::Point<int> trackAndSlot) const {
        if (Track::isMaster(getTrack(trackAndSlot.x)))
            return {trackAndSlot.y + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(), view.getNumProcessorSlots()};
        return trackAndSlot;
    }

    juce::Point<int> gridPositionToTrackAndSlot(juce::Point<int> gridPosition, bool allowUpFromMaster = false) const;

    bool isTrackInView(const ValueTree &track) const {
        if (Track::isMaster(track)) return true;

        auto trackIndex = track.getParent().indexOf(track);
        auto trackViewOffset = view.getGridViewTrackOffset();
        return trackIndex >= trackViewOffset && trackIndex < trackViewOffset + View::NUM_VISIBLE_TRACKS;
    }

    bool isProcessorSlotInView(const ValueTree &track, int slot) const {
        bool inView = slot >= getSlotOffsetForTrack(track) &&
                      slot < getSlotOffsetForTrack(track) + View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (Track::isMaster(track) ? 1 : 0);
        return inView && isTrackInView(track);
    }

    static int getNumVisibleSlotsForTrack(const ValueTree &track) {
        return Track::isMaster(track) ? View::NUM_VISIBLE_MASTER_TRACK_SLOTS : View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS;
    }

    int getSlotOffsetForTrack(const ValueTree &track) const {
        return Track::isMaster(track) ? view.getMasterViewSlotOffset() : view.getGridViewSlotOffset();
    }

    int getNumSlotsForTrack(const ValueTree &track) const {
        return view.getNumProcessorSlots(Track::isMaster(track));
    }

    int getProcessorSlotSize(const ValueTree &track) const {
        return Track::isMaster(track) ? view.getTrackWidth() : view.getProcessorHeight();
    }

    int findSlotAt(juce::Point<int> position, const ValueTree &track) const;

    static constexpr juce::Point<int> INVALID_TRACK_AND_SLOT = {-1, -1};

private:
    View &view;
    PluginManager &pluginManager;
    UndoManager &undoManager;
};
