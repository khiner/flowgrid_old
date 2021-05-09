#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

using namespace juce;

class LevelMeterSource {
public:
    LevelMeterSource() : holdMSecs(500), lastMeasurement(0), suspended(false) {}

    ~LevelMeterSource() { masterReference.clear(); }

    /**
     Resize the meters data containers. Set the
     \param numChannels to the number of channels. If you don't do this in prepareToPlay,
            it will be done when calling measureBlock, but a few bytes will be allocated
            on the audio thread, so be aware.
     \param rmsWindow is the number of rms values to gather. Keep that aligned with
            the sampleRate and the blocksize to get reproducable results.
            e.g. `rmsWindow = msecs * 0.001f * sampleRate / blockSize;`
     \FIXME: don't call this when measureBlock is processing
     */
    void resize(int channels, int rmsWindow);

    void measureBlock(const AudioBuffer<float> &buffer) {
        lastMeasurement = Time::currentTimeMillis();
        if (!suspended) {
            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();

            levels.resize(numChannels);
            for (int channel = 0; channel < numChannels; ++channel) {
                levels[channel].setLevels(lastMeasurement,
                                          buffer.getMagnitude(channel, 0, numSamples),
                                          buffer.getRMSLevel(channel, 0, numSamples),
                                          holdMSecs);
            }
        }
    }

    /**
     This is called from the GUI. If processing was stalled, this will pump zeroes into the buffer,
     until the readings return to zero.
     */
    void decayIfNeeded();

    /**
     This is the max level as displayed by the little line above the RMS bar.
     It is reset by \see setMaxHoldMS.
     */
    float getMaxLevel(unsigned int channel) const { return levels[channel].max; }

    /**
     This is the RMS level that the bar will indicate. It is
     summed over rmsWindow number of blocks/measureBlock calls.
     */
    float getRMSLevel(unsigned int channel) const { return levels[channel].getAvgRMS(); }

    unsigned int getNumChannels() const { return static_cast<unsigned int>(levels.size()); }

private:
    class ChannelData {
    public:
        explicit ChannelData(unsigned long rmsWindow = 8) :
                max(), maxOverall(), clip(false), hold(0), rmsHistory(rmsWindow, 0.0), rmsSum(0.0), rmsPtr(0) {}

        ChannelData(const ChannelData &other) :
                max(other.max.load()), maxOverall(other.maxOverall.load()), clip(other.clip.load()),
                hold(other.hold.load()), rmsHistory(8, 0.0), rmsSum(0.0), rmsPtr(0) {}

        std::atomic<float> max;
        std::atomic<float> maxOverall;
        std::atomic<bool> clip;

        float getAvgRMS() const {
            if (!rmsHistory.empty()) return sqrtf(rmsSum / static_cast<float>(rmsHistory.size()));
            return sqrtf(rmsSum);
        }

        void setLevels(const int64 time, const float newMax, const float newRms, const int64 holdMSecs) {
            if (newMax > 1.0 || newRms > 1.0)
                clip = true;

            maxOverall = fmaxf(maxOverall, newMax);
            if (newMax >= max) {
                max = std::min(1.0f, newMax);
                hold = time + holdMSecs;
            } else if (time > hold) {
                max = std::min(1.0f, newMax);
            }
            pushNextRMS(std::min(1.0f, newRms));
        }

        void setRMSsize(const int numBlocks) {
            rmsHistory.assign(numBlocks, 0.0f);
            rmsSum = 0.0;
            numBlocks > 1 ? (rmsPtr %= rmsHistory.size()) : (rmsPtr = 0);
        }

    private:
        void pushNextRMS(const float newRMS) {
            const float squaredRMS = std::min(newRMS * newRMS, 1.0f);
            if (!rmsHistory.empty()) {
                rmsSum = rmsSum + squaredRMS - rmsHistory[rmsPtr];
                rmsHistory[rmsPtr] = squaredRMS;
                rmsPtr = (rmsPtr + 1) % rmsHistory.size();
            } else {
                rmsSum = squaredRMS;
            }
        }

        std::atomic<int64> hold;
        std::vector<float> rmsHistory;
        std::atomic<float> rmsSum;
        int rmsPtr;
    };

    WeakReference<LevelMeterSource>::Master masterReference;

    friend class WeakReference<LevelMeterSource>;

    std::vector<ChannelData> levels;
    int64 holdMSecs;
    std::atomic<int64> lastMeasurement;
    bool suspended;
};
