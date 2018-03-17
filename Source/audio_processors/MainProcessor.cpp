#include <push2/Push2MidiCommunicator.h>
#include "MainProcessor.h"

typedef Push2MidiCommunicator Push2;
const auto& ccNumberForControlLabel = Push2::ccNumberForControlLabel;

MainProcessor::MainProcessor(int inputChannelCount, int outputChannelCount): source(new ToneGeneratorAudioSource) {

    this->setLatencySamples(0);
    // if this was a plug-in, the plug-in wrapper code in JUCE would query us
    // for our channel configuration and call the setPlayConfigDetails() method
    // so that things would be set correctly internally as an AudioProcessor
    // object (which are always initialized as zero in, zero out). The sample rate
    // and blockSize values will get sent to us again when our prepareToPlay()
    // method is called before playback begins.
    this->setPlayConfigDetails(inputChannelCount, outputChannelCount, 0, 0);
    addParameter(new AudioParameterFloat("masterVolume", "Volume", 0.0f, 1.0f, 0.5f));
}

int MainProcessor::parameterIndexForMidiCcNumber(const int midiCcNumber) const {
    static const int MASTER_KNOB = ccNumberForControlLabel.at(Push2::ControlLabel::masterKnob);
    if (midiCcNumber == MASTER_KNOB) {
        return MAIN_VOLUME_INDEX;
    }

    return -1;
}

// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();
    const int parameterIndex = parameterIndexForMidiCcNumber(ccNumber);

    if (parameterIndex < 0)
        return;
    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    setParameter(parameterIndex, value);
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
    //source->getNextAudioBlock(AudioSourceChannelInfo(buffer));
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

float MainProcessor::getParameter(int parameterIndex) {
    return 0.f;
}

const String MainProcessor::getParameterText(int parameterIndex) {
    return "0.0";
}

void MainProcessor::setParameter(int parameterIndex, float newValue) {

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

