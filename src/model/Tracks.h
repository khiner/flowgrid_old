#pragma once

#include "model/Track.h"
#include "model/View.h"
#include "view/PluginWindow.h"
#include "Stateful.h"
#include "PluginManager.h"
#include "StatefulList.h"

namespace TracksIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACKS)
#undef ID
}

struct Tracks : public Stateful<Tracks> {
//                private StatefulList<Track> {
    Tracks(View &view, PluginManager &pluginManager, UndoManager &undoManager);

//    ~Tracks() override {
//        freeObjects();
//    }

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }
    void loadFromState(const ValueTree &fromState) override;

//    bool isChildType(const ValueTree &tree) const override { return Track::isType(tree); }

//    void deleteObject(Track *track) override { delete track; }
//    void newObjectAdded(Track *track) override {}
//    void objectRemoved(Track *) override {}
//    void objectOrderChanged() override {}

    int getNumTracks() const { return state.getNumChildren(); }
    int indexOfTrack(const ValueTree &track) const { return state.indexOf(track); }

    ValueTree getTrack(int trackIndex) const { return state.getChild(trackIndex); }
    int getViewIndexForTrack(const ValueTree &track) const { return indexOfTrack(track) - view.getGridViewTrackOffset(); }
    ValueTree getTrackWithViewIndex(int trackViewIndex) const { return getTrack(trackViewIndex + view.getGridViewTrackOffset()); }
    ValueTree getMasterTrack() const { return state.getChildWithProperty(TrackIDs::isMaster, true); }
    int getNumNonMasterTracks() const { return getMasterTrack().isValid() ? state.getNumChildren() - 1 : state.getNumChildren(); }

    bool anyTrackSelected() const {
        for (const auto &track : state)
            if (Track::isSelected(track))
                return true;
        return false;
    }

    bool anyTrackHasSelections() const {
        for (const auto &track : state)
            if (Track::hasSelections(track))
                return true;
        return false;
    }

    bool moreThanOneTrackHasSelections() const {
        bool foundOne = false;
        for (const auto &track : state) {
            if (Track::hasSelections(track)) {
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

    void setTrackColour(ValueTree &track, const Colour &colour) {
        track.setProperty(TrackIDs::colour, colour.toString(), &undoManager);
    }

    ValueTree getFocusedTrack() const { return getTrack(view.getFocusedTrackAndSlot().x); }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree &track = getTrack(trackAndSlot.x);
        return Track::getProcessorAtSlot(track, trackAndSlot.y);
    }

    bool isProcessorFocused(const ValueTree &processor) const { return getFocusedProcessor() == processor; }

    bool isSlotFocused(const ValueTree &track, int slot) const {
        auto trackAndSlot = view.getFocusedTrackAndSlot();
        return indexOfTrack(track) == trackAndSlot.x && slot == trackAndSlot.y;
    }

    ValueTree findFirstTrackWithSelections() {
        for (const auto &track : state)
            if (Track::hasSelections(track))
                return track;
        return {};
    }

    ValueTree findLastTrackWithSelections() const {
        for (int i = getNumTracks() - 1; i >= 0; i--) {
            const auto &track = getTrack(i);
            if (Track::hasSelections(track))
                return track;
        }
        return {};
    }

    Array<BigInteger> getSelectedSlotsMasks() const {
        Array<BigInteger> selectedSlotMasks;
        for (const auto &track : state) {
            const auto &lane = Track::getProcessorLane(track);
            selectedSlotMasks.add(ProcessorLane::getSelectedSlotsMask(lane));
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

    BigInteger createFullSelectionBitmask(const ValueTree &track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, getNumSlotsForTrack(track), true);
        return selectedSlotsMask;
    }

    Array<ValueTree> findAllSelectedItems() const;

    void setTrackName(ValueTree track, const String &name) {
        undoManager.beginNewTransaction();
        track.setProperty(TrackIDs::name, name, &undoManager);
    }

    void showWindow(ValueTree processor, PluginWindow::Type type) {
        processor.setProperty(ProcessorIDs::pluginWindowType, int(type), &undoManager);
    }

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const ValueTree &track) {
        for (const auto &processor : Track::getProcessorLane(track))
            if (auto existingDescription = pluginManager.getDescriptionForIdentifier(Processor::getId(processor)))
                if (PluginManager::isGeneratorOrInstrument(existingDescription.get()))
                    return true;
        return false;
    }

    juce::Point<int> trackAndSlotWithLeftRightDelta(int delta) const {
        return view.isGridPaneFocused() ? trackAndSlotWithGridDelta(delta, 0) : selectionPaneTrackAndSlotWithLeftRightDelta(delta);
    }

    juce::Point<int> trackAndSlotWithUpDownDelta(int delta) const {
        return view.isGridPaneFocused() ? trackAndSlotWithGridDelta(0, delta) : selectionPaneTrackAndSlotWithUpDownDelta(delta);
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

        const ValueTree &focusedProcessor = Track::getProcessorAtSlot(focusedTrack, focusedTrackAndSlot.y);
        // down when track is selected deselects the track
        if (delta > 0 && Track::isSelected(focusedTrack)) return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};

        ValueTree siblingProcessorToSelect;
        if (focusedProcessor.isValid()) {
            siblingProcessorToSelect = focusedProcessor.getSibling(delta);
        } else { // no focused processor - selection is on empty slot
            for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < getNumSlotsForTrack(focusedTrack)); slot += delta) {
                siblingProcessorToSelect = Track::getProcessorAtSlot(focusedTrack, slot);
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

        int siblingTrackIndex = indexOfTrack(siblingTrackToSelect);
        const auto &focusedProcessorLane = Track::getProcessorLane(focusedTrack);
        if (Track::isSelected(focusedTrack) || focusedProcessorLane.getNumChildren() == 0)
            return {siblingTrackIndex, -1};

        if (focusedTrackAndSlot.y != -1) {
            const auto &processorToSelect = Track::findProcessorNearestToSlot(siblingTrackToSelect, focusedTrackAndSlot.y);
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

    static int getNumVisibleSlotsForTrack(const ValueTree &track) { return View::getNumVisibleSlots(Track::isMaster(track)); }
    bool isTrackInView(const ValueTree &track) const { return view.isTrackInView(Track::getIndex(track), Track::isMaster(track)); }
    bool isProcessorSlotInView(const ValueTree &track, int slot) const { return view.isProcessorSlotInView(Track::getIndex(track), Track::isMaster(track), slot); }
    int getSlotOffsetForTrack(const ValueTree &track) const { return view.getSlotOffset(Track::isMaster(track)); }
    int getNumSlotsForTrack(const ValueTree &track) const { return view.getNumProcessorSlots(Track::isMaster(track)); }
    int getProcessorSlotSize(const ValueTree &track) const { return view.getProcessorSlotSize(Track::isMaster(track)); }
    int findSlotAt(juce::Point<int> position, const ValueTree &track) const;

    static constexpr juce::Point<int> INVALID_TRACK_AND_SLOT = {-1, -1};

private:
    View &view;
    PluginManager &pluginManager;
    UndoManager &undoManager;
};
