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

struct Tracks : public Stateful<Tracks>,
                public StatefulList<Track> {
    struct Listener {
        virtual void trackAdded(Track *track) = 0;
        virtual void trackRemoved(Track *track, int oldIndex) = 0;
        virtual void trackOrderChanged() = 0;
    };


    Tracks(View &view, PluginManager &pluginManager, UndoManager &undoManager);

    ~Tracks() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }
    void loadFromState(const ValueTree &fromState) override;

    bool isChildType(const ValueTree &tree) const override { return Track::isType(tree); }

    void addTracksListener(Listener *listener) {
        listeners.add(listener);
    }
    void removeTracksListener(Listener *listener) {
        listeners.remove(listener);
    }

    Track *createNewObject(const ValueTree &tree) override { return new Track(tree); }
    void deleteObject(Track *track) override { delete track; }
    void newObjectAdded(Track *track) override { listeners.call([track](Listener &l) { l.trackAdded(track); }); }
    void objectRemoved(Track *track, int oldIndex) override { listeners.call([track, oldIndex](Listener &l) { l.trackRemoved(track, oldIndex); }); }
    void objectOrderChanged() override { listeners.call([](Listener &l) { l.trackOrderChanged(); }); }

    int indexOfTrack(const ValueTree &track) const { return parent.indexOf(track); }

    ValueTree getTrackState(int trackIndex) const { return parent.getChild(trackIndex); }
    int getViewIndexForTrack(const ValueTree &track) const { return indexOfTrack(track) - view.getGridViewTrackOffset(); }
    Track *getTrackWithViewIndex(int trackViewIndex) const { return getChild(trackViewIndex + view.getGridViewTrackOffset()); }
    Track *getMasterTrack() const {
        for (auto *track : children)
            if (track->isMaster())
                return track;
        return {};
    }
    ValueTree getMasterTrackState() const {
        for (auto *track : children)
            if (track->isMaster())
                return track->getState();
        return {};
    }
    int getNumNonMasterTracks() const { return getMasterTrackState().isValid() ? size() - 1 : size(); }

    bool anyTrackSelected() const {
        for (const auto *track : children)
            if (track->isSelected())
                return true;
        return false;
    }

    bool anyTrackHasSelections() const {
        for (const auto *track : children)
            if (track->hasSelections())
                return true;
        return false;
    }

    bool moreThanOneTrackHasSelections() const {
        bool foundOne = false;
        for (const auto *track : children) {
            if (track->hasSelections()) {
                if (foundOne) return true; // found a second one
                else foundOne = true;
            }
        }
        return false;
    }

    ValueTree findTrackWithUuid(const String &uuid) {
        for (auto *track : children)
            if (track->getUuid() == uuid)
                return track->getState();
        return {};
    }

    void setTrackColour(ValueTree &track, const Colour &colour) {
        track.setProperty(TrackIDs::colour, colour.toString(), &undoManager);
    }

    static ValueTree getTrackStateForProcessor(const ValueTree &processor) {
        return Track::isType(processor.getParent()) ? processor.getParent() : processor.getParent().getParent().getParent();
    }
    Track *getTrackForProcessor(const ValueTree &processor) {
        return getChildForState(getTrackStateForProcessor(processor));
    }
    int getTrackIndexForProcessor(const ValueTree &processor) {
        auto *track = getTrackForProcessor(processor);
        return track == nullptr ? -1 : children.indexOf(track);
    }
    ValueTree getFocusedTrackState() const { return getTrackState(view.getFocusedTrackAndSlot().x); }
    Track *getFocusedTrack() const { return getChild(view.getFocusedTrackAndSlot().x); }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const Track *track = getChild(trackAndSlot.x);
        if (track == nullptr) return {};

        return track->getProcessorAtSlot(trackAndSlot.y);
    }

    bool isProcessorFocused(const ValueTree &processor) const { return getFocusedProcessor() == processor; }

    bool isSlotFocused(const ValueTree &track, int slot) const {
        auto trackAndSlot = view.getFocusedTrackAndSlot();
        return indexOfTrack(track) == trackAndSlot.x && slot == trackAndSlot.y;
    }

    ValueTree findFirstTrackWithSelections() {
        for (auto *track : children)
            if (track->hasSelections())
                return track->getState();
        return {};
    }

    ValueTree findLastTrackWithSelections() const {
        for (int i = children.size() - 1; i >= 0; i--) {
            auto *track = children.getUnchecked(i);
            if (track->hasSelections())
                return track->getState();
        }
        return {};
    }

    Array<BigInteger> getSelectedSlotsMasks() const {
        Array<BigInteger> selectedSlotMasks;
        for (const auto *track : children) {
            const auto &lane = track->getProcessorLane();
            selectedSlotMasks.add(ProcessorLane::getSelectedSlotsMask(lane));
        }
        return selectedSlotMasks;
    }

    Array<bool> getTrackSelections() const {
        Array<bool> trackSelections;
        for (const auto *track : children) {
            trackSelections.add(track->isSelected());
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

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const Track *track) {
        for (const auto &processor : track->getProcessorLane())
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
        const auto &focusedTrack = getTrackState(focusedTrackAndSlot.x);
        if (Track::isSelected(focusedTrack))
            focusedTrackAndSlot.y = -1;

        const auto fromGridPosition = trackAndSlotToGridPosition(focusedTrackAndSlot);
        return gridPositionToTrackAndSlot(fromGridPosition + juce::Point(xDelta, yDelta), Track::isMaster(focusedTrack));
    }

    juce::Point<int> selectionPaneTrackAndSlotWithUpDownDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrackState(focusedTrackAndSlot.x);
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
        const auto &focusedTrack = getTrackState(focusedTrackAndSlot.x);
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
        if (Track::isMaster(getTrackState(trackAndSlot.x)))
            return {trackAndSlot.y + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(), view.getNumProcessorSlots()};
        return trackAndSlot;
    }

    juce::Point<int> gridPositionToTrackAndSlot(juce::Point<int> gridPosition, bool allowUpFromMaster = false) const;

    static int getNumVisibleSlotsForTrack(const ValueTree &track) { return View::getNumVisibleSlots(Track::isMaster(track)); }
    bool isTrackInView(const ValueTree &track) const { return view.isTrackInView(Track::getIndex(track), Track::isMaster(track)); }
    bool isProcessorSlotInView(const Track *track, int slot) const { return view.isProcessorSlotInView(track->getIndex(), track->isMaster(), slot); }
    int getSlotOffsetForTrack(const ValueTree &track) const { return view.getSlotOffset(Track::isMaster(track)); }
    int getNumSlotsForTrack(const ValueTree &track) const { return view.getNumProcessorSlots(Track::isMaster(track)); }
    int getProcessorSlotSize(const ValueTree &track) const { return view.getProcessorSlotSize(Track::isMaster(track)); }
    int findSlotAt(juce::Point<int> position, const ValueTree &track) const;

    static constexpr juce::Point<int> INVALID_TRACK_AND_SLOT = {-1, -1};

private:
    View &view;
    PluginManager &pluginManager;
    UndoManager &undoManager;

    ListenerList<Listener> listeners;
};
