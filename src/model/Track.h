#pragma once

#include <utility>

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

    static Identifier getIdentifier() { return TrackIDs::TRACK; }

    int getIndex() const { return state.getParent().indexOf(state); }
    String getUuid() const { return state[TrackIDs::uuid]; }
    Colour getColour() const { return Colour::fromString(state[TrackIDs::colour].toString()); }
    Colour getDisplayColour() const { return isSelected(state) ? getColour(state).brighter(0.25) : getColour(state); }
    String getName() const { return state[TrackIDs::name]; }
    bool isSelected() const { return state[TrackIDs::selected]; }
    bool isMaster() const { return state[TrackIDs::isMaster]; }
    BigInteger getSlotMask() const { return ProcessorLane::getSelectedSlotsMask(getProcessorLane(state)); }
    bool isSlotSelected(int slot) const { return getSlotMask()[slot]; }
    int firstSelectedSlot() const { return getSlotMask().getHighestBit(); }
    bool hasAnySlotSelected() const { return firstSelectedSlot(state) != -1; }
    bool hasSelections() const { return isSelected(state) || hasAnySlotSelected(state); }
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

    void setName(const String &name) { state.setProperty(TrackIDs::name, name, nullptr); }
    void setColour(const Colour &colour, UndoManager *undoManager) { state.setProperty(TrackIDs::colour, colour.toString(), undoManager); }
    void setSelected(bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    void setMaster(bool isMaster) { state.setProperty(TrackIDs::isMaster, isMaster, nullptr); }

    // TODO delete static copies
    static int getIndex(const ValueTree &state) { return state.getParent().indexOf(state); }
    static String getUuid(const ValueTree &state) { return state[TrackIDs::uuid]; }
    static Colour getColour(const ValueTree &state) { return Colour::fromString(state[TrackIDs::colour].toString()); }
    static String getName(const ValueTree &state) { return state[TrackIDs::name]; }
    static bool isSelected(const ValueTree &state) { return state[TrackIDs::selected]; }
    static bool isMaster(const ValueTree &state) { return state[TrackIDs::isMaster]; }
    static BigInteger getSlotMask(const ValueTree &state) { return ProcessorLane::getSelectedSlotsMask(getProcessorLane(state)); }
    static bool isSlotSelected(const ValueTree &state, int slot) { return getSlotMask(state)[slot]; }
    static int firstSelectedSlot(const ValueTree &state) { return getSlotMask(state).getHighestBit(); }
    static bool hasAnySlotSelected(const ValueTree &state) { return firstSelectedSlot(state) != -1; }
    static bool hasSelections(const ValueTree &state) { return isSelected(state) || hasAnySlotSelected(state); }
    static ValueTree findProcessorNearestToSlot(const ValueTree &state, int slot);
    static ValueTree getProcessorAtSlot(const ValueTree &state, int slot) {
        const auto &lane = getProcessorLane(state);
        return lane.isValid() ? lane.getChildWithProperty(ProcessorIDs::slot, slot) : ValueTree();
    }
    static ValueTree findFirstSelectedProcessor(const ValueTree &state);
    static ValueTree findLastSelectedProcessor(const ValueTree &state);
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
    static ValueTree getInputProcessor(const ValueTree &state) {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackInputProcessorName());
    }
    static ValueTree getOutputProcessor(const ValueTree &state) {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }
    static Array<ValueTree> getAllProcessors(const ValueTree &state);
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
