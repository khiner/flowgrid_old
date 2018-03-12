#include "Push2Demo.h"
#include "chrono"

using namespace std;

namespace {
    bool SMatchSubStringNoCase(const std::string &haystack, const std::string &needle) {
        auto it = std::search(
                haystack.begin(), haystack.end(),
                needle.begin(), needle.end(),
                [](char ch1, char ch2)                // case insensitive
                {
                    return std::toupper(ch1) == std::toupper(ch2);
                });
        return it != haystack.end();
    }
}

Demo::Demo():  bridge() {
    // Initialises the midi input
    openMidiDevice();

    // Reset elapsed time
    elapsed = 0;

    // Start the timer to draw the animation
    startTimerHz(60);
}

void Demo::openMidiDevice() {
    // Look for an input device matching push 2
    auto devices = MidiInput::getDevices();
    int deviceIndex = -1;
    int index = 0;
    for (auto &device: devices) {
        if (SMatchSubStringNoCase(device.toStdString(), "ableton push 2")) {
            deviceIndex = index;
            break;
        }
        index++;
    }

    if (deviceIndex == -1) {
        throw runtime_error("Failed to find input midi device for push2");
    }

    // Try opening the device
    auto input = MidiInput::openDevice(deviceIndex, this);
    if (!input) {
        throw runtime_error("Failed to open input device");
    }

    /*
     https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Aftertouch
     In channel pressure mode (default), the pad with the highest pressure determines the value sent. The pressure range that produces aftertouch is given by the aftertouch threshold pad parameters. The value curve is linear to the pressure and in range 0 to 127. See Pad Parameters.

     In polyphonic key pressure mode, aftertouch for each pressed key is sent individually. The value is defined by the pad velocity curve and in range 1…​127. See Velocity Curve.
     */
    MidiOutput *midiOutput = MidiOutput::openDevice(deviceIndex);
    unsigned char setAftertouchModeSysExCommand[7] = {0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01};
    MidiMessage sysExMessage = MidiMessage::createSysExMessage(setAftertouchModeSysExCommand, 7);

    midiOutput->sendMessageNow(sysExMessage);
    // Store and starts listening to the device
    midiInput.reset(input);
    midiInput->start();
}


//------------------------------------------------------------------------------

void Demo::SetMidiInputCallback(const midicb_t &callback) {
    midiCallback = callback;
}


//------------------------------------------------------------------------------

void Demo::handleIncomingMidiMessage(MidiInput * /*source*/, const MidiMessage &message) {
    // if a callback has been set, forward the incoming message
    if (midiCallback) {
        midiCallback(message);
    }
}


//------------------------------------------------------------------------------

void Demo::timerCallback() {
    elapsed += 0.02f;
    //auto t1 = std::chrono::high_resolution_clock::now();
    drawFrame();
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
}


//------------------------------------------------------------------------------

void Demo::drawFrame() {
    static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

    auto &g = bridge.getGraphics();
    g.fillAll(CLEAR_COLOR);

    // Create a path for the animated wave
    const auto height = ableton::Push2Display::HEIGHT;
    const auto width = ableton::Push2Display::WIDTH;

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

    // Draw the path
    g.setColour(juce::Colour::greyLevel(0.5f));
    g.fillPath(wavePath);

    // Blit the logo on top
    auto logo = ImageCache::getFromMemory(BinaryData::PushStartup_png, BinaryData::PushStartup_pngSize);
    g.drawImageAt(logo, (width - logo.getWidth()) / 2, (height - logo.getHeight()) / 2);

    bridge.writeFrameToDisplay();
}
