#pragma once

#include "JuceHeader.h"
#include "GainProcessor.h"
#include "MixerChannelProcessor.h"
#include "SineBank.h"
#include "BalanceProcessor.h"
#include "InternalPluginFormat.h"

class ProcessorIds : private ChangeListener {
public:
    ProcessorIds() {
        PropertiesFile::Options options;
        options.applicationName = ProjectInfo::projectName;
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Preferences";
        appProperties.setStorageParameters(options);

        InternalPluginFormat internalFormat;
        internalFormat.getAllTypes(internalTypes);
        std::unique_ptr<XmlElement> savedPluginList(appProperties.getUserSettings()->getXmlValue("pluginList"));

        if (savedPluginList != nullptr)
            knownPluginList.recreateFromXml(*savedPluginList);

        for (auto* pluginType : internalTypes)
            knownPluginList.addType(*pluginType);

        pluginSortMethod = (KnownPluginList::SortMethod) appProperties.getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByManufacturer);
        knownPluginList.addChangeListener(this);

        formatManager.addDefaultFormats();
        formatManager.addFormat(new InternalPluginFormat());
    }

    ApplicationProperties& getApplicationProperties() {
        return appProperties;
    }

    PluginListComponent* makePluginListComponent() {
        const File &deadMansPedalFile = appProperties.getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
        return new PluginListComponent(formatManager, knownPluginList, deadMansPedalFile, appProperties.getUserSettings(), true);
    }

    PluginDescription *getTypeForIdentifier(const String &identifier) {
        return knownPluginList.getTypeForIdentifierString(identifier);
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

    const PluginDescription* getChosenType(const int menuId) const {
        if (menuId >= 1 && menuId < 1 + internalTypes.size())
            return internalTypes[menuId - 1];

        return knownPluginList.getType(knownPluginList.getIndexChosenByMenu(menuId));
    }

    const OwnedArray<PluginDescription>& getInternalTypes() const {
        return internalTypes;
    }

private:
    OwnedArray<PluginDescription> internalTypes;
    KnownPluginList knownPluginList;
    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;
    ApplicationProperties appProperties;

    void changeListenerCallback(ChangeBroadcaster* changed) override {
        if (changed == &knownPluginList) {
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            std::unique_ptr<XmlElement> savedPluginList(knownPluginList.createXml());

            if (savedPluginList != nullptr) {
                appProperties.getUserSettings()->setValue("pluginList", savedPluginList.get());
                appProperties.saveIfNeeded();
            }
        }
    }
};
