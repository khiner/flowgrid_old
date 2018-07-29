#include <Utilities.h>
#include "view/push2/Push2Component.h"
#include "MidiControlHandler.h"
#include "ApplicationKeyListener.h"
#include <view/ValueTreeEditor.h>
#include <view/graph_editor/GraphEditor.h>

File getSaveFile() {
    return File::getSpecialLocation(File::userDesktopDirectory).getChildFile("ValueTreeDemoEdit.xml");
}

class SoundMachineApplication : public JUCEApplication, public MenuBarModel {
public:
    SoundMachineApplication() : project(Utilities::loadValueTree(getSaveFile(), true), undoManager, processorManager, deviceManager),
                                applicationKeyListener(project, undoManager),
                                processorGraph(project, undoManager, deviceManager),
                                midiControlHandler(project, processorGraph, undoManager) {}

    const String getApplicationName() override { return ProjectInfo::projectName; }

    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String &) override {
        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            MessageManager::callAsync([this, message]() { midiControlHandler.handleControlMidi(message); });
        });

        graphEditorWindow = std::make_unique<MainWindow>("Graph Editor", new GraphEditor(processorGraph, project), &applicationKeyListener);
        std::unique_ptr<XmlElement> savedAudioState(processorManager.getApplicationProperties().getUserSettings()->getXmlValue("audioDeviceState"));
        deviceManager.initialise(256, 256, savedAudioState.get(), true);
        player.setProcessor(&processorGraph);
        deviceManager.addAudioCallback(&player);
        processorGraph.audioDeviceManagerInitialized();

        Process::makeForegroundProcess();
        auto *push2Component = new Push2Component(project, push2MidiCommunicator, processorGraph);
        push2Window = std::make_unique<MainWindow>("Push 2 Mirror", push2Component, &applicationKeyListener);
        auto *valueTreeEditor = new ValueTreeEditor(project.getState(), undoManager, project, processorGraph);
        treeWindow = std::make_unique<MainWindow>("Tree Editor", valueTreeEditor, &applicationKeyListener);


        audioDeviceSelectorComponent = std::make_unique<AudioDeviceSelectorComponent>(deviceManager, 0, 256, 0, 256,
                                                                                      true, true, true, false);
        audioDeviceSelectorComponent->setSize(600, 600);

        pluginListComponent = std::unique_ptr<PluginListComponent>(processorManager.makePluginListComponent());

        treeWindow->setBoundsRelative(0.05, 0.25, 0.45, 0.35);

        float graphEditorHeightToWidthRatio = float(Project::NUM_VISIBLE_PROCESSOR_SLOTS) / Project::NUM_VISIBLE_TRACKS;

        graphEditorWindow->setBoundsRelative(0.5, 0.1, 0.4, 0.5);
        graphEditorWindow->setSize(graphEditorWindow->getWidth(), int(graphEditorWindow->getWidth() * graphEditorHeightToWidthRatio));
        push2Window->setBounds(treeWindow->getPosition().x, treeWindow->getPosition().y - Push2Display::HEIGHT - graphEditorWindow->getTitleBarHeight(),
                               Push2Display::WIDTH, Push2Display::HEIGHT + graphEditorWindow->getTitleBarHeight());
        push2Window->setResizable(false, false);
        midiControlHandler.setPush2Component(push2Component);
        project.sendItemSelectedMessage(project.findFirstSelectedItem());

        setMacMainMenu(this);
    }

    void shutdown() override {
        push2Window = nullptr;
        treeWindow = nullptr;
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
            menu.addItem(2, "Edit the list of available plugins");

            const auto& pluginSortMethod = processorManager.getPluginSortMethod();

            PopupMenu sortTypeMenu;
            sortTypeMenu.addItem(200, "List plugins in default order",      true, pluginSortMethod == KnownPluginList::defaultOrder);
            sortTypeMenu.addItem(201, "List plugins in alphabetical order", true, pluginSortMethod == KnownPluginList::sortAlphabetically);
            sortTypeMenu.addItem(202, "List plugins by category",           true, pluginSortMethod == KnownPluginList::sortByCategory);
            sortTypeMenu.addItem(203, "List plugins by manufacturer",       true, pluginSortMethod == KnownPluginList::sortByManufacturer);
            sortTypeMenu.addItem(204, "List plugins based on the directory structure", true, pluginSortMethod == KnownPluginList::sortByFileSystemLocation);
            menu.addSubMenu ("Plugin menu type", sortTypeMenu);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override {
        if (topLevelMenuIndex == 0) { // "Options" menu
            if (menuItemID == 1) {
                showAudioSettings();
            } else if (menuItemID == 2) {
                showPluginList();
            } else if (menuItemID >= 200 && menuItemID < 210) {
                if (menuItemID == 200) processorManager.setPluginSortMethod(KnownPluginList::defaultOrder);
                else if (menuItemID == 201) processorManager.setPluginSortMethod(KnownPluginList::sortAlphabetically);
                else if (menuItemID == 202) processorManager.setPluginSortMethod(KnownPluginList::sortByCategory);
                else if (menuItemID == 203) processorManager.setPluginSortMethod(KnownPluginList::sortByManufacturer);
                else if (menuItemID == 204) processorManager.setPluginSortMethod(KnownPluginList::sortByFileSystemLocation);

                processorManager.getApplicationProperties().getUserSettings()->setValue("pluginSortMethod", (int) processorManager.getPluginSortMethod());
                menuItemsChanged();
            }
        }
    }

    void menuBarActivated(bool isActivated) override {}

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
            setBackgroundColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
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
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    ProcessorManager processorManager;

    std::unique_ptr<MainWindow> treeWindow, push2Window, graphEditorWindow;
    std::unique_ptr<AudioDeviceSelectorComponent> audioDeviceSelectorComponent;
    std::unique_ptr<PluginListComponent> pluginListComponent;

    UndoManager undoManager;
    AudioDeviceManager deviceManager;

    Push2MidiCommunicator push2MidiCommunicator;

    AudioProcessorPlayer player;

    Project project;
    ApplicationKeyListener applicationKeyListener;
    ProcessorGraph processorGraph;
    MidiControlHandler midiControlHandler;

    void showAudioSettings() {
        DialogWindow::LaunchOptions o;
        o.content.setNonOwned(audioDeviceSelectorComponent.get());
        o.dialogTitle = "Audio Settings";
        o.componentToCentreAround = treeWindow.get();
        o.dialogBackgroundColour = treeWindow->findColour(ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;

        auto *w = o.create();
        w->enterModalState(true, ModalCallbackFunction::create([this](int) {
            std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
            processorManager.getApplicationProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
            processorManager.getApplicationProperties().getUserSettings()->saveIfNeeded();
        }), true);
    }

    void showPluginList() {
        DialogWindow::LaunchOptions o;
        o.content.setNonOwned(pluginListComponent.get());
        o.dialogTitle = "Available Plugins";
        o.componentToCentreAround = graphEditorWindow.get();
        o.dialogBackgroundColour = graphEditorWindow->findColour(ResizableWindow::backgroundColourId);
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
