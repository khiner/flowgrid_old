#include "Push2TrackManagingView.h"


Push2TrackManagingView::Push2TrackManagingView(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2)
        : Push2ComponentBase(view, tracks, push2), project(project) {
    tracks.addChildListener(this);
    view.addListener(this);

    for (int i = 0; i < NUM_COLUMNS; i++)
        addChildComponent(trackLabels.add(new Push2Label(i, false, push2)));
}

Push2TrackManagingView::~Push2TrackManagingView() {
    push2.getPush2Colours().removeListener(this);
    view.removeListener(this);
    tracks.removeChildListener(this);
}

void Push2TrackManagingView::resized() {
    auto r = getLocalBounds();
    auto labelWidth = getWidth() / NUM_COLUMNS;
    auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
    for (auto *trackLabel : trackLabels)
        trackLabel->setBounds(bottom.removeFromLeft(labelWidth));
}

void Push2TrackManagingView::belowScreenButtonPressed(int buttonIndex) {
    auto *track = tracks.getTrackWithViewIndex(buttonIndex);
    if (track != nullptr && !track->isMaster())
        project.setTrackSelected(track, true);
}

void Push2TrackManagingView::updateEnabledPush2Buttons() {
    for (auto *label : trackLabels)
        label->setVisible(false);

    if (!isVisible()) return;

    const auto *focusedTrack = tracks.getFocusedTrack();
    int labelIndex = 0;
    for (int i = 0; i < jmin(trackLabels.size(), tracks.getNumNonMasterTracks()); i++) {
        auto *label = trackLabels.getUnchecked(labelIndex++);
        const auto *track = tracks.getTrackWithViewIndex(i);
        // TODO left/right buttons
        label->setVisible(true);
        label->setMainColour(track->getColour());
        label->setText(track->getName(), dontSendNotification);
        label->setSelected(track == focusedTrack);
    }
}

void Push2TrackManagingView::onChildChanged(Track *track, const Identifier &i) {
    if (i == TrackIDs::name || i == TrackIDs::colour) {
        int trackIndex = tracks.getViewIndexForTrack(track);
        if (trackIndex < 0 || trackIndex >= trackLabels.size()) return;

        if (i == TrackIDs::name && !track->isMaster()) {
            trackLabels.getUnchecked(trackIndex)->setText(track->getName(), dontSendNotification);
        }
    } else if (i == TrackIDs::selected && track->isSelected()) {
        trackSelected(track);
    }
}

void Push2TrackManagingView::trackColourChanged(const String &trackUuid, const Colour &colour) {
    auto *track = tracks.findTrackWithUuid(trackUuid);
    if (track == nullptr) return;
    if (!track->isMaster())
        if (auto *trackLabel = trackLabels[tracks.getViewIndexForTrack(track)])
            trackLabel->setMainColour(colour);
}
