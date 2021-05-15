#include "OutputChannelsState.h"

void OutputChannelsState::loadFromState(const ValueTree &fromState) {
    state.copyPropertiesFrom(fromState, nullptr);
    moveAllChildren(fromState, state, nullptr);
}
