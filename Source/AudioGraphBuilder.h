#pragma once

#include <drow/drow_Utilities.h>
#include <drow/drow_ValueTreeObjectList.h>
#include <drow/Identifiers.h>

struct AudioGraphClasses {
    struct AudioTrack : public drow::ValueTreePropertyChangeListener {
        explicit AudioTrack (ValueTree v) : state (v) {
            state.addListener(this);
        }

        ValueTree state;

    private:
        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override {
            if (v == state) {
                if (i == IDs::name || i == IDs::colour) {
                }
            }
        }
    };

//==============================================================================
    struct AudioTrackList : public drow::ValueTreeObjectList<AudioTrack> {
        explicit AudioTrackList (ValueTree editTree)
                : drow::ValueTreeObjectList<AudioTrack> (editTree) {
            rebuildObjects();
        }

        ~AudioTrackList() override {
            freeObjects();
        }

        bool isSuitableType (const juce::ValueTree& v) const override {
            return v.hasType (IDs::TRACK);
        }

        AudioTrack* createNewObject (const juce::ValueTree& v) override {
            auto* at = new AudioTrack(v);
            return at;
        }

        void deleteObject (AudioTrack* at) override {
            delete at;
        }

        void newObjectAdded (AudioTrack*) override { }
        void objectRemoved (AudioTrack*) override { }
        void objectOrderChanged() override { }
    };
};

class AudioGraphBuilder {
public:
    explicit AudioGraphBuilder (ValueTree editToUse) {
        trackList = std::make_unique<AudioGraphClasses::AudioTrackList> (editToUse);
    }

private:
    std::unique_ptr<AudioGraphClasses::AudioTrackList> trackList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)
};
