#pragma once

#include <utility>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"

class Push2ProcessorSelector : public Push2ComponentBase {
    class LabelWithUnderline : public Label {
    public:
        void setUnderline(bool underline) {
            this->underline = underline;
        }

        void paint(Graphics& g) override {
            Label::paint(g);
            if (underline) {
                g.setColour(findColour(textColourId));
                g.drawLine(8, getHeight() - 1, getWidth() - 8, getHeight() - 1);
            }
        }

    private:
        bool underline { false };
    };

    class ProcessorSelectorRow : public Component {
    public:
        explicit ProcessorSelectorRow(Project &project) : project(project) {
            for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
                auto *label = new LabelWithUnderline();
                addChildComponent(label);
                label->setJustificationType(Justification::centred);
                labels.add(label);
            }
        }

        KnownPluginList::PluginTree* selectFolder(int index) {
            if (currentTree == nullptr)
                return nullptr;
            if (index + currentViewOffset < currentTree->subFolders.size()) {
                auto *subfolder = currentTree->subFolders.getUnchecked(index + currentViewOffset);
                selectFolder(subfolder);
                return subfolder;
            }

            return nullptr;
        }

        void selectFolder(KnownPluginList::PluginTree* subfolder) {
            jassert(currentTree->subFolders.contains(subfolder));

            for (auto* folder : currentTree->subFolders) {
                folder->selected = false;
            }
            subfolder->selected = true;
            updateLabels();
        }

        const PluginDescription* selectProcessor(int index) {
            if (currentTree == nullptr)
                return nullptr;

            if (index + currentViewOffset >= currentTree->subFolders.size()) {
                index -= currentTree->subFolders.size();
                if (index + currentViewOffset < currentTree->plugins.size() && labels[index]->isEnabled()) {
                    return currentTree->plugins.getUnchecked(index + currentViewOffset);
                }
            }

            return nullptr;
        }

        void setCurrentTree(KnownPluginList::PluginTree* tree) {
            if (currentTree == tree)
                return;

            if (currentTree != nullptr && currentTree->subFolders.contains(tree)) {
                for (auto* folder : currentTree->subFolders) {
                    folder->selected = false;
                }
            }
            currentTree = tree;
            if (currentTree != nullptr)
                currentTree->selected = true;
            setCurrentViewOffset(0);
        }

        void setSelected(bool selected) {
            this->selected = selected;
            repaint();
        }

        int getCurrentViewOffset() {
            return currentViewOffset;
        }

        void setCurrentViewOffset(int currentViewOffset) {
            this->currentViewOffset = currentViewOffset;
            updateLabels();
        }

