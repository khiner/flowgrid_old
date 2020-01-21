#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "ValueTreeItems.h"
#include "state_managers/ViewStateManager.h"
#include "processors/ProcessorManager.h"
#include "StateManager.h"

class TracksStateManager :
        public StateManager,
        public ProcessorLifecycleBroadcaster,
        private ValueTree::Listener {
public:
    struct TrackAndSlot {
        TrackAndSlot() : slot(0) {};
        TrackAndSlot(ValueTree track, const ValueTree& processor) : track(std::move(track)), slot(processor[IDs::processorSlot]) {}
        TrackAndSlot(ValueTree track, const int slot) : track(std::move(track)), slot(jmax(-1, slot)) {}
        TrackAndSlot(ValueTree track) : track(std::move(track)), slot(-1) {}

        bool isValid() const { return track.isValid(); }

        void select(TracksStateManager& tracksManager) {
            if (!track.isValid())
                return;

            const auto& selectedTrack = tracksManager.getSelectedTrack();
            if (slot != -1)
                tracksManager.selectProcessorSlot(track, slot);
            if (slot == -1 || (selectedTrack != track && selectedTrack[IDs::selected]))
                tracksManager.setTrackSelected(track, true);
        }

        ValueTree track;
        const int slot;
    };

    TracksStateManager(ViewStateManager& viewManager, StatefulAudioProcessorContainer& audioProcessorContainer,
                       ProcessorManager& processorManager, UndoManager& undoManager)
            : viewManager(viewManager), audioProcessorContainer(audioProcessorContainer),
              processorManager(processorManager), undoManager(undoManager) {
        tracks = ValueTree(IDs::TRACKS);
        tracks.addListener(this);
        tracks.setProperty(IDs::name, "Tracks", nullptr);
    }

    void loadFromState(const ValueTree& state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);

        // Re-save all non-string value types,
        // since type information is not saved in XML
        // Also, re-set some vars just to trigger the event (like selected slot mask)
        for (auto track : tracks) {
            if (track.hasProperty(IDs::isMasterTrack)) {
                resetVarToBool(track, IDs::isMasterTrack, this);
            }
            if (track.hasProperty(IDs::selectedSlotsMask)) {
                track.sendPropertyChangeMessage(IDs::selectedSlotsMask);
            }
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

    ValueTree& getState() override { return tracks; }

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

    static inline bool isMasterTrack(const ValueTree& track) {
        return track.hasProperty(IDs::isMasterTrack);
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
        return getSlotMask(track).getHighestBit();
    }

    inline bool trackHasAnySlotSelected(const ValueTree &track) const {
        return findSelectedSlotForTrack(track) != -1;
    }

    inline const Colour getTrackColour(const ValueTree& track) const {
        return Colour::fromString(track[IDs::colour].toString());
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

    inline BigInteger getSlotMask(const ValueTree& track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    inline bool isSlotSelected(const ValueTree& track, int slot) const {
        return getSlotMask(track)[slot];
    }

    inline bool isProcessorSelected(const ValueTree& processor) const {
        return processor.hasType(IDs::PROCESSOR) &&
               isSlotSelected(processor.getParent(), processor[IDs::processorSlot]);
    }

    void setTrackSelected(ValueTree& track, bool selected, bool deselectOthers=true) {
        // take care of this track
        if (selected != bool(track[IDs::selected])) {
            track.setProperty(IDs::selected, selected, nullptr);
        } else {
            // we want to send out a change even no matter what
            track.sendPropertyChangeMessage(IDs::selected);
        }
        // take care of other tracks
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track);
        if (selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, -1);
        } else {
            if (selected && deselectOthers) {
                for (auto otherTrack : tracks) {
                    if (otherTrack != track) {
                        setTrackSelected(otherTrack, false, false);
                    }
                }
            }
        }
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true) {
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track, slot);
        if (selectionStartTrackAndSlot != nullptr) // shift+select
            selectRectangle(track, slot);
        else
            setProcessorSlotSelected(track, slot, true, deselectOthers);
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
    }

    void setProcessorSlot(const ValueTree& track, ValueTree& processor, int newSlot, UndoManager* undoManager) {
        if (!processor.isValid())
            return;

        if (newSlot >= getMixerChannelSlotForTrack(track) && !isMixerChannelProcessor(processor)) {
            if (isMasterTrack(track)) {
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

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), track, undoManager, -1);

        track.setProperty(IDs::selected, true, undoManager);

        return track;
    }

    ValueTree createAndAddProcessor(const PluginDescription& description, UndoManager* undoManager) {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid())
            return createAndAddProcessor(description, selectedTrack, undoManager, -1);
        else
            return {};
    }

    ValueTree createAndAddProcessor(const PluginDescription &description, ValueTree track, UndoManager *undoManager, int slot = -1) {
        if (description.name == MixerChannelProcessor::name() && getMixerChannelProcessorForTrack(track).isValid())
            return {}; // only one mixer channel per track

        if (ProcessorManager::isGeneratorOrInstrument(&description) &&
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
        if (selectedTrack[IDs::selected] && !isMasterTrack(selectedTrack))
            return true;
        const auto& selectedProcessor = getSelectedProcessor();
        return selectedProcessor.isValid() && !isMixerChannelProcessor(selectedProcessor);
    }

    void deleteSelectedItems() {
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (const auto &selectedItem : allSelectedItems) {
            deleteTrackOrProcessor(selectedItem, &undoManager);
        }
    }

    void duplicateSelectedItems() {
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (auto selectedItem : allSelectedItems) {
            duplicateItem(selectedItem, &undoManager);
        }
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK) && !isMasterTrack(item)) {
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

    void moveProcessor(ValueTree &processorState, int toTrackIndex, int toSlot, UndoManager *undoManager) {
        auto toTrack = getTrack(toTrackIndex);
        int fromSlot = processorState[IDs::processorSlot];
        if (fromSlot == toSlot && processorState.getParent() == toTrack)
            return;

        setProcessorSlot(processorState.getParent(), processorState, toSlot, undoManager);

        const int insertIndex = getParentIndexForProcessor(toTrack, processorState, undoManager);
        toTrack.moveChildFromParent(processorState.getParent(), processorState.getParent().indexOf(processorState), insertIndex, undoManager);

        makeSlotsValid(toTrack, undoManager);
    }

    void deleteTrackOrProcessor(const ValueTree &item, UndoManager *undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteTrackOrProcessor(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorWillBeDestroyedMessage(item);
            }
            item.getParent().removeChild(item, undoManager);
            if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorHasBeenDestroyedMessage(item);
            }
        }
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

    TrackAndSlot findItemToSelectWithLeftRightDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(delta, 0);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    TrackAndSlot findItemToSelectWithUpDownDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(0, delta);
        else
            return findSelectionPaneItemToSelectWithUpDownDelta(delta);
    }

    void startRectangleSelection() {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid()) {
            selectionStartTrackAndSlot = selectedTrack[IDs::selected]
                             ? std::make_unique<TrackAndSlot>(selectedTrack)
                             : std::make_unique<TrackAndSlot>(selectedTrack, findSelectedSlotForTrack(selectedTrack));
        }
    }

    void endRectangleSelection() {
        selectionStartTrackAndSlot.reset();
        selectionEndTrackAndSlot.reset();
    }

    void clear() {
        while (tracks.getNumChildren() > 0) {
            deleteTrackOrProcessor(tracks.getChild(tracks.getNumChildren() - 1), nullptr);
        }
    }

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() { return trackWidth; }
    int getProcessorHeight() { return processorHeight; }

    static constexpr int TRACK_LABEL_HEIGHT = 32;
