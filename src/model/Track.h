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

class Track : public Stateful<Track> {
public:
    static Identifier getIdentifier() { return TrackIDs::TRACK; }

    static String getUuid(const ValueTree &state) { return state[TrackIDs::uuid]; }
    static Colour getColour(const ValueTree &state) { return Colour::fromString(state[TrackIDs::colour].toString()); }
    static Colour getDisplayColour(const ValueTree &state) { return isSelected(state) ? getColour(state).brighter(0.25) : getColour(state); }
    static String getName(const ValueTree &state) { return state[TrackIDs::name]; }
    static bool isSelected(const ValueTree &state) { return state[TrackIDs::selected]; }
    static bool isMaster(const ValueTree &state) { return state[TrackIDs::isMaster]; }
    static BigInteger getSlotMask(const ValueTree &state);
    static bool isSlotSelected(const ValueTree &state, int slot) { return getSlotMask(state)[slot]; }
    static int firstSelectedSlot(const ValueTree &state) { return getSlotMask(state).getHighestBit(); }
    static bool hasAnySlotSelected(const ValueTree &state) { return firstSelectedSlot(state) != -1; }
    static bool hasSelections(const ValueTree &state) { return isSelected(state) || hasAnySlotSelected(state); }
    static ValueTree findProcessorNearestToSlot(const ValueTree &state, int slot);
    static ValueTree getProcessorAtSlot(const ValueTree &state, int slot) {
        const auto &lane = getProcessorLane(state);
        return lane.isValid() ? lane.getChildWithProperty(ProcessorIDs::slot, slot) : ValueTree();
    }
    static bool isProcessorSelected(const ValueTree &processor) { return Processor::isType(processor) && isSlotSelected(getTrackForProcessor(processor), Processor::getSlot(processor)); }
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
    static ValueTree getTrackForProcessor(const ValueTree &processor) {
        return isType(processor.getParent()) ? processor.getParent() : processor.getParent().getParent().getParent();
    }
    static ValueTree getInputProcessor(const ValueTree &state) {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackInputProcessorName());
    }
    static ValueTree getOutputProcessor(const ValueTree &state) {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }
    static Array<ValueTree> getAllProcessors(const ValueTree &state);
    static int getInsertIndexForProcessor(const ValueTree &state, const ValueTree &processor, int insertSlot);
    static bool isProcessorLeftToRightFlowing(const ValueTree &processor) {
        return isMaster(getTrackForProcessor(processor)) && !Processor::isTrackIOProcessor(processor);
    }

    static void setUuid(ValueTree &state, const String &uuid) { state.setProperty(TrackIDs::uuid, uuid, nullptr); }
    static void setName(ValueTree &state, const String &name) { state.setProperty(TrackIDs::name, name, nullptr); }
    static void setColour(ValueTree &state, const Colour &colour) { state.setProperty(TrackIDs::colour, colour.toString(), nullptr); }
    static void setSelected(ValueTree &state, bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    static void setMaster(ValueTree &state, bool isMaster) { state.setProperty(TrackIDs::isMaster, isMaster, nullptr); }
};