        KnownPluginList::PluginTree* findSelectedSubfolder() const {
            if (currentTree == nullptr)
                return nullptr;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_ITEMS_PER_ROW); i++) {
                if (i + currentViewOffset < currentTree->subFolders.size()) {
                    auto *subfolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
                    if (subfolder->selected)
                        return subfolder;
                }
            }
            if (currentViewOffset < currentTree->subFolders.size()) {
                // default to first subfolder in view
                return currentTree->subFolders.getUnchecked(currentViewOffset);
            }

            return nullptr;
        }
        
        LabelWithUnderline* findSelectedLabel() const {
            auto* selectedSubfolder = findSelectedSubfolder();
            if (selectedSubfolder != nullptr)
                return labels.getUnchecked(currentTree->subFolders.indexOf(selectedSubfolder) - currentViewOffset);

            return nullptr;
        }

        void arrowPressed(Direction direction) {
            if (direction == Direction::left)
                setCurrentViewOffset(jmax(0, currentViewOffset - NUM_ITEMS_PER_ROW));
            else if (direction == Direction::right && currentTree != nullptr)
                setCurrentViewOffset(jmin(currentViewOffset + NUM_ITEMS_PER_ROW,
                        getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_ITEMS_PER_ROW));
        }

        int getTotalNumberOfTreeItems() const {
            return currentTree != nullptr ? currentTree->subFolders.size() + currentTree->plugins.size() : 0;
        }

        void paint(Graphics& g) override {
            g.fillAll(selected ? Colours::white.withAlpha(0.07f) : Colours::transparentBlack);
        }

        void resized() override {
            auto r = getLocalBounds();
            for (auto *label : labels) {
                label->setBounds(r.removeFromLeft(getWidth() / NUM_ITEMS_PER_ROW));
            }
        }

        KnownPluginList::PluginTree* currentTree{};

        OwnedArray<LabelWithUnderline> labels;
    private:
        int currentViewOffset { 0 };
        bool selected { false };
        Project &project;

        void updateLabels() {
            const bool trackHasMixerAlready = project.selectedTrackHasMixerChannel();

            for (int i = 0; i < labels.size(); i++) {
                Label *label = labels.getUnchecked(i);
                label->setColour(Label::ColourIds::backgroundColourId, Colours::transparentBlack);
                label->setVisible(false);
            }

            if (currentTree == nullptr)
                return;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_ITEMS_PER_ROW); i++) {
                LabelWithUnderline *label = labels.getUnchecked(i);
                labels[i]->setVisible(true);
                if (i + currentViewOffset < currentTree->subFolders.size()) {
                    KnownPluginList::PluginTree *subFolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
                    label->setText(subFolder->folder, dontSendNotification);
                    label->setUnderline(true);
                } else if (i + currentViewOffset < getTotalNumberOfTreeItems()) {
                    const PluginDescription *plugin = currentTree->plugins.getUnchecked(i + currentViewOffset - currentTree->subFolders.size());
                    label->setText(plugin->name, dontSendNotification);
                    label->setEnabled(!trackHasMixerAlready || plugin->name != MixerChannelProcessor::name());
                    label->setUnderline(false);
                }
            }
            if (auto *selectedLabel = findSelectedLabel()) {
                selectedLabel->setColour(Label::ColourIds::backgroundColourId, Colours::white.withAlpha(0.08f));
            }
        }
    };

