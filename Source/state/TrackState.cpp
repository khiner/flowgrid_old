#include "TrackState.h"

void TrackState::loadFromState(const ValueTree &fromState) {
    state.copyPropertiesFrom(fromState, nullptr);
    moveAllChildren(fromState, state, nullptr);
}
