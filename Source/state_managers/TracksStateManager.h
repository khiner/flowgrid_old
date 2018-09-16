#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "ValueTreeItems.h"
#include "state_managers/ViewStateManager.h"
#include "processors/ProcessorManager.h"

class TracksStateManager {
public:
    struct TrackAndSlot {
        TrackAndSlot() : slot(0) {};
        TrackAndSlot(ValueTree track, const ValueTree& processor) : track(std::move(track)), slot(processor[IDs::processorSlot]) {}
        TrackAndSlot(ValueTree track, const int slot) : track(std::move(track)), slot(slot) {}
        TrackAndSlot(ValueTree track) : track(std::move(track)), slot(-1) {}

        bool isValid() const { return track.isValid(); }

        void select(TracksStateManager& tracksManager) {
            if (!track.isValid())
                return;

            const auto& selectedTrack = tracksManager.getSelectedTrack();
            if (slot != -1)
                tracksManager.selectProcessorSlot(track, slot);
            if (slot == -1 || (selectedTrack != track && selectedTrack[IDs::selected]))
                track.setProperty(IDs::selected, true, nullptr);
        }

        ValueTree track;
        const int slot;
    };

    Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (trackAndSlot.track.hasProperty(IDs::isMasterTrack))
            return {viewManager.getGridViewTrackOffset() + trackAndSlot.slot - viewManager.getMasterViewSlotOffset(), viewManager.getNumTrackProcessorSlots()};
        else
            return {indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    TracksStateManager(ViewStateManager& viewManager, StatefulAudioProcessorContainer& audioProcessorContainer,
                 ProjectChangeBroadcaster& projectChangeBroadcaster, ProcessorManager& processorManager,
                 UndoManager& undoManager)
            : viewManager(viewManager), audioProcessorContainer(audioProcessorContainer),
              projectChangeBroadcaster(projectChangeBroadcaster), processorManager(processorManager),
              undoManager(undoManager) {
        tracks = ValueTree(IDs::TRACKS);
        tracks.setProperty(IDs::name, "Tracks", nullptr);
    }

    ValueTree& getState() { return tracks; }
    int getNumTracks() const { return tracks.getNumChildren(); }
    int indexOf(const ValueTree& track) const { return tracks.indexOf(track); }

    int getViewIndexForTrack(const ValueTree& track) const {
        return indexOf(track) - viewManager.getGridViewTrackOffset();
    }

    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return viewManager.getNumAvailableSlotsForTrack(track);
    }

