#pragma once

#include "push2/Push2MidiCommunicator.h"

class Push2NoteModePadLedManager : public MidiInputCallback, public Push2Colours::Listener {
public:
    Push2NoteModePadLedManager(Tracks &tracks, Push2MidiCommunicator &push2) : tracks(tracks), push2(push2) {
        push2.getPush2Colours().addListener(this);
    }

    ~Push2NoteModePadLedManager() override {
        push2.getPush2Colours().removeListener(this);
    }

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override {
        if (!isVisible) return;

        if (message.isNoteOff())
            push2.setPadColour(message.getNoteNumber(), getPadColourForNoteNumber(message.getNoteNumber()));
        else
            push2.setPadColour(message.getNoteNumber(), Colours::green);
    }

    void setVisible(bool visible) {
        this->isVisible = visible;
        updatePadColours();
    }

    void trackSelected(const Track *track) {
        if (track != nullptr) {
            selectedTrackColour = track->getColour();
            updatePadColours();
        }
    }

    void trackColourChanged(const String &trackUuid, const Colour &colour) override {
        // TODO a bit slow. might want to pass this all down from parent instead of listening directly
        const auto *track = tracks.findTrackWithUuid(trackUuid);
        if (track != nullptr && track->hasSelections()) {
            selectedTrackColour = colour;
            updatePadColours();
        }
    }

    void colourAdded(const Colour &colour, uint8 index) override {}

private:
    Tracks &tracks;
    Push2MidiCommunicator &push2;
    Colour selectedTrackColour{Colours::white};
    bool isVisible{false};

    void updatePadColours() {
        for (int note = 36; note < 100; note++) {
            if (isVisible)
                push2.setPadColour(note, getPadColourForNoteNumber(note));
            else
                push2.disablePad(note);
        }
    }

    const Colour &getPadColourForNoteNumber(int noteNumber) {
        return noteNumber % 12 == 0 ? selectedTrackColour : Colours::white;
    }
};