public:
    Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        topProcessorSelector = std::make_unique<ProcessorSelectorRow>(project);
        bottomProcessorSelector = std::make_unique<ProcessorSelectorRow>(project);
        addChildComponent(topProcessorSelector.get());
        addChildComponent(bottomProcessorSelector.get());

        rootTree = KnownPluginList::PluginTree();

        KnownPluginList::PluginTree *internalPluginTree = project.getUserCreatablePluginListInternal().createTree(project.getPluginSortMethod());
        internalPluginTree->folder = "Internal";
        KnownPluginList::PluginTree *externalPluginTree = project.getPluginListExternal().createTree(project.getPluginSortMethod());
        externalPluginTree->folder = "External";
        internalPluginTree->parent = &rootTree;
        externalPluginTree->parent = &rootTree;

        rootTree.subFolders.add(internalPluginTree);
        rootTree.subFolders.add(externalPluginTree);

        setCurrentTree(topProcessorSelector.get(), &rootTree);
        selectProcessorRow(topProcessorSelector.get());
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        topProcessorSelector->setVisible(visible);
        bottomProcessorSelector->setVisible(visible);
        
        if (visible) {
            updateEnabledPush2Buttons();
            updateEnabledPush2Arrows();
        }
    }

    const PluginDescription* selectTopProcessor(int index) {
        if (const auto *description = topProcessorSelector->selectProcessor(index))
            return description;

        if (auto* subfolder = topProcessorSelector->selectFolder(index)) {
            setCurrentTree(bottomProcessorSelector.get(), subfolder);
            selectProcessorRow(bottomProcessorSelector.get());
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    const PluginDescription* selectBottomProcessor(int index) {
        if (const auto *description = bottomProcessorSelector->selectProcessor(index))
            return description;

        if (auto* subfolder = bottomProcessorSelector->selectFolder(index)) {
            setCurrentTree(topProcessorSelector.get(), bottomProcessorSelector->currentTree);
            topProcessorSelector->setCurrentViewOffset(bottomProcessorSelector->getCurrentViewOffset());
            setCurrentTree(bottomProcessorSelector.get(), subfolder);
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectTopProcessor(buttonIndex)) {
            project.createAndAddProcessor(*selectedProcessor);
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectBottomProcessor(buttonIndex)) {
            project.createAndAddProcessor(*selectedProcessor);
        }
    }

    void arrowPressed(Direction direction) override {
        if (!canNavigateInDirection(direction))
            return;
        if (currentProcessorSelector != nullptr && (direction == Direction::left || direction == Direction::right)) {
            currentProcessorSelector->arrowPressed(direction);
        } else if (direction == Direction::down) {
            if (auto* selectedLabel = currentProcessorSelector->findSelectedLabel()) {
                int selectedIndex = currentProcessorSelector->labels.indexOf(selectedLabel);
                if (currentProcessorSelector == topProcessorSelector.get()) {
                    selectTopProcessor(selectedIndex);
                } else if (currentProcessorSelector == bottomProcessorSelector.get()) {
                    selectBottomProcessor(selectedIndex);
                }
            }
        } else if (direction == Direction::up) {
            if (currentProcessorSelector == bottomProcessorSelector.get()) {
                selectProcessorRow(topProcessorSelector.get());
            } else if (auto* parent = currentProcessorSelector->currentTree->parent) {
                setCurrentTree(bottomProcessorSelector.get(), topProcessorSelector->currentTree);
                bottomProcessorSelector->setCurrentViewOffset(topProcessorSelector->getCurrentViewOffset());
                bottomProcessorSelector->selectFolder(topProcessorSelector->findSelectedSubfolder());
                setCurrentTree(topProcessorSelector.get(), parent);
            }
        }
        updateEnabledPush2Arrows();
    }

    void resized() override {
        auto r = getLocalBounds();
        topProcessorSelector->setBounds(r.removeFromTop(30));
        bottomProcessorSelector->setBounds(r.removeFromBottom(30));
    }
private:
    static const int NUM_ITEMS_PER_ROW = 8;

    void setCurrentTree(ProcessorSelectorRow *processorSelectorRow, KnownPluginList::PluginTree* tree) {
        processorSelectorRow->setCurrentTree(tree);
        updateEnabledPush2Buttons();
    }

    void selectProcessorRow(ProcessorSelectorRow* processorSelectorRow) {
        if (currentProcessorSelector != nullptr) {
            currentProcessorSelector->setSelected(false);
        }
        if (processorSelectorRow) {
            currentProcessorSelector = processorSelectorRow;
            currentProcessorSelector->setSelected(true);
        }
    }

    void updateEnabledPush2Buttons() {
        if (!isVisible())
            return;
        if (topProcessorSelector != nullptr) {
            for (int i = 0; i < topProcessorSelector->labels.size(); i++){
                push2MidiCommunicator.setAboveScreenButtonEnabled(i, topProcessorSelector->labels.getUnchecked(i)->isVisible());
            }
        }
        if (bottomProcessorSelector != nullptr) {
            for (int i = 0; i < bottomProcessorSelector->labels.size(); i++){
                push2MidiCommunicator.setBelowScreenButtonEnabled(i, bottomProcessorSelector->labels.getUnchecked(i)->isVisible());
            }
        }
    }

    void updateEnabledPush2Arrows() {
        if (!isVisible())
            return;
        if (currentProcessorSelector == nullptr) {
            push2MidiCommunicator.setAllArrowButtonsEnabled(false);
        } else {
            for (Direction direction : Push2MidiCommunicator::directions) {
                push2MidiCommunicator.setArrowButtonEnabled(direction, canNavigateInDirection(direction));
            }
        }
    }

    bool canNavigateInDirection(Direction direction) {
        switch (direction) {
            case Direction::right: return canNavigateRight();
            case Direction::down: return canNavigateDown();
            case Direction::left: return canNavigateLeft();
            case Direction::up: return canNavigateUp();
        }
    }

    bool canNavigateRight() const { return currentProcessorSelector->getCurrentViewOffset() + NUM_ITEMS_PER_ROW < currentProcessorSelector->getTotalNumberOfTreeItems(); }
    bool canNavigateDown() const { return currentProcessorSelector->findSelectedLabel() != nullptr; }
    bool canNavigateLeft() const { return currentProcessorSelector->getCurrentViewOffset() > 0; }
    bool canNavigateUp() const { return bottomProcessorSelector.get() == currentProcessorSelector || currentProcessorSelector->currentTree->parent != nullptr; }

    KnownPluginList::PluginTree rootTree;
    std::unique_ptr<ProcessorSelectorRow> topProcessorSelector;
    std::unique_ptr<ProcessorSelectorRow> bottomProcessorSelector;
    
    ProcessorSelectorRow* currentProcessorSelector{};
};
