#pragma once


#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "state/ViewState.h"
#include "PluginManager.h"
#include "Stateful.h"

class TracksState :
        public Stateful,
        private ValueTree::Listener {
public:
    TracksState(ViewState &view, PluginManager &pluginManager, UndoManager &undoManager)
            : view(view), pluginManager(pluginManager), undoManager(undoManager) {
        tracks = ValueTree(IDs::TRACKS);
        tracks.setProperty(IDs::name, "Tracks", nullptr);
    }

    void loadFromState(const ValueTree &state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);

        // Re-save all non-string value types,
        // since type information is not saved in XML
        // Also, re-set some vars just to trigger the event (like selected slot mask)
        for (auto track : tracks) {
            resetVarToBool(track, IDs::isMasterTrack, this);
            track.sendPropertyChangeMessage(IDs::selectedSlotsMask);
            resetVarToBool(track, IDs::selected, this);
            for (auto processor : track) {
                if (processor.hasType(IDs::PROCESSOR)) {
                    resetVarToInt(processor, IDs::processorSlot, this);
                    resetVarToInt(processor, IDs::nodeId, this);
                    resetVarToInt(processor, IDs::processorInitialized, this);
                    resetVarToBool(processor, IDs::bypassed, this);
                    resetVarToBool(processor, IDs::acceptsMidi, this);
                    resetVarToBool(processor, IDs::producesMidi, this);
                    resetVarToBool(processor, IDs::allowDefaultConnections, this);
                }
            }
        }
    }

    ValueTree &getState() override { return tracks; }

    int getNumTracks() const { return tracks.getNumChildren(); }

    int indexOf(const ValueTree &track) const { return tracks.indexOf(track); }

    ValueTree getTrack(int trackIndex) const { return tracks.getChild(trackIndex); }

    int getViewIndexForTrack(const ValueTree &track) const { return indexOf(track) - view.getGridViewTrackOffset(); }

    int getMixerChannelSlotForTrack(const ValueTree &track) const { return getNumAvailableSlotsForTrack(track) - 1; }

    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return isMasterTrack(track) ? view.getNumMasterProcessorSlots() : view.getNumTrackProcessorSlots();
    }

    int getSlotOffsetForTrack(const ValueTree &track) const {
        return isMasterTrack(track) ? view.getMasterViewSlotOffset() : view.getGridViewSlotOffset();
    }

    bool isProcessorSlotInView(const ValueTree &track, int correctedSlot) {
        bool inView = correctedSlot >= getSlotOffsetForTrack(track) &&
                      correctedSlot < getSlotOffsetForTrack(track) + ViewState::NUM_VISIBLE_TRACKS;
        if (isMasterTrack(track)) {
            return inView && view.getGridViewSlotOffset() + ViewState::NUM_VISIBLE_TRACKS > view.getNumTrackProcessorSlots();
        } else {
            auto trackIndex = track.getParent().indexOf(track);
            auto trackViewOffset = view.getGridViewTrackOffset();
            return inView && trackIndex >= trackViewOffset && trackIndex < trackViewOffset + ViewState::NUM_VISIBLE_TRACKS;
        }
    }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return getTrack(trackViewIndex + view.getGridViewTrackOffset());
    }

    ValueTree getMasterTrack() const { return tracks.getChildWithProperty(IDs::isMasterTrack, true); }

    static bool isMasterTrack(const ValueTree &track) { return track[IDs::isMasterTrack]; }

    ValueTree getMixerChannelProcessorForTrack(const ValueTree &track) const {
        return getProcessorAtSlot(track, getMixerChannelSlotForTrack(track));
    }

    ValueTree getMixerChannelProcessorForFocusedTrack() const {
        return getMixerChannelProcessorForTrack(getFocusedTrack());
    }

    bool focusedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForFocusedTrack().isValid();
    }

    int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? tracks.getNumChildren() - 1 : tracks.getNumChildren();
    }

    static bool isMixerChannelProcessor(const ValueTree &processor) {
        return processor[IDs::name] == MixerChannelProcessor::name();
    }

    static bool doesTrackHaveSelections(const ValueTree &track) {
        return track[IDs::selected] || trackHasAnySlotSelected(track);
    }

    bool anyTrackSelected() const {
        for (const auto &track : tracks)
            if (track[IDs::selected])
                return true;
        return false;
    }

    bool anyTrackHasSelections() const {
        for (const auto &track : tracks)
            if (doesTrackHaveSelections(track))
                return true;
        return false;
    }

    bool moreThanOneTrackHasSelections() const {
        bool foundOne = false;
        for (const auto &track : tracks) {
            if (doesTrackHaveSelections(track)) {
                if (foundOne) return true; // found a second one
                else foundOne = true;
            }
        }
        return false;
    }

    ValueTree findTrackWithUuid(const String &uuid) {
        for (const auto &track : tracks)
            if (track[IDs::uuid] == uuid)
                return track;
        return {};
    }

    static int firstSelectedSlotForTrack(const ValueTree &track) {
        return getSlotMask(track).getHighestBit();
    }

    static bool trackHasAnySlotSelected(const ValueTree &track) {
        return firstSelectedSlotForTrack(track) != -1;
    }

    static Colour getTrackColour(const ValueTree &track) {
        return Colour::fromString(track[IDs::colour].toString());
    }

    void setTrackColour(ValueTree track, const Colour &colour) {
        track.setProperty(IDs::colour, colour.toString(), &undoManager);
    }

    static bool isSlotSelected(const ValueTree &track, int slot) {
        return getSlotMask(track)[slot];
    }

    static bool isProcessorSelected(const ValueTree &processor) {
        return processor.hasType(IDs::PROCESSOR) &&
               isSlotSelected(processor.getParent(), processor[IDs::processorSlot]);
    }

    ValueTree getFocusedTrack() const {
        auto trackAndSlot = view.getFocusedTrackAndSlot();
        return getTrack(trackAndSlot.x);
    }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree &track = getTrack(trackAndSlot.x);
        return getProcessorAtSlot(track, trackAndSlot.y);
    }

    bool isProcessorFocused(const ValueTree &processor) const {
        return getFocusedProcessor() == processor;
    }

    bool isSlotFocused(const ValueTree &track, int slot) const {
        auto trackAndSlot = view.getFocusedTrackAndSlot();
        return indexOf(track) == trackAndSlot.x && slot == trackAndSlot.y;
    }

    ValueTree findFirstTrackWithSelectedProcessors() {
        for (const auto &track : tracks)
            if (findFirstSelectedProcessor(track).isValid())
                return track;
        return {};
    }

    ValueTree findLastTrackWithSelectedProcessors() {
        for (int i = getNumTracks() - 1; i >= 0; i--) {
            const auto &track = getTrack(i);
            if (findFirstSelectedProcessor(track).isValid())
                return track;
        }
        return {};
    }

    static ValueTree findFirstSelectedProcessor(const ValueTree &track) {
        for (const auto &processor : track)
            if (isSlotSelected(track, processor[IDs::processorSlot]))
                return processor;
        return {};
    }

    static ValueTree findLastSelectedProcessor(const ValueTree &track) {
        for (int i = track.getNumChildren() - 1; i >= 0; i--) {
            const auto &processor = track.getChild(i);
            if (isSlotSelected(track, processor[IDs::processorSlot]))
                return processor;
        }
        return {};
    }

    Array<String> getSelectedSlotsMasks() const {
        Array<String> selectedSlotMasks;
        for (const auto &track : tracks) {
            selectedSlotMasks.add(track[IDs::selectedSlotsMask]);
        }
        return selectedSlotMasks;
    }

    Array<bool> getTrackSelections() const {
        Array<bool> trackSelections;
        for (const auto &track : tracks) {
            trackSelections.add(track[IDs::selected]);
        }
        return trackSelections;
    }

    static BigInteger getSlotMask(const ValueTree &track) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    String createFullSelectionBitmask(const ValueTree &track) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, getNumAvailableSlotsForTrack(track), true);
        return selectedSlotsMask.toString(2);
    }

    static ValueTree getProcessorAtSlot(const ValueTree &track, int slot) {
        return track.isValid() ? track.getChildWithProperty(IDs::processorSlot, slot) : ValueTree();
    }

    int getInsertIndexForProcessor(const ValueTree &track, const ValueTree &processor, int insertSlot) {
        bool sameTrack = track == processor.getParent();
        auto handleSameTrack = [sameTrack](int index) -> int { return sameTrack ? std::max(0, index - 1) : index; };
        for (const auto &otherProcessor : track) {
            int otherSlot = otherProcessor[IDs::processorSlot];
            if (otherSlot >= insertSlot && otherProcessor != processor) {
                int otherIndex = track.indexOf(otherProcessor);
                if (sameTrack && track.indexOf(processor) < otherIndex)
                    return handleSameTrack(otherIndex);
                else
                    return otherIndex;
            }
        }
        const ValueTree &mixerChannel = getMixerChannelProcessorForTrack(track);
        return handleSameTrack(mixerChannel.isValid() && processor != mixerChannel ? track.getNumChildren() - 1 : track.getNumChildren());
    }

    Array<ValueTree> findAllSelectedItems() const {
        Array<ValueTree> selectedItems;
        for (const auto &track : tracks) {
            if (track[IDs::selected])
                selectedItems.add(track);
            else
                selectedItems.addArray(findSelectedProcessorsForTrack(track));
        }
        return selectedItems;
    }

    Array<ValueTree> findSelectedNonMasterTracks() const {
        Array<ValueTree> selectedTracks;
        for (const auto &track : tracks)
            if (track[IDs::selected] && !isMasterTrack(track))
                selectedTracks.add(track);
        return selectedTracks;
    }

    Array<ValueTree> findNonSelectedTracks() const {
        Array<ValueTree> nonSelectedTracks;
        for (const auto &track : tracks)
            if (!track[IDs::selected])
                nonSelectedTracks.add(track);
        return nonSelectedTracks;
    }

    static Array<ValueTree> findSelectedProcessorsForTrack(const ValueTree &track) {
        Array<ValueTree> selectedProcessors;
        auto selectedSlotsMask = getSlotMask(track);
        for (const auto &processor : track)
            if (selectedSlotsMask[int(processor[IDs::processorSlot])])
                selectedProcessors.add(processor);
        return selectedProcessors;
    }

    ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot) const {
        auto nearestSlot = INT_MAX;
        ValueTree nearestProcessor;
        for (const auto &processor : track) {
            int otherSlot = processor[IDs::processorSlot];
            if (otherSlot == slot)
                return processor;
            if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
                nearestSlot = otherSlot;
                nearestProcessor = processor;
            }
            if (otherSlot > slot)
                break; // processors are ordered by slot.
        }
        return nearestProcessor;
    }

    void setTrackName(ValueTree track, const String &name) {
        undoManager.beginNewTransaction();
        track.setProperty(IDs::name, name, &undoManager);
    }

    juce::Point<int> trackAndSlotWithLeftRightDelta(int delta) const {
        if (view.isGridPaneFocused())
            return trackAndSlotWithGridDelta(delta, 0);
        else
            return selectionPaneTrackAndSlotWithLeftRightDelta(delta);
    }

    juce::Point<int> trackAndSlotWithUpDownDelta(int delta) const {
        if (view.isGridPaneFocused())
            return trackAndSlotWithGridDelta(0, delta);
        else
            return selectionPaneTrackAndSlotWithUpDownDelta(delta);
    }

    juce::Point<int> trackAndSlotWithGridDelta(int xDelta, int yDelta) const {
        auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (focusedTrack[IDs::selected])
            focusedTrackAndSlot.y = -1;

        const auto fromGridPosition = trackAndSlotToGridPosition(focusedTrackAndSlot);
        return gridPositionToTrackAndSlot(fromGridPosition + juce::Point(xDelta, yDelta), TracksState::isMasterTrack(focusedTrack));
    }

    juce::Point<int> selectionPaneTrackAndSlotWithUpDownDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return INVALID_TRACK_AND_SLOT;

        const ValueTree &focusedProcessor = TracksState::getProcessorAtSlot(focusedTrack, focusedTrackAndSlot.y);
        if (delta > 0 && focusedTrack[IDs::selected])
            // down when track is selected deselects the track
            return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};
        ValueTree siblingProcessorToSelect;
        if (focusedProcessor.isValid()) {
            siblingProcessorToSelect = focusedProcessor.getSibling(delta);
        } else { // no focused processor - selection is on empty slot
            for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < getNumAvailableSlotsForTrack(focusedTrack)); slot += delta) {
                siblingProcessorToSelect = TracksState::getProcessorAtSlot(focusedTrack, slot);
                if (siblingProcessorToSelect.isValid())
                    break;
            }
        }
        if (siblingProcessorToSelect.isValid())
            return {focusedTrackAndSlot.x, siblingProcessorToSelect[IDs::processorSlot]};
        else if (delta < 0)
            return {focusedTrackAndSlot.x, -1};
        else
            return INVALID_TRACK_AND_SLOT;
    }

    juce::Point<int> selectionPaneTrackAndSlotWithLeftRightDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree &focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return INVALID_TRACK_AND_SLOT;

        const auto &siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return INVALID_TRACK_AND_SLOT;

        int siblingTrackIndex = indexOf(siblingTrackToSelect);

        if (focusedTrack[IDs::selected] || focusedTrack.getNumChildren() == 0)
            return {siblingTrackIndex, -1};

        if (focusedTrackAndSlot.y != -1) {
            const auto &processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect, focusedTrackAndSlot.y);
            if (processorToSelect.isValid())
                return {siblingTrackIndex, focusedTrackAndSlot.y};
            else
                return {siblingTrackIndex, -1};
        }

        return INVALID_TRACK_AND_SLOT;
    }

    juce::Point<int> trackAndSlotToGridPosition(const juce::Point<int> trackAndSlot) const {
        if (TracksState::isMasterTrack(getTrack(trackAndSlot.x)))
            return {trackAndSlot.y + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(), view.getNumTrackProcessorSlots()};
        else
            return trackAndSlot;
    }

    juce::Point<int> gridPositionToTrackAndSlot(const juce::Point<int> gridPosition, bool allowUpFromMaster = false) const {
        if (gridPosition.y > view.getNumTrackProcessorSlots())
            return INVALID_TRACK_AND_SLOT;

        int trackIndex, slot;
        if (gridPosition.y == view.getNumTrackProcessorSlots()) {
            trackIndex = indexOf(getMasterTrack());
            slot = gridPosition.x + view.getMasterViewSlotOffset() - view.getGridViewTrackOffset();
        } else {
            trackIndex = gridPosition.x;
            if (trackIndex < 0 || trackIndex >= getNumNonMasterTracks()) {
                if (allowUpFromMaster)
                    // This is annoyingly tied to arrow selection.
                    // Allow navigating UP from master track, but not LEFT or RIGHT into no track
                    trackIndex = std::clamp(trackIndex, 0, getNumNonMasterTracks() - 1);
                else
                    return INVALID_TRACK_AND_SLOT;
            }
            slot = gridPosition.y + view.getGridViewSlotOffset();
        }

        if (trackIndex < 0 || slot < -1 || slot >= getNumAvailableSlotsForTrack(getTrack(trackIndex)))
            return INVALID_TRACK_AND_SLOT;

        return {trackIndex, slot};
    }

    static constexpr juce::Point<int> INVALID_TRACK_AND_SLOT = {-1, -1};

private:
    ValueTree tracks;
    ViewState &view;
    PluginManager &pluginManager;
    UndoManager &undoManager;
};
