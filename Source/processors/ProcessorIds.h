#pragma once

#include "JuceHeader.h"
#include "GainProcessor.h"
#include "MixerChannelProcessor.h"
#include "SineBank.h"
#include "BalanceProcessor.h"
#include "InternalPluginFormat.h"

const static StringArray processorIdsWithoutMixer { GainProcessor::getIdentifier(), BalanceProcessor::getIdentifier(), SineBank::getIdentifier() };
static StringArray allProcessorIds { MixerChannelProcessor::getIdentifier(), GainProcessor::getIdentifier(), BalanceProcessor::getIdentifier(), SineBank::getIdentifier() };

static const StringArray getAvailableProcessorIdsForTrack(const ValueTree& track) {
    if (!track.isValid()) {
        return StringArray();
    } else if (track.getChildWithProperty(IDs::name, MixerChannelProcessor::getIdentifier()).isValid()) {
        // at most one MixerChannel per track
        return processorIdsWithoutMixer;
    } else {
        return allProcessorIds;
    }
}

static StatefulAudioProcessorWrapper *createStatefulAudioProcessorFromId(const String &id, const ValueTree &state, UndoManager &undoManager) {
    if (id == MixerChannelProcessor::getIdentifier()) {
        return new StatefulAudioProcessorWrapper(new MixerChannelProcessor(MixerChannelProcessor::getPluginDescription()), state, undoManager);
    } else if (id == GainProcessor::getIdentifier()) {
        return new StatefulAudioProcessorWrapper(new GainProcessor(GainProcessor::getPluginDescription()), state, undoManager);
    } else if (id == BalanceProcessor::getIdentifier()) {
        return new StatefulAudioProcessorWrapper(new BalanceProcessor(BalanceProcessor::getPluginDescription()), state, undoManager);
    } else if (id == SineBank::getIdentifier()) {
        return new StatefulAudioProcessorWrapper(new SineBank(SineBank::getPluginDescription()), state, undoManager);
    } else {
        return nullptr;
    }
}

class ProcessorIds : private ChangeListener {
public:
    void load(ApplicationProperties* appProperties) {
        this->appProperties = appProperties;

        InternalPluginFormat internalFormat;
        internalFormat.getAllTypes(internalTypes);
        std::unique_ptr<XmlElement> savedPluginList(appProperties->getUserSettings()->getXmlValue("pluginList"));

        if (savedPluginList != nullptr)
            knownPluginList.recreateFromXml(*savedPluginList);

        for (auto* pluginType : internalTypes)
            knownPluginList.addType(*pluginType);

        pluginSortMethod = (KnownPluginList::SortMethod) appProperties->getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByManufacturer);
        knownPluginList.addChangeListener(this);

        formatManager.addDefaultFormats();
        formatManager.addFormat(new InternalPluginFormat());
    }

    PluginListComponent* makePluginListComponent() {
        const File &deadMansPedalFile = appProperties->getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
        return new PluginListComponent(formatManager, knownPluginList, deadMansPedalFile, appProperties->getUserSettings(), true);
    }

    KnownPluginList& getKnownPluginList() {
        return knownPluginList;
    }

    const KnownPluginList::SortMethod getPluginSortMethod() const {
        return pluginSortMethod;
    }

    AudioPluginFormatManager& getFormatManager() {
        return formatManager;
    }

    void setPluginSortMethod(const KnownPluginList::SortMethod pluginSortMethod) {
        this->pluginSortMethod = pluginSortMethod;
    }

    void addPluginsToMenu(PopupMenu& m) const {
        int i = 0;
        for (auto* t : internalTypes)
            m.addItem (++i, t->name + " (" + t->pluginFormatName + ")", true);

        m.addSeparator();

        knownPluginList.addToMenu(m, pluginSortMethod);
    }

    const PluginDescription* getChosenType(const int menuId) const {
        if (menuId >= 1 && menuId < 1 + internalTypes.size())
            return internalTypes[menuId - 1];

        return knownPluginList.getType(knownPluginList.getIndexChosenByMenu(menuId));
    }

private:
    OwnedArray<PluginDescription> internalTypes;
    KnownPluginList knownPluginList;
    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;
    ApplicationProperties* appProperties;
    
    void changeListenerCallback(ChangeBroadcaster* changed) override {
        if (changed == &knownPluginList) {
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            std::unique_ptr<XmlElement> savedPluginList(knownPluginList.createXml());

            if (savedPluginList != nullptr && appProperties != nullptr) {
                appProperties->getUserSettings()->setValue("pluginList", savedPluginList.get());
                appProperties->saveIfNeeded();
            }
        }
    }
};
