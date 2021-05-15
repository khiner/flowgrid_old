#include "ConnectionState.h"

void ConnectionState::loadFromState(const ValueTree &fromState) {
    state.copyPropertiesFrom(fromState, nullptr);
    moveAllChildren(fromState, state, nullptr);
}
