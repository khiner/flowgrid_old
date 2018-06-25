#include <drow/drow_Utilities.h>
#include <drow/ValueTreeItems.h>
#include "JuceHeader.h"
#include "Push2Animator.h"
#include <ArrangeView.h>
#include <ValueTreesDemo.h>
#include <audio_processors/MainProcessor.h>
#include <view/InstrumentViewComponent.h>
#include <view/Push2ViewComponent.h>

File getSaveFile()
{
    return File::getSpecialLocation (File::userDesktopDirectory).getChildFile ("ValueTreeDemoEdit.xml");
}

ValueTree loadOrCreateDefaultEdit()
{
    ValueTree v (drow::loadValueTree (getSaveFile(), true));

    if (! v.isValid())
        v =  Helpers::createDefaultEdit();

    return v;
}

class SoundMachineApplication : public JUCEApplication {
public:
    SoundMachineApplication(): mainProcessor(2, 2), push2Animator(&push2ViewComponent) {}

    const String getApplicationName() override { return ProjectInfo::projectName; }

    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String &) override {
        player.setProcessor(&mainProcessor);
        deviceManager.addAudioCallback(&player);

        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            mainProcessor.handleControlMidi(message);
        });

        mainProcessor.setInstrument(IDs::SINE_BANK_INSTRUMENT);
        instrumentViewComponent.setInstrument(mainProcessor.getCurrentInstrument());
        push2ViewComponent.addAndMakeVisible(instrumentViewComponent);

        deviceManager.initialiseWithDefaultDevices(2, 2);
        Process::makeForegroundProcess();
        // This method is where you should put your application's initialisation code..
        push2Window = new MainWindow(getApplicationName(), &push2ViewComponent);
        editTree = loadOrCreateDefaultEdit();
        treeWindow = new MainWindow("Tree Editor", new ValueTreesDemo (editTree));
        arrangeWindow = new MainWindow("Overview", new ArrangeView (editTree));

        audioSetupWindow = new MainWindow("Audio Setup", new AudioDeviceSelectorComponent(deviceManager, 0, 256, 0, 256, true, true, true, false));

        treeWindow->setBoundsRelative(0.15, 0.25, 0.35, 0.35);
        arrangeWindow->setBoundsRelative(0.50, 0.25, 0.35, 0.35);
        audioSetupWindow->setBoundsRelative(0.15, 0.60, 0.35, 0.50);
        push2Window->setBounds(treeWindow->getPosition().x, treeWindow->getPosition().y - Push2Display::HEIGHT - 30, Push2Display::WIDTH, Push2Display::HEIGHT + 30);
    }

    void shutdown() override {
        // Add your application's shutdown code here..
        push2Window = nullptr; // (deletes our window)
        treeWindow = nullptr;
        audioSetupWindow = nullptr;
        arrangeWindow = nullptr;
        deviceManager.removeAudioCallback(&player);
        drow::saveValueTree (editTree, getSaveFile(), true);
    }

    void systemRequestedQuit() override {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted(const String &commandLine) override {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    /*
        This class implements the desktop window that contains an instance of
        our MainContentComponent class.
    */
    class MainWindow : public DocumentWindow {
    public:
        explicit MainWindow(const String &name, Component* contentComponent) : DocumentWindow(name,
                                                 Colours::lightgrey,
                                                 DocumentWindow::allButtons) {
            setContentOwned(contentComponent, true);
            setResizable(true, true);

            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            setBackgroundColour(backgroundColor);
        }

        void closeButtonPressed() override {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        const Colour backgroundColor = dynamic_cast<LookAndFeel_V4&>(LookAndFeel::getDefaultLookAndFeel()).getCurrentColourScheme().getUIColour((LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    ValueTree editTree;
    ScopedPointer<MainWindow> treeWindow, arrangeWindow, audioSetupWindow;
    ScopedPointer<MainWindow> push2Window;
    AudioDeviceManager deviceManager;

    Push2MidiCommunicator push2MidiCommunicator;

    MainProcessor mainProcessor;
    InstrumentViewComponent instrumentViewComponent;
    Push2ViewComponent push2ViewComponent;
    Push2Animator push2Animator;
    AudioProcessorPlayer player;

};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (SoundMachineApplication)
