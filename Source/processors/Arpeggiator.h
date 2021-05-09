#pragma once

#include "DefaultAudioProcessor.h"

class Arpeggiator : public DefaultAudioProcessor {
public:

    Arpeggiator() : DefaultAudioProcessor(getPluginDescription(), AudioChannelSet::disabled()) {
        addParameter(speed = new AudioParameterFloat("speed", "Arpeggiator Speed", 0.0, 1.0, 0.5));
    }

    ~Arpeggiator() override = default;

    static String name() { return "Arpeggiator"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, true, AudioChannelSet::disabled());
    }

    bool acceptsMidi() const override { return true; }

    bool producesMidi() const override { return true; }

    bool isMidiEffect() const override { return true; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        ignoreUnused(samplesPerBlock);

        notes.clear();
        currentNote = 0;
        lastNoteValue = -1;
        time = 0;
        rate = static_cast<float> (sampleRate);
    }

    void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi) override {
        // however we use the buffer to get timing information
        auto numSamples = buffer.getNumSamples();
        auto noteDuration = static_cast<int> (std::ceil(rate * 0.25f * (0.1f + (1.0f - (*speed)))));

        MidiMessage msg;
        int ignore;
        for (MidiBuffer::Iterator it(midi); it.getNextEvent(msg, ignore);) {
            if (msg.isNoteOn()) notes.add(msg.getNoteNumber());
            else if (msg.isNoteOff()) notes.removeValue(msg.getNoteNumber());
        }

        midi.clear();

        if ((time + numSamples) >= noteDuration) {
            auto offset = jmax(0, jmin((int) (noteDuration - time), numSamples - 1));

            if (lastNoteValue > 0) {
                midi.addEvent(MidiMessage::noteOff(1, lastNoteValue), offset);
                lastNoteValue = -1;
            }

            if (notes.size() > 0) {
                currentNote = (currentNote + 1) % notes.size();
                lastNoteValue = notes[currentNote];
                midi.addEvent(MidiMessage::noteOn(1, lastNoteValue, (uint8) 127), offset);
            }

        }

        time = (time + numSamples) % noteDuration;
    }

private:
    AudioParameterFloat *speed;
    int currentNote{0}, lastNoteValue{0};
    int time{0};
    float rate{0};
    SortedSet<int> notes;
};