    int getSlotOffsetForTrack(const ValueTree& track) const {
        return viewManager.getSlotOffsetForTrack(track);
    }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return getTrack(trackViewIndex + viewManager.getGridViewTrackOffset());
    }

    ValueTree getTrack(int trackIndex) const { return tracks.getChild(trackIndex); }
    ValueTree getMasterTrack() const { return tracks.getChildWithProperty(IDs::isMasterTrack, true); }

    int getMixerChannelSlotForTrack(const ValueTree& track) const {
        return viewManager.getMixerChannelSlotForTrack(track);
    }

    const ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) const {
        return track.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
    }

    const ValueTree getMixerChannelProcessorForSelectedTrack() const {
        return getMixerChannelProcessorForTrack(getSelectedTrack());
    }

    bool isMixerChannelProcessor(const ValueTree& processor) const {
        return processor[IDs::name] == MixerChannelProcessor::name();
    }

    const bool selectedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForSelectedTrack().isValid();
    }

    inline int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? tracks.getNumChildren() - 1 : tracks.getNumChildren();
    }

    inline ValueTree getSelectedTrack() const {
        for (const auto& track : tracks) {
            if (isTrackSelected(track))
                return track;
        }
        return {};
    }

    inline bool isTrackSelected(const ValueTree& track) const {
        if (track[IDs::selected])
            return true;
        return trackHasAnySlotSelected(track);
    }

    ValueTree findTrackWithUuid(const String& uuid) {
        for (const auto& track : tracks) {
            if (track[IDs::uuid] == uuid)
                return track;
        }
        return {};
    }

    inline int findSelectedSlotForTrack(const ValueTree& track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask.getHighestBit();
    }

    inline bool trackHasAnySlotSelected(const ValueTree &track) const {
        return findSelectedSlotForTrack(track) != -1;
    }

    inline const Colour getTrackColour(const ValueTree& track) const {
        return Colour::fromString(track[IDs::colour].toString());
    }

    Array<ValueTree> findAllSelectedItems() const {
        Array<ValueTree> items;
        for (auto track : tracks) {
            if (track[IDs::selected]) {
                items.add(track);
            } else {
                for (auto processor : track) {
                    if (isProcessorSelected(processor)) {
                        items.add(processor);
                    }
                }
            }
        }
        return items;
    }

    inline ValueTree findSelectedProcessorForTrack(const ValueTree &track) const {
        for (const auto& processor : track) {
            if (isProcessorSelected(processor))
                return processor;
        }
        return {};
    }

    inline ValueTree getSelectedProcessor() const {
        for (const auto& track : tracks) {
            const auto& selectedProcessor = findSelectedProcessorForTrack(track);
            if (selectedProcessor.isValid())
                return selectedProcessor;
        }
        return {};
    }

    inline bool isSlotSelected(const ValueTree& track, int slot) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask[slot];
    }

    inline bool isProcessorSelected(const ValueTree& processor) const {
        return processor.hasType(IDs::PROCESSOR) &&
               isSlotSelected(processor.getParent(), processor[IDs::processorSlot]);
    }

    bool isProcessorSlotInView(const ValueTree& track, int correctedSlot) {
        bool inView = correctedSlot >= viewManager.getSlotOffsetForTrack(track) &&
                      correctedSlot < viewManager.getSlotOffsetForTrack(track) + NUM_VISIBLE_TRACKS;
        if (track.hasProperty(IDs::isMasterTrack))
            inView &= viewManager.getGridViewSlotOffset() + NUM_VISIBLE_TRACKS > viewManager.getNumTrackProcessorSlots();
        else {
            auto trackIndex = indexOf(track);
            auto trackViewOffset = viewManager.getGridViewTrackOffset();
            inView &= trackIndex >= trackViewOffset && trackIndex < trackViewOffset + NUM_VISIBLE_TRACKS;
        }
        return inView;
    }

    void deselectAllTracksExcept(const ValueTree& except) {
        for (auto track : tracks) {
            if (track != except) {
                if (track[IDs::selected])
                    track.setProperty(IDs::selected, false, nullptr);
                else
                    track.sendPropertyChangeMessage(IDs::selected);
            }
        }
    }

    void addProcessorRow() {
        viewManager.addProcessorRow(&undoManager);
        for (const auto& track : tracks) {
            if (track.hasProperty(IDs::isMasterTrack))
                continue;
            auto mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
            if (mixerChannelProcessor.isValid()) {
                setProcessorSlot(track, mixerChannelProcessor, getMixerChannelSlotForTrack(track), &undoManager);
            }
        }
    }

    void addMasterProcessorSlot() {
        viewManager.addMasterProcessorSlot(&undoManager);
        const auto& masterTrack = getMasterTrack();
        auto mixerChannelProcessor = getMixerChannelProcessorForTrack(masterTrack);
        if (mixerChannelProcessor.isValid()) {
            setProcessorSlot(masterTrack, mixerChannelProcessor, getMixerChannelSlotForTrack(masterTrack), &undoManager);
        }
    }

    void selectProcessor(const ValueTree& processor, ValueTree::Listener* excludingListener=nullptr) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], excludingListener);
    }

    void setTrackSelected(ValueTree& track, bool selected, bool deselectOthers=true,
                          ValueTree::Listener* excludingListener=nullptr) {
        track.setProperty(IDs::selected, selected, nullptr);
        if (selected && deselectOthers) {
            deselectAllTracksExcept(track);
            if (!trackHasAnySlotSelected(track)) {
                auto slotToSelect = track.getNumChildren() > 0 ? int(track.getChild(0)[IDs::processorSlot]) : 0;
                selectProcessorSlot(track, slotToSelect, true, excludingListener);
            }
        } else if (!selected) {
            track.removeProperty(IDs::selectedSlotsMask, nullptr);
        }
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true,
                             ValueTree::Listener* excludingListener=nullptr) {
        if (initialSelectTrackAndSlot != nullptr) {
            const auto initialGridPosition = trackAndSlotToGridPosition(*initialSelectTrackAndSlot);
            const auto gridPosition = trackAndSlotToGridPosition({track, slot});
            Rectangle<int> selectionRectangle(initialGridPosition, gridPosition);
            selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

            for (const auto& otherTrack : tracks) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition),
                                             false, excludingListener);
                }
            }
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers, excludingListener);
        }
    }

    void setProcessorSlotSelected(ValueTree track, int slot, bool selected, bool deselectOthers=true,
                                  ValueTree::Listener* excludingListener=nullptr) {
        if (deselectOthers) {
            for (auto otherTrack : tracks) {
                if (otherTrack != track) {
                    otherTrack.setProperty(IDs::selected, false, nullptr);
                    otherTrack.removeProperty(IDs::selectedSlotsMask, nullptr);
                }
            }
        }
        BigInteger selectedSlotsMask;
        const String &currentSelectedSlotsMask = track.getProperty(IDs::selectedSlotsMask).toString();
        selectedSlotsMask.parseString(currentSelectedSlotsMask, 2);
        if (deselectOthers)
            selectedSlotsMask.clear();
        selectedSlotsMask.setBit(slot, selected);
        const auto &bitmaskString = selectedSlotsMask.toString(2);
        if (excludingListener == nullptr) {
            if (bitmaskString != currentSelectedSlotsMask)
                track.setProperty(IDs::selectedSlotsMask, bitmaskString, nullptr);
            else
                track.sendPropertyChangeMessage(IDs::selectedSlotsMask);
        } else {
            track.setPropertyExcludingListener(excludingListener, IDs::selectedSlotsMask, bitmaskString, nullptr);
        }
    }

    void setProcessorSlot(const ValueTree& track, ValueTree& processor, int newSlot, UndoManager* undoManager) {
        if (!processor.isValid())
            return;

        if (newSlot >= getMixerChannelSlotForTrack(track) && !isMixerChannelProcessor(processor)) {
            if (track.hasProperty(IDs::isMasterTrack)) {
                addMasterProcessorSlot();
            } else {
                addProcessorRow();
            }
            newSlot = getMixerChannelSlotForTrack(track) - 1;
        }
        processor.setProperty(IDs::processorSlot, newSlot, undoManager);
    }


    ValueTree createAndAddMasterTrack(UndoManager* undoManager, bool addMixer=true) {
        if (getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        tracks.addChild(masterTrack, -1, undoManager);

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, undoManager, -1);

        return masterTrack;
    }

    ValueTree createAndAddTrack(UndoManager* undoManager, bool addMixer=true, ValueTree nextToTrack={}, bool forceImmediatelyToRight=false) {
        int numTracks = getNumNonMasterTracks();
        const auto& selectedTrack = getSelectedTrack();

        if (!nextToTrack.isValid()) {
            if (selectedTrack.isValid()) {
                // If a track is selected, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                nextToTrack = selectedTrack;

                if (addMixer && !forceImmediatelyToRight) {
                    while (nextToTrack.isValid() && !getMixerChannelProcessorForTrack(nextToTrack).isValid())
                        nextToTrack = nextToTrack.getSibling(1);
                }
            }
        }
        if (nextToTrack == getMasterTrack())
            nextToTrack = {};

        ValueTree track(IDs::TRACK);
        track.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        track.setProperty(IDs::name, (nextToTrack.isValid() && !addMixer) ? makeTrackNameUnique(nextToTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
        track.setProperty(IDs::colour, (nextToTrack.isValid() && !addMixer) ? nextToTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        tracks.addChild(track, nextToTrack.isValid() ? nextToTrack.getParent().indexOf(nextToTrack) + (addMixer || forceImmediatelyToRight ? 1 : 0): numTracks, undoManager);

        track.setProperty(IDs::selected, true, undoManager);

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), track, undoManager, -1);

        return track;
    }

    ValueTree createAndAddProcessor(const PluginDescription& description, UndoManager* undoManager) {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid())
            return createAndAddProcessor(description, selectedTrack, undoManager, -1);
        else
            return ValueTree();
    }

    ValueTree createAndAddProcessor(const PluginDescription &description, ValueTree track, UndoManager *undoManager, int slot = -1) {
        if (description.name == MixerChannelProcessor::name() && getMixerChannelProcessorForTrack(track).isValid())
            return ValueTree(); // only one mixer channel per track

        if (processorManager.isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            return createAndAddProcessor(description, createAndAddTrack(undoManager, false, track), undoManager, slot);
        }

        ValueTree processor(IDs::PROCESSOR);
        processor.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processor.setProperty(IDs::name, description.name, nullptr);
        processor.setProperty(IDs::allowDefaultConnections, true, nullptr);

        int insertIndex;
        if (isMixerChannelProcessor(processor)) {
            insertIndex = -1;
            slot = getMixerChannelSlotForTrack(track);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                insertIndex = 0;
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
                insertIndex = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren();
                slot = insertIndex <= 0 ? 0 : int(track.getChild(insertIndex - 1)[IDs::processorSlot]) + 1;
            }
        } else {
            setProcessorSlot(track, processor, slot, nullptr);
            insertIndex = getParentIndexForProcessor(track, processor, nullptr);
        }
        setProcessorSlot(track, processor, slot, nullptr);
        addProcessorToTrack(track, processor, insertIndex, undoManager);

        return processor;
    }

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        track.addChild(processor, insertIndex, undoManager);
        makeSlotsValid(track, undoManager);
        projectChangeBroadcaster.sendProcessorCreatedMessage(processor);
    }

    void makeSlotsValid(const ValueTree& parent, UndoManager* undoManager) {
        std::vector<int> slots;
        for (const ValueTree& child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                slots.push_back(int(child[IDs::processorSlot]));
            }
        }
        sort(slots.begin(), slots.end());
        for (int i = 1; i < slots.size(); i++) {
            while (slots[i] <= slots[i - 1]) {
                slots[i] += 1;
            }
        }

        auto iterator = slots.begin();
        for (ValueTree child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                int newSlot = *(iterator++);
                setProcessorSlot(parent, child, newSlot, undoManager);
            }
        }
    }

    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState, UndoManager* undoManager) {
        auto slot = int(processorState[IDs::processorSlot]);
        for (ValueTree otherProcessorState : parent) {
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR)) {
                auto otherSlot = int(otherProcessorState[IDs::processorSlot]);
                if (otherSlot == slot) {
                    if (otherProcessorState.getParent() == processorState.getParent()) {
                        // moving within same parent - need to resolve the "tie" in a way that guarantees the child order changes.
                        int currentIndex = parent.indexOf(processorState);
                        int currentOtherIndex = parent.indexOf(otherProcessorState);
                        if (currentIndex < currentOtherIndex) {
                            setProcessorSlot(parent, otherProcessorState, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            setProcessorSlot(parent, otherProcessorState, otherSlot + 1, undoManager);
                            return currentIndex - 1;
                        }
                    } else {
                        return parent.indexOf(otherProcessorState);
                    }
                } else if (otherSlot > slot) {
                    return parent.indexOf(otherProcessorState);
                }
            }
        }

        // TODO in this and other places, we assume processors are the only type of track child.
        return parent.getNumChildren();
    }

    bool canDuplicateSelected() const {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack[IDs::selected] && !selectedTrack.hasProperty(IDs::isMasterTrack))
            return true;
        const auto& selectedProcessor = getSelectedProcessor();
        return selectedProcessor.isValid() && !isMixerChannelProcessor(selectedProcessor);
    }

    void deleteSelectedItems() {
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (const auto &selectedItem : allSelectedItems) {
            deleteItem(selectedItem, &undoManager);
        }
    }

    void duplicateSelectedItems() {
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (auto &selectedItem : allSelectedItems) {
            duplicateItem(selectedItem, &undoManager);
        }
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK) && !item.hasProperty(IDs::isMasterTrack)) {
                auto copiedTrack = createAndAddTrack(undoManager, false, item, true);
                for (auto processor : item) {
                    saveProcessorStateInformationToState(processor);
                    auto copiedProcessor = processor.createCopy();
                    copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                    addProcessorToTrack(copiedTrack, copiedProcessor, item.indexOf(processor), undoManager);
                }
            } else if (item.hasType(IDs::PROCESSOR) && !isMixerChannelProcessor(item)) {
                saveProcessorStateInformationToState(item);
                auto track = item.getParent();
                auto copiedProcessor = item.createCopy();
                setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                addProcessorToTrack(track, copiedProcessor, track.indexOf(item), undoManager);
            }
        }
    }

    void deleteItem(const ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteItem(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                projectChangeBroadcaster.sendProcessorWillBeDestroyedMessage(item);
            }
            item.getParent().removeChild(item, undoManager);
            if (item.hasType(IDs::PROCESSOR)) {
                projectChangeBroadcaster.sendProcessorHasBeenDestroyedMessage(item);
            }
        }
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
    }

    void makeConnectionsSnapshot() {
        slotForNodeIdSnapshot.clear();
        for (auto track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    slotForNodeIdSnapshot.insert(std::__1::pair<int, int>(child[IDs::nodeId], child[IDs::processorSlot]));
                }
            }
        }
    }

    void restoreConnectionsSnapshot() {
        for (const auto& track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    setProcessorSlot(track, child, slotForNodeIdSnapshot.at(int(child[IDs::nodeId])), nullptr);
                }
            }
        }
    }

    void saveProcessorStateInformation() const {
        for (const auto& track : tracks) {
            for (auto processorState : track) {
                saveProcessorStateInformationToState(processorState);
            }
        }
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto* processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto* processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
    }

    TrackAndSlot findItemToSelectWithLeftRightDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithLeftRightDelta(delta);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    TrackAndSlot findItemToSelectWithUpDownDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithUpDownDelta(delta);
        else
            return findSelectionPaneItemToSelectWithUpDownDelta(delta);
    }

    TrackAndSlot findGridItemToSelectWithLeftRightDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};

        auto selectedSlot = findSelectedSlotForTrack(selectedTrack);
        if (selectedTrack.hasProperty(IDs::isMasterTrack)) {
            int slotToSelect = selectedSlot + delta;
            if (delta > 0) {
                if (selectedTrack[IDs::selected]) {
                    // re-selecting the processor will deselect the parent.
                    return {selectedTrack, selectedSlot};
                }
            } else if (delta < 0) {
                if (slotToSelect < 0 && !selectedTrack[IDs::selected])
                    return {selectedTrack};
            }
            return {selectedTrack, jmin(slotToSelect, viewManager.getNumMasterProcessorSlots() - 1)};
        } else {
            auto siblingTrackToSelect = selectedTrack.getSibling(delta);
            if (siblingTrackToSelect.isValid() && !siblingTrackToSelect.hasProperty(IDs::isMasterTrack)) {
                return {siblingTrackToSelect, selectedSlot};
            }
        }

        return {};
    }

    TrackAndSlot findGridItemToSelectWithUpDownDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        int selectedProcessorSlot = findSelectedSlotForTrack(selectedTrack);
        if (selectedTrack.hasProperty(IDs::isMasterTrack)) {
            if (delta < 0) {
                auto trackToSelect = getTrackWithViewIndex(selectedProcessorSlot - viewManager.getMasterViewSlotOffset());
                if (!trackToSelect.isValid() || trackToSelect.hasProperty(IDs::isMasterTrack))
                    trackToSelect = getTrack(getNumNonMasterTracks() - 1);
                return {trackToSelect, viewManager.getNumTrackProcessorSlots() - 1};
            } else {
                return {};
            }
        } else {
            auto slotToSelect = selectedProcessorSlot + delta;
            if (delta > 0) {
                if (selectedTrack[IDs::selected]) {
                    // re-selecting the processor will deselect the parent.
                    return {selectedTrack, selectedProcessorSlot};
                } else if (slotToSelect >= viewManager.getNumTrackProcessorSlots()) {
                    const auto& masterTrack = getMasterTrack();
                    if (masterTrack.isValid()) {
                        auto masterProcessorSlotToSelect = getViewIndexForTrack(selectedTrack) + viewManager.getMasterViewSlotOffset();
                        return {masterTrack, jmin(masterProcessorSlotToSelect, viewManager.getNumMasterProcessorSlots() - 1)};
                    }
                }
            } else if (delta < 0) {
                if (slotToSelect < 0 && !selectedTrack[IDs::selected])
                    return {selectedTrack};
            }

            return {selectedTrack, jmin(slotToSelect, viewManager.getNumTrackProcessorSlots() - 1)};
        }
    }

    TrackAndSlot findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};

        auto siblingTrackToSelect = selectedTrack.getSibling(delta);
        if (siblingTrackToSelect.isValid()) {
            if (selectedTrack[IDs::selected] || selectedTrack.getNumChildren() == 0) {
                return {siblingTrackToSelect};
            } else {
                auto selectedSlot = findSelectedSlotForTrack(selectedTrack);
                if (selectedSlot != -1) {
                    const auto& processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect, selectedSlot);
                    return processorToSelect.isValid() ? TrackAndSlot(siblingTrackToSelect, processorToSelect) : TrackAndSlot(siblingTrackToSelect);
                }
            }
        }

        return {};
    }

    TrackAndSlot findSelectionPaneItemToSelectWithUpDownDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};

        const auto& selectedProcessor = findSelectedProcessorForTrack(selectedTrack);
        if (delta > 0 && selectedTrack[IDs::selected]) {
            // re-selecting the processor will deselect the parent.
            return {selectedTrack, selectedProcessor};
        }

        int selectedProcessorSlot = findSelectedSlotForTrack(selectedTrack);
        auto siblingProcessorToSelect = selectedProcessor.getSibling(delta);
        if (!siblingProcessorToSelect.isValid()) {
            for (int slot = selectedProcessorSlot;
                 (delta < 0 ? slot >= 0 : slot < viewManager.getNumAvailableSlotsForTrack(selectedTrack));
                 slot += delta) {
                const auto &processor = selectedTrack.getChildWithProperty(IDs::processorSlot, slot);
                if (processor.isValid()) {
                    siblingProcessorToSelect = processor;
                    break;
                }
            }
        }

        if (delta < 0 && !siblingProcessorToSelect.isValid() && !selectedTrack[IDs::selected]) {
            return {selectedTrack};
        }

        if (siblingProcessorToSelect.isValid())
            return {selectedTrack, siblingProcessorToSelect};
        return {};
    }

    ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot) const {
        int nearestSlot = INT_MAX;
        ValueTree nearestProcessor;
        for (const auto& processor : track) {
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

    void setInitialSelectPosition() {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid()) {
            auto selectedSlot = findSelectedSlotForTrack(selectedTrack);
            if (selectedSlot != -1) {
                initialSelectTrackAndSlot = std::make_unique<TrackAndSlot>(selectedTrack, selectedSlot);
            }
        }
    }

    void resetInitialSelectPosition() {
        initialSelectTrackAndSlot.reset();
    }

    void clear() {
        while (tracks.getNumChildren() > 0)
            deleteItem(tracks.getChild(tracks.getNumChildren() - 1), nullptr);
    }

    // NOTE: assumes the track hasn't been added yet!
    const String makeTrackNameUnique(const String& trackName) {
        for (auto track : tracks) {
            String otherTrackName = track[IDs::name];
            if (otherTrackName == trackName) {
                if (trackName.contains("-")) {
                    int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                    if (i != 0) {
                        return makeTrackNameUnique(trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
                    }
                } else {
                    return makeTrackNameUnique(trackName + "-" + String(1));
                }
            }
        }

        return trackName;
    }

    void updateViewTrackOffsetToInclude(int trackIndex) {
        if (trackIndex < 0)
            return; // invalid
        auto viewTrackOffset = viewManager.getGridViewTrackOffset();
        if (trackIndex >= viewTrackOffset + NUM_VISIBLE_TRACKS)
            viewManager.setGridViewTrackOffset(trackIndex - NUM_VISIBLE_TRACKS + 1);
        else if (trackIndex < viewTrackOffset)
            viewManager.setGridViewTrackOffset(trackIndex);
        else if (getNumNonMasterTracks() - viewTrackOffset < NUM_VISIBLE_TRACKS && getNumNonMasterTracks() >= NUM_VISIBLE_TRACKS)
            // always show last N tracks if available
            viewManager.setGridViewTrackOffset(getNumNonMasterTracks() - NUM_VISIBLE_TRACKS);
    }

    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
        if (processorSlot < 0)
            return; // invalid
        if (isMasterTrack) {
            auto viewSlotOffset = viewManager.getMasterViewSlotOffset();
            if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
                viewManager.setMasterViewSlotOffset(processorSlot - NUM_VISIBLE_TRACKS + 1);
            else if (processorSlot < viewSlotOffset)
                viewManager.setMasterViewSlotOffset(processorSlot);
            processorSlot = viewManager.getNumTrackProcessorSlots();
        }

        auto viewSlotOffset = viewManager.getGridViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
            viewManager.setGridViewSlotOffset(processorSlot - NUM_VISIBLE_TRACKS + 1);
        else if (processorSlot < viewSlotOffset)
            viewManager.setGridViewSlotOffset(processorSlot);
    }

    static constexpr int NUM_VISIBLE_TRACKS = 8;
    static constexpr int NUM_VISIBLE_PROCESSOR_SLOTS = 10;

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() { return trackWidth; }
    int getProcessorHeight() { return processorHeight; }

    static constexpr int TRACK_LABEL_HEIGHT = 32;
    int trackWidth {0}, processorHeight {0};
private:
    ValueTree tracks;
    ViewStateManager& viewManager;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProjectChangeBroadcaster& projectChangeBroadcaster;
    ProcessorManager &processorManager;
    UndoManager &undoManager;

    std::unordered_map<int, int> slotForNodeIdSnapshot;
    std::unique_ptr<TrackAndSlot> initialSelectTrackAndSlot {};
};
