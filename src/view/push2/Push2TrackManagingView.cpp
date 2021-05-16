#include "Push2TrackManagingView.h"


Push2TrackManagingView::Push2TrackManagingView(ViewState &view, TracksState &tracks, Project &project, Push2MidiCommunicator &push2)
        : Push2ComponentBase(view, tracks, push2), project(project) {
    tracks.addListener(this);
    view.addListener(this);

    for (int i = 0; i < NUM_COLUMNS; i++)
        addChildComponent(trackLabels.add(new Push2Label(i, false, push2)));
}

Push2TrackManagingView::~Push2TrackManagingView() {
    push2.getPush2Colours().removeListener(this);
    view.removeListener(this);
    tracks.removeListener(this);
}

void Push2TrackManagingView::resized() {
    auto r = getLocalBounds();
    auto labelWidth = getWidth() / NUM_COLUMNS;
    auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
    for (auto *trackLabel : trackLabels)
        trackLabel->setBounds(bottom.removeFromLeft(labelWidth));
}

void Push2TrackManagingView::belowScreenButtonPressed(int buttonIndex) {
    auto track = tracks.getTrackWithViewIndex(buttonIndex);
    if (track.isValid() && !TracksState::isMasterTrack(track))
        project.setTrackSelected(track, true);
}

void Push2TrackManagingView::updateEnabledPush2Buttons() {
    auto focusedTrack = tracks.getFocusedTrack();
    for (auto *label : trackLabels)
        label->setVisible(false);

    if (!isVisible()) return;

    int labelIndex = 0;
    for (int i = 0; i < jmin(trackLabels.size(), tracks.getNumNonMasterTracks()); i++) {
        auto *label = trackLabels.getUnchecked(labelIndex++);
        const auto &track = tracks.getTrackWithViewIndex(i);
        // TODO left/right buttons
        label->setVisible(true);
        label->setMainColour(TracksState::getTrackColour(track));
        label->setText(track[TrackStateIDs::name], dontSendNotification);
        label->setSelected(track == focusedTrack);
    }
}

void Push2TrackManagingView::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (TrackState::isType(child))
        trackAdded(child);
}

void Push2TrackManagingView::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) {
    if (TrackState::isType(child))
        trackRemoved(child);
}

void Push2TrackManagingView::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (TrackState::isType(tree)) {
        if (i == TrackStateIDs::name || i == TrackStateIDs::colour) {
            int trackIndex = tracks.getViewIndexForTrack(tree);
            if (trackIndex < 0 || trackIndex >= trackLabels.size())
                return;
            if (i == TrackStateIDs::name && !TracksState::isMasterTrack(tree))
                trackLabels.getUnchecked(trackIndex)->setText(tree[TrackStateIDs::name], dontSendNotification);
        } else if (i == TrackStateIDs::selected && tree[TrackStateIDs::selected]) {
            trackSelected(tree);
        }
    } else if (i == ViewStateIDs::gridViewTrackOffset) {
        updateEnabledPush2Buttons();
    }
}

void Push2TrackManagingView::valueTreeChildOrderChanged(ValueTree &tree, int, int) {
    if (TracksState::isType(tree))
        updateEnabledPush2Buttons();
}

void Push2TrackManagingView::trackColourChanged(const String &trackUuid, const Colour &colour) {
    auto track = tracks.findTrackWithUuid(trackUuid);
    if (!TracksState::isMasterTrack(track))
        if (auto *trackLabel = trackLabels[tracks.getViewIndexForTrack(track)])
            trackLabel->setMainColour(colour);
}