#include <push2/Push2MidiCommunicator.h>
#include <intruments/SineBank.h>
#include "MainProcessor.h"

MainProcessor::MainProcessor(int inputChannelCount, int outputChannelCount):
        undoManager(30000,30), state(*this, &undoManager),
        masterVolumeParamId("masterVolume") {

    this->setLatencySamples(0);

    // if this was a plug-in, the plug-in wrapper code in JUCE would query us
    // for our channel configuration and call the setPlayConfigDetails() method
    // so that things would be set correctly internally as an AudioProcessor
    // object (which are always initialized as zero in, zero out). The sample rate
    // and blockSize values will get sent to us again when our prepareToPlay()
    // method is called before playback begins.
    this->setPlayConfigDetails(inputChannelCount, outputChannelCount, 0, 0);

    currentInstrument = std::make_unique<SineBank>(state);
    for (int i = 0; i < currentInstrument->getNumParameters(); i++) {
        parameterIdForMidiNumber.insert(std::make_pair<int, String>(Push2::ccNumberForTopKnobIndex(i), currentInstrument->getParameterId(i)));
    }

    masterVolumeParam = state.createAndAddParameter("masterVolume", "Volume", "Volume",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(Decibels::gainToDecibels<float>(value, 0), 3) + "dB"; }, nullptr);

    state.addParameterListener(masterVolumeParamId, this);
    mixerAudioSource.addInputSource(currentInstrument->getAudioSource(), false);
}


// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();
    auto parameterIdEntry = parameterIdForMidiNumber.find(ccNumber);
    if (parameterIdEntry == parameterIdForMidiNumber.end()) {
        return;
    }

    String parameterId = parameterIdEntry->second;

    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    auto param = state.getParameter(parameterId);

    auto newValue = param->getValue() + value / 5.0f; // todo move manual scaling to param

    if (newValue > 0) {
        param->setValue(newValue);
    }
}

const String MainProcessor::getName() const {
    return "MainProcessor";
}

void MainProcessor::prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) {
    this->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, estimatedSamplesPerBlock);
}

void MainProcessor::releaseResources() {

}

void MainProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);
    mixerAudioSource.getNextAudioBlock(channelInfo);
    gain.applyGain(buffer, channelInfo.numSamples);
}

const String MainProcessor::getInputChannelName(int channelIndex) const {
    return "channel " + String(channelIndex);
}

const String MainProcessor::getOutputChannelName(int channelIndex) const {
    return "channel " + String(channelIndex);
}

bool MainProcessor::isInputChannelStereoPair(int index) const {
    return (2 == getTotalNumInputChannels());
}

bool MainProcessor::isOutputChannelStereoPair(int index) const {
    return (2 == getTotalNumOutputChannels());
}

bool MainProcessor::silenceInProducesSilenceOut() const {
    return true;
}

bool MainProcessor::acceptsMidi() const {
    return false;
}

bool MainProcessor::producesMidi() const {
    return false;
}

AudioProcessorEditor* MainProcessor::createEditor() {
    return nullptr;
}

bool MainProcessor::hasEditor() const {
    return false;
}

int MainProcessor::getNumParameters() {
    return 0;
}

const String MainProcessor::getParameterName(int parameterIndex) {
    return "parameter " + String(parameterIndex);
}

const String MainProcessor::getParameterText(int parameterIndex) {
    return "0.0";
}

int MainProcessor::getNumPrograms() {
    return 0;
}

int MainProcessor::getCurrentProgram() {
    return 0;
}

void MainProcessor::setCurrentProgram(int index) {

}

const String MainProcessor::getProgramName(int index) {
    return "program #" + String(index);
}

void MainProcessor::changeProgramName(int index, const String& newName) {

}

void MainProcessor::getStateInformation(juce::MemoryBlock& destData) {

}

void MainProcessor::setStateInformation(const void* data, int sizeInBytes) {

}

double MainProcessor::getTailLengthSeconds() const {
    return 0;
}

