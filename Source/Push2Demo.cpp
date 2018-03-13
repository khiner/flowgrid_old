#include "Push2Demo.h"
#include "chrono"

using namespace std;

namespace {
    bool matchSubStringIgnoreCase(const std::string &haystack, const std::string &needle) {
        auto it = std::search(
                haystack.begin(), haystack.end(),
                needle.begin(), needle.end(),
                [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
        return it != haystack.end();
    }
}

Demo::Demo():  bridge() {
    openMidiDevice();
    elapsed = 0;
    startTimerHz(60); // animation timer
}

void Demo::openMidiDevice() {
    // Look for an input device matching Push 2
    auto devices = MidiInput::getDevices();
    int deviceIndex = -1;
    int index = 0;
    for (auto &device: devices) {
        if (matchSubStringIgnoreCase(device.toStdString(), "ableton push 2")) {
            deviceIndex = index;
            break;
        }
        index++;
    }

    if (deviceIndex == -1) {
        throw runtime_error("Failed to find input midi device for push2");
    }

    // Try opening the device
    midiInput.reset(MidiInput::openDevice(deviceIndex, this));
    if (!midiInput) {
        throw runtime_error("Failed to open MIDI input device");
    }

    /*
     https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Aftertouch
     In channel pressure mode (default), the pad with the highest pressure determines the value sent. The pressure range that produces aftertouch is given by the aftertouch threshold pad parameters. The value curve is linear to the pressure and in range 0 to 127. See Pad Parameters.

     In polyphonic key pressure mode, aftertouch for each pressed key is sent individually. The value is defined by the pad velocity curve and in range 1…​127. See Velocity Curve.
     */
    midiOutput.reset(MidiOutput::openDevice(deviceIndex));
    if (!midiOutput) {
        throw runtime_error("Failed to open MIDI output device");
    }
    unsigned char setAftertouchModeSysExCommand[7] = {0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01};
    MidiMessage sysExMessage = MidiMessage::createSysExMessage(setAftertouchModeSysExCommand, 7);

    midiOutput->sendMessageNow(sysExMessage);
    midiInput->start();
}

void Demo::setMidiInputCallback(const midicb_t &callback) {
    midiCallback = callback;
}

void Demo::handleIncomingMidiMessage(MidiInput * /*source*/, const MidiMessage &message) {
    if (midiCallback) {
        midiCallback(message);
    }
}

void Demo::timerCallback() {
    elapsed += 0.02f;
    //auto t1 = std::chrono::high_resolution_clock::now();
    drawFrame();
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
}

void Demo::drawFrame() {
    static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

    auto &g = bridge.getGraphics();
    g.fillAll(CLEAR_COLOR);

    const auto height = Push2Display::HEIGHT;
    const auto width = Push2Display::WIDTH;

    Path wavePath;

    const float waveStep = 10.0f;
    const float waveY = height * 0.44f;
    int i = 0;

    for (float x = waveStep * 0.5f; x < width; x += waveStep) {
        const float y1 = waveY + height * 0.10f * std::sin(i * 0.38f + elapsed);
        const float y2 = waveY + height * 0.20f * std::sin(i * 0.20f + elapsed * 2.0f);

        wavePath.addLineSegment(Line<float>(x, y1, x, y2), 2.0f);
        wavePath.addEllipse(x - waveStep * 0.3f, y1 - waveStep * 0.3f, waveStep * 0.6f, waveStep * 0.6f);
        wavePath.addEllipse(x - waveStep * 0.3f, y2 - waveStep * 0.3f, waveStep * 0.6f, waveStep * 0.6f);

        ++i;
    }

    g.setColour(juce::Colour::greyLevel(0.5f));
    g.fillPath(wavePath);

    auto logo = ImageCache::getFromMemory(BinaryData::PushStartup_png, BinaryData::PushStartup_pngSize);
    g.drawImageAt(logo, (width - logo.getWidth()) / 2, (height - logo.getHeight()) / 2);

    bridge.writeFrameToDisplay();
}
