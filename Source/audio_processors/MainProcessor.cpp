#include <push2/Push2MidiCommunicator.h>
#include <intruments/SineBank.h>
#include <drow/Identifiers.h>
#include "MainProcessor.h"

MainProcessor::MainProcessor(int inputChannelCount, int outputChannelCount):
        DefaultAudioProcessor(inputChannelCount, outputChannelCount),
        undoManager(30000,30), state(*this, &undoManager),
        masterVolumeParamId("masterVolume") {

    state.createAndAddParameter("masterVolume", "Volume", "dB",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(Decibels::gainToDecibels<float>(value, 0), 3) + "dB"; }, nullptr);

    state.addParameterListener(masterVolumeParamId, this);
    gain.setValue(0.5f);
}


void MainProcessor::setInstrument(const Identifier& instrumentId) {
    if (instrumentId == IDs::SINE_BANK_INSTRUMENT) {
        currentInstrument = std::make_unique<SineBank>(state);
    }

    state.state = ValueTree(Identifier("sound-machine"));
}

// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();

    if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::undo)) {
        undoManager.undo();
        return;
    }

    StringRef parameterId;
    if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::masterKnob)) {
        parameterId = masterVolumeParamId;
    } else {
        for (int i = 0; i < 8; i ++) {
            if (ccNumber == Push2::ccNumberForTopKnobIndex(i)) {
                parameterId = currentInstrument->getParameterId(i);
                break;
            }
        }

    }

    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    auto param = state.getParameter(parameterId);

    auto newValue = param->getValue() + value / 5.0f; // todo move manual scaling to param

    if (newValue > 0) {
        param->setValue(newValue);
    }
}

void MainProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);
    currentInstrument->processBlock(buffer, midiMessages);
    gain.applyGain(buffer, channelInfo.numSamples);
}

