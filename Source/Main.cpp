#include <Utilities.h>
#include "view/push2/Push2Component.h"
#include "MidiControlHandler.h"
#include "ApplicationKeyListener.h"
#include <view/ArrangeView.h>
#include <view/ValueTreeEditor.h>
#include <view/GraphEditorPanel.h>
#include <memory>
#include <processors/InternalPluginFormat.h>

File getSaveFile() {
    return File::getSpecialLocation(File::userDesktopDirectory).getChildFile("ValueTreeDemoEdit.xml");
}

class SoundMachineApplication : public JUCEApplication, public MenuBarModel, private ChangeListener {
public:
    SoundMachineApplication() : project(Utilities::loadValueTree(getSaveFile(), true), undoManager, processorIds),
                                applicationKeyListener(project, undoManager),
                                processorGraph(project, undoManager),
                                midiControlHandler(project, processorGraph, undoManager) {}

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
        ValueTreeEditor *valueTreeEditor = new ValueTreeEditor(project.getState(), undoManager, project, processorGraph);
        treeWindow = std::make_unique<MainWindow>("Tree Editor", valueTreeEditor, &applicationKeyListener);
        arrangeWindow = std::make_unique<MainWindow>("Arrange View", new ArrangeView(project.getTracks()), &applicationKeyListener);
        graphEditorWindow = std::make_unique<MainWindow>("Graph Editor", new GraphEditorPanel(processorGraph, project), &applicationKeyListener);

        audioDeviceSelectorComponent = std::make_unique<AudioDeviceSelectorComponent>(deviceManager, 0, 256, 0, 256,
                                                                                      true, true, true, false);
        audioDeviceSelectorComponent->setSize(600, 600);

        pluginListComponent = std::unique_ptr<PluginListComponent>(processorIds.makePluginListComponent());

        treeWindow->setBoundsRelative(0.05, 0.25, 0.45, 0.35);
        arrangeWindow->setBoundsRelative(0.05, 0.6, 0.45, 0.35);

        float graphEditorHeightToWidthRatio = 9.0f / 8.0f;

        graphEditorWindow->setBoundsRelative(0.5, 0.25, 0.4, 0.5);
        graphEditorWindow->setSize(graphEditorWindow->getWidth(), int(graphEditorWindow->getWidth() * graphEditorHeightToWidthRatio + arrangeWindow->getTitleBarHeight()));
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
            menu.addItem(2, "Edit the list of available plugins");

            const auto& pluginSortMethod = processorIds.getPluginSortMethod();

            PopupMenu sortTypeMenu;
            sortTypeMenu.addItem (200, "List plugins in default order",      true, pluginSortMethod == KnownPluginList::defaultOrder);
            sortTypeMenu.addItem (201, "List plugins in alphabetical order", true, pluginSortMethod == KnownPluginList::sortAlphabetically);
            sortTypeMenu.addItem (202, "List plugins by category",           true, pluginSortMethod == KnownPluginList::sortByCategory);
            sortTypeMenu.addItem (203, "List plugins by manufacturer",       true, pluginSortMethod == KnownPluginList::sortByManufacturer);
            sortTypeMenu.addItem (204, "List plugins based on the directory structure", true, pluginSortMethod == KnownPluginList::sortByFileSystemLocation);
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
                if (menuItemID == 200) processorIds.setPluginSortMethod(KnownPluginList::defaultOrder);
                else if (menuItemID == 201) processorIds.setPluginSortMethod(KnownPluginList::sortAlphabetically);
                else if (menuItemID == 202) processorIds.setPluginSortMethod(KnownPluginList::sortByCategory);
                else if (menuItemID == 203) processorIds.setPluginSortMethod(KnownPluginList::sortByManufacturer);
                else if (menuItemID == 204) processorIds.setPluginSortMethod(KnownPluginList::sortByFileSystemLocation);

                processorIds.getApplicationProperties().getUserSettings()->setValue("pluginSortMethod", (int) processorIds.getPluginSortMethod());
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
    ProcessorIds processorIds;

    std::unique_ptr<MainWindow> treeWindow, arrangeWindow, push2Window, graphEditorWindow;
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
        // TODO handle load/save of audio settings state in callback (like plugin host example)
        w->enterModalState(true, ModalCallbackFunction::create([this](int) {}), true);
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

    void changeListenerCallback(ChangeBroadcaster* changed) override {
        if (changed == &processorIds.getKnownPluginList()) {
            menuItemsChanged();
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            std::unique_ptr<XmlElement> savedPluginList(processorIds.getKnownPluginList().createXml());

            if (savedPluginList != nullptr) {
                processorIds.getApplicationProperties().getUserSettings()->setValue("pluginList", savedPluginList.get());
                processorIds.getApplicationProperties().saveIfNeeded();
            }
        }
    }
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (SoundMachineApplication)
