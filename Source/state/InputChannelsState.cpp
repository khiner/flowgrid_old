#include "InputChannelsState.h"

void InputChannelsState::loadFromState(const ValueTree &fromState) {
    state.copyPropertiesFrom(fromState, nullptr);
    moveAllChildren(fromState, state, nullptr);
}
