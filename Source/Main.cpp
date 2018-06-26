#include <drow/drow_Utilities.h>
#include <drow/ValueTreeItems.h>
#include "JuceHeader.h"
#include "Push2Animator.h"
#include "AudioGraphBuilder.h"
#include "MidiControlHandler.h"
#include <ArrangeView.h>
#include <ValueTreesDemo.h>
#include <push2/Push2MidiCommunicator.h>

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
    SoundMachineApplication(): editTree(loadOrCreateDefaultEdit()), audioGraphBuilder(editTree, undoManager), midiControlHandler(audioGraphBuilder, undoManager) {}

    const String getApplicationName() override { return ProjectInfo::projectName; }

    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String &) override {
        player.setProcessor(audioGraphBuilder.getMainAudioProcessor());
        deviceManager.addAudioCallback(&player);

        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            midiControlHandler.handleControlMidi(message);
        });

        deviceManager.initialiseWithDefaultDevices(2, 2);

        Process::makeForegroundProcess();
        push2Window = std::make_unique<MainWindow>("Push 2 Mirror", new Push2Animator(audioGraphBuilder));
        treeWindow = std::make_unique<MainWindow>("Tree Editor", new ValueTreesDemo (editTree, undoManager, audioGraphBuilder));
        arrangeWindow = std::make_unique<MainWindow>("Arrange View", new ArrangeView (editTree));

        auto *audioDeviceSelectorComponent = new AudioDeviceSelectorComponent(deviceManager, 0, 256, 0, 256, true, true, true, false);
        audioDeviceSelectorComponent->setBoundsRelative(0, 0, 100, 100); // needs some nonzero initial size or warnings are thrown
        audioSetupWindow = std::make_unique<MainWindow>("Audio Setup", audioDeviceSelectorComponent);

        treeWindow->setBoundsRelative(0.15, 0.25, 0.35, 0.35);
        arrangeWindow->setBoundsRelative(0.50, 0.25, 0.35, 0.35);
        audioSetupWindow->setBoundsRelative(0.15, 0.60, 0.35, 0.50);
        push2Window->setBounds(treeWindow->getPosition().x, treeWindow->getPosition().y - Push2Display::HEIGHT - 30, Push2Display::WIDTH, Push2Display::HEIGHT + 30);
    }

    void shutdown() override {
        push2Window = nullptr;
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
    std::unique_ptr<MainWindow> treeWindow, arrangeWindow, audioSetupWindow, push2Window;
    UndoManager undoManager;
    AudioDeviceManager deviceManager;

    Push2MidiCommunicator push2MidiCommunicator;

    AudioProcessorPlayer player;

    ValueTree editTree;
    AudioGraphBuilder audioGraphBuilder;
    MidiControlHandler midiControlHandler;
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (SoundMachineApplication)