private:
    ValueTree tracks;
    ViewStateManager& viewManager;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProcessorManager &processorManager;
    UndoManager &undoManager;

    std::unordered_map<int, int> slotForNodeIdSnapshot;
    std::unique_ptr<TrackAndSlot> selectionStartTrackAndSlot {};
    std::unique_ptr<TrackAndSlot> selectionEndTrackAndSlot {};
    int trackWidth {0}, processorHeight {0};

    Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (isMasterTrack(trackAndSlot.track))
            return {trackAndSlot.slot + viewManager.getGridViewTrackOffset() - viewManager.getMasterViewSlotOffset(),
                    viewManager.getNumTrackProcessorSlots()};
        else
            return {indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    const TrackAndSlot gridPositionToTrackAndSlot(const Point<int> gridPosition) const {
        if (gridPosition.y >= viewManager.getNumTrackProcessorSlots())
            return {getMasterTrack(),
                    jmin(gridPosition.x + viewManager.getMasterViewSlotOffset() - viewManager.getGridViewTrackOffset(),
                         viewManager.getNumMasterProcessorSlots() - 1)};
        else
            return {getTrack(jmin(gridPosition.x, getNumNonMasterTracks() - 1)),
                    jmin(gridPosition.y + viewManager.getGridViewSlotOffset(),
                         viewManager.getNumTrackProcessorSlots() - 1)};
    }

    void setProcessorSlotSelected(ValueTree track, int slot, bool selected, bool deselectOthers=true) {
        const auto currentSlotMask = getSlotMask(track);
        if (deselectOthers) {
            for (auto anyTrack : tracks) { // also deselect this track!
                setTrackSelected(anyTrack, false, false);
            }
        }
        auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
        newSlotMask.setBit(slot, selected);
        track.setProperty(IDs::selectedSlotsMask, newSlotMask.toString(2), nullptr);
    }

    void selectRectangle(const ValueTree &track, int slot) {
        const auto trackIndex = indexOf(track);
        const auto selectionStartTrackIndex = indexOf(selectionStartTrackAndSlot->track);
        const auto gridPosition = trackAndSlotToGridPosition({track, slot});
        Rectangle<int> selectionRectangle(trackAndSlotToGridPosition(*selectionStartTrackAndSlot), gridPosition);
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int otherTrackIndex = 0; otherTrackIndex < getNumTracks(); otherTrackIndex++) {
            auto otherTrack = getTrack(otherTrackIndex);
            bool trackSelected = (slot == -1 || selectionStartTrackAndSlot->slot == -1) &&
                                 ((selectionStartTrackIndex <= otherTrackIndex && otherTrackIndex <= trackIndex) ||
                                  (trackIndex <= otherTrackIndex && otherTrackIndex <= selectionStartTrackIndex));
            otherTrack.setProperty(IDs::selected, trackSelected, nullptr);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
    }

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        track.addChild(processor, insertIndex, undoManager);
        makeSlotsValid(track, undoManager);
        selectProcessor(processor);
        sendProcessorCreatedMessage(processor);
    }

    void addProcessorRow() {
        viewManager.addProcessorRow(&undoManager);
        for (const auto& track : tracks) {
            if (isMasterTrack(track))
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

    TrackAndSlot findGridItemToSelectWithDelta(int xDelta, int yDelta) const {
        const auto gridPosition = trackAndSlotToGridPosition(getMostRecentSelectedTrackAndSlot());
        return gridPositionToTrackAndSlot(gridPosition + Point<int>(xDelta, yDelta));
    }

    TrackAndSlot getMostRecentSelectedTrackAndSlot() const {
        if (selectionEndTrackAndSlot != nullptr) {
            return *selectionEndTrackAndSlot;
        } else {
            const auto& selectedTrack = getSelectedTrack();
            auto selectedProcessorSlot = findSelectedSlotForTrack(selectedTrack);
            return {selectedTrack, selectedProcessorSlot};
        }
    }

    void selectAllTrackSlots(ValueTree& track) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, viewManager.getNumAvailableSlotsForTrack(track), true);
        track.setProperty(IDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr);
    }

    void deselectAllTrackSlots(ValueTree& track) {
        track.removeProperty(IDs::selectedSlotsMask, nullptr);
    }

    TrackAndSlot findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};

        const auto& siblingTrackToSelect = selectedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return {};

        if (selectedTrack[IDs::selected] || selectedTrack.getNumChildren() == 0)
            return {siblingTrackToSelect};

        auto selectedSlot = findSelectedSlotForTrack(selectedTrack);
        if (selectedSlot != -1) {
            const auto& processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect, selectedSlot);
            return processorToSelect.isValid() ? TrackAndSlot(siblingTrackToSelect, processorToSelect) : TrackAndSlot(siblingTrackToSelect);
        }

        return {};
    }

    TrackAndSlot findSelectionPaneItemToSelectWithUpDownDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};

        const auto& selectedProcessor = findSelectedProcessorForTrack(selectedTrack);
        if (delta > 0 && selectedTrack[IDs::selected])
            return {selectedTrack, selectedProcessor}; // re-selecting the processor will deselect the parent.

        auto selectedProcessorSlot = findSelectedSlotForTrack(selectedTrack);
        const auto& siblingProcessorToSelect = selectedProcessor.getSibling(delta);
        if (siblingProcessorToSelect.isValid())
            return {selectedTrack, siblingProcessorToSelect};

        for (int slot = selectedProcessorSlot;
             (delta < 0 ? slot >= 0 : slot < viewManager.getNumAvailableSlotsForTrack(selectedTrack));
             slot += delta) {
            const auto& processor = selectedTrack.getChildWithProperty(IDs::processorSlot, slot);
            if (processor.isValid())
                return {selectedTrack, processor};
        }

        return delta < 0 && !selectedTrack[IDs::selected] ? TrackAndSlot(selectedTrack) : TrackAndSlot();
    }

    ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot) const {
        auto nearestSlot = INT_MAX;
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

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto* processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto* processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
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

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {}

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::TRACK) && i == IDs::selected) {
            if (tree[i]) {
                if (!isMasterTrack(tree))
                    viewManager.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
                selectAllTrackSlots(tree);
            } else {
                deselectAllTrackSlots(tree);
            }
        } else if (i == IDs::selectedSlotsMask) {
            if (!isMasterTrack(tree))
                viewManager.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
            auto slot = findSelectedSlotForTrack(tree);
            if (slot != -1)
                viewManager.updateViewSlotOffsetToInclude(slot, isMasterTrack(tree));
        } else if (i == IDs::processorSlot) {
            selectProcessor(tree);
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}
    void valueTreeParentChanged(ValueTree &) override {}
    void valueTreeRedirected(ValueTree &) override {}
};
