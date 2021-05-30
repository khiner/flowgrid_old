#pragma once

#include "Stateful.h"
#include "ProcessorLanes.h"

namespace TrackIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACK)
ID(uuid)
ID(colour)
ID(name)
ID(selected)
ID(isMaster)
#undef ID
}

struct Track : public Stateful<Track> {
    explicit Track(ValueTree state): Stateful<Track>(std::move(state)) {}

    Track copy() { return Track(state); }

    static Identifier getIdentifier() { return TrackIDs::TRACK; }

    static BigInteger createFullSelectionBitmask(int numSlots) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, numSlots, true);
        return selectedSlotsMask;
    }

    int getIndex() const { return state.getParent().indexOf(state); }
    String getUuid() const { return state[TrackIDs::uuid]; }
    Colour getColour() const { return Colour::fromString(state[TrackIDs::colour].toString()); }
    Colour getDisplayColour() const { return isSelected() ? getColour().brighter(0.25) : getColour(); }
    String getName() const { return state[TrackIDs::name]; }
    bool isSelected() const { return state[TrackIDs::selected]; }
    bool isMaster() const { return state[TrackIDs::isMaster]; }
    static bool isMaster(const ValueTree &state) { return state[TrackIDs::isMaster]; }
    BigInteger getSlotMask() const { return ProcessorLane::getSelectedSlotsMask(getProcessorLane(state)); }
    bool isSlotSelected(int slot) const { return getSlotMask()[slot]; }
    int firstSelectedSlot() const { return getSlotMask().getHighestBit(); }
    bool hasAnySlotSelected() const { return firstSelectedSlot(state) != -1; }
    bool hasSelections() const { return isSelected() || hasAnySlotSelected(); }
    ValueTree getProcessorAtSlot(int slot) const {
        const auto &lane = getProcessorLane(state);
        return lane.isValid() ? lane.getChildWithProperty(ProcessorIDs::slot, slot) : ValueTree();
    }
    int getInsertIndexForProcessor(const ValueTree &processor, int insertSlot) const;
    ValueTree findProcessorNearestToSlot(int slot) const;
    ValueTree findFirstSelectedProcessor() const;
    ValueTree findLastSelectedProcessor() const;
    Array<ValueTree> findSelectedProcessors() const;
    Array<ValueTree> getAllProcessors() const;
    ValueTree getProcessorLanes() const {
        return state.isValid() ? state.getChildWithName(ProcessorLanesIDs::PROCESSOR_LANES) : ValueTree();
    }
    ValueTree getProcessorLane() const {
        const auto &lanes = getProcessorLanes(state);
        return lanes.isValid() ? lanes.getChild(0) : ValueTree();
    }
    ValueTree getInputProcessor() const {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackInputProcessorName());
    }
    ValueTree getOutputProcessor() const {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }

    void setName(const String &name, UndoManager *undoManager) {
        if (undoManager) undoManager->beginNewTransaction();
        state.setProperty(TrackIDs::name, name, undoManager);
    }
    void setColour(const Colour &colour, UndoManager *undoManager) { state.setProperty(TrackIDs::colour, colour.toString(), undoManager); }
    void setSelected(bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    void setMaster(bool isMaster) { state.setProperty(TrackIDs::isMaster, isMaster, nullptr); }

    // TODO delete static copies
    static BigInteger getSlotMask(const ValueTree &state) { return ProcessorLane::getSelectedSlotsMask(getProcessorLane(state)); }
    static bool isSlotSelected(const ValueTree &state, int slot) { return getSlotMask(state)[slot]; }
    static int firstSelectedSlot(const ValueTree &state) { return getSlotMask(state).getHighestBit(); }
    static bool hasAnySlotSelected(const ValueTree &state) { return firstSelectedSlot(state) != -1; }
    static ValueTree getProcessorAtSlot(const ValueTree &state, int slot) {
        const auto &lane = getProcessorLane(state);
        return lane.isValid() ? lane.getChildWithProperty(ProcessorIDs::slot, slot) : ValueTree();
    }
    static ValueTree findFirstSelectedProcessor(const ValueTree &state);
    static Array<ValueTree> findSelectedProcessors(const ValueTree &state);
    static ValueTree getProcessorLanes(const ValueTree &state) {
        return state.isValid() ? state.getChildWithName(ProcessorLanesIDs::PROCESSOR_LANES) : ValueTree();
    }
    static ValueTree getProcessorLane(const ValueTree &state) {
        const auto &lanes = getProcessorLanes(state);
        return lanes.isValid() ? lanes.getChild(0) : ValueTree();
    }
    static ValueTree getProcessorLaneForProcessor(const ValueTree &processor) {
        const auto &parent = processor.getParent();
        return parent.hasType(ProcessorLaneIDs::PROCESSOR_LANE) ? parent : getProcessorLane(parent);
    }
    static ValueTree getOutputProcessor(const ValueTree &state) {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }
    static int getInsertIndexForProcessor(const ValueTree &state, const ValueTree &processor, int insertSlot);
    bool isProcessorSelected(const ValueTree &processor) const {
        return Processor::isType(processor) && isSlotSelected(Processor::getSlot(processor));
    }
    static bool isProcessorSelected(const ValueTree &track, const ValueTree &processor) {
        return Processor::isType(processor) && isSlotSelected(track, Processor::getSlot(processor));
    }
    bool isProcessorLeftToRightFlowing(const ValueTree &processor) const {
        return isMaster() && !Processor::isTrackIOProcessor(processor);
    }
    static void setUuid(ValueTree &state, const String &uuid) { state.setProperty(TrackIDs::uuid, uuid, nullptr); }
    static void setName(ValueTree &state, const String &name) { state.setProperty(TrackIDs::name, name, nullptr); }
    static void setColour(ValueTree &state, const Colour &colour) { state.setProperty(TrackIDs::colour, colour.toString(), nullptr); }
    static void setSelected(ValueTree &state, bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    static void setMaster(ValueTree &state, bool isMaster) { state.setProperty(TrackIDs::isMaster, isMaster, nullptr); }
};
