#include <Utilities.h>
#include "view/push2/Push2Component.h"
#include "MidiControlHandler.h"
#include "ApplicationKeyListener.h"
#include <view/ArrangeView.h>
#include <view/ValueTreeEditor.h>
#include <view/GraphEditorPanel.h>

File getSaveFile() {
    return File::getSpecialLocation(File::userDesktopDirectory).getChildFile("ValueTreeDemoEdit.xml");
}

ValueTree loadOrCreateDefaultEdit() {
    return Utilities::loadValueTree(getSaveFile(), true);
}

class SoundMachineApplication : public JUCEApplication, public MenuBarModel {
public:
    SoundMachineApplication() : project(loadOrCreateDefaultEdit(), undoManager),
                                processorGraph(project, undoManager),
                                midiControlHandler(project, processorGraph, undoManager),
                                applicationKeyListener(treeView, undoManager) {}

    const String getApplicationName() override { return ProjectInfo::projectName; }

    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String &) override {
        player.setProcessor(&processorGraph);
        deviceManager.addAudioCallback(&player);

        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            MessageManager::callAsync([this, message]() { midiControlHandler.handleControlMidi(message); });
        });
        deviceManager.initialiseWithDefaultDevices(2, 2);

        Process::makeForegroundProcess();
        auto *push2Component = new Push2Component(project, processorGraph);
        push2Window = std::make_unique<MainWindow>("Push 2 Mirror", push2Component, &applicationKeyListener);
        ValueTreeEditor *valueTreeEditor = new ValueTreeEditor(treeView, project.getState(), undoManager, project, processorGraph);
        treeWindow = std::make_unique<MainWindow>("Tree Editor", valueTreeEditor, &applicationKeyListener);
        arrangeWindow = std::make_unique<MainWindow>("Arrange View", new ArrangeView(project.getTracks()), &applicationKeyListener);
        graphEditorWindow = std::make_unique<MainWindow>("Graph Editor", new GraphEditorPanel(processorGraph), &applicationKeyListener);

        audioDeviceSelectorComponent = std::make_unique<AudioDeviceSelectorComponent>(deviceManager, 0, 256, 0, 256,
                                                                                      true, true, true, false);
        audioDeviceSelectorComponent->setSize(600, 600);

        treeWindow->setBoundsRelative(0.05, 0.3, 0.45, 0.35);
        arrangeWindow->setBoundsRelative(0.05, 0.65, 0.45, 0.35);
        graphEditorWindow->setBoundsRelative(0.5, 0.3, 0.5, 0.7);
        push2Window->setBounds(treeWindow->getPosition().x, treeWindow->getPosition().y - Push2Display::HEIGHT - graphEditorWindow->getTitleBarHeight(),
                               Push2Display::WIDTH, Push2Display::HEIGHT + graphEditorWindow->getTitleBarHeight());
        push2Window->setResizable(false, false);
        midiControlHandler.setPush2Component(push2Component);
        valueTreeEditor->sendSelectMessageForFirstSelectedItem();

        setMacMainMenu(this);
    }

    void shutdown() override {
        push2Window = nullptr;
        treeWindow = nullptr;
        arrangeWindow = nullptr;
        deviceManager.removeAudioCallback(&player);
        Utilities::saveValueTree(project.getState(), getSaveFile(), true);
        setMacMainMenu(nullptr);
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

    StringArray getMenuBarNames() override {
        StringArray names;
        names.add("Options");
        return names;
    }

    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String & /*menuName*/) override {
        PopupMenu menu;

        // TODO use ApplicationCommand stuff like in plugin host example
        if (topLevelMenuIndex == 0) { // "Options" menu
            menu.addItem(1, "Change the audio device settings");
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override {
        if (topLevelMenuIndex == 0) { // "Options" menu
            if (menuItemID == 1) {
                showAudioSettings();
            }
        }
    }

    void menuBarActivated(bool isActivated) override {
    }

    /*
        This class implements the desktop window that contains an instance of
        our MainContentComponent class.
    */
    class MainWindow : public DocumentWindow {
    public:
        explicit MainWindow(const String &name, Component *contentComponent, KeyListener *keyListener) : DocumentWindow(name,
                                                                                              Colours::lightgrey,
                                                                                              DocumentWindow::allButtons) {
            contentComponent->setSize(1, 1); // nonzero size to avoid warnings
            setContentOwned(contentComponent, true);
            setResizable(true, true);

            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            setBackgroundColour(backgroundColor);
            addKeyListener(keyListener);
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
        const Colour backgroundColor = dynamic_cast<LookAndFeel_V4 &>(LookAndFeel::getDefaultLookAndFeel()).getCurrentColourScheme().getUIColour(
                (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> treeWindow, arrangeWindow, push2Window, graphEditorWindow;
    std::unique_ptr<AudioDeviceSelectorComponent> audioDeviceSelectorComponent;
    UndoManager undoManager;
    AudioDeviceManager deviceManager;
    TreeView treeView;
    ApplicationKeyListener applicationKeyListener;

    Push2MidiCommunicator push2MidiCommunicator;

    AudioProcessorPlayer player;

    Project project;
    ProcessorGraph processorGraph;
    MidiControlHandler midiControlHandler;

    void showAudioSettings() {
        DialogWindow::LaunchOptions o;
        o.content.setNonOwned(audioDeviceSelectorComponent.get());
        o.dialogTitle = "Audio Settings";
        o.componentToCentreAround = treeWindow.get();
        o.dialogBackgroundColour = treeWindow->getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;

        auto *w = o.create();
        // TODO handle load/save of audio settings state in callback (like plugin host example)
        w->enterModalState(true, ModalCallbackFunction::create([this](int) {}), true);
    }
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (SoundMachineApplication)
