#include "LevelMeterSource.h"

/**
 This is called from the GUI. If processing was stalled, this will pump zeroes into the buffer,
 until the readings return to zero.
 */
void LevelMeterSource::decayIfNeeded() {
    int64 time = Time::currentTimeMillis();
    if (time - lastMeasurement > 100) {
        lastMeasurement = time;
        for (auto &level : levels) {
            level.setLevels(lastMeasurement, 0.0f, 0.0f, holdMSecs);
        }
    }
}