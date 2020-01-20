# Smoke Tests

These smoke test cases are verified for every release (major, minor or patch).

**This is not an exhaustive list of all app behavior.**
It is an evolving set of cases representing a subset of application behavior verified to function exactly as documented _at the time of the last release tag_.

For example, to see the verified functionality for release `0.0.1`, check out the `0.0.1` tag and view the contents of this file.

## Audio and MIDI IO

* Loading application for the first time (no `<VALUE name="audioDeviceState">` tag in `~/Library/Preferences/sound-machine.settings`)
  - Default system audio input and output channels should be present
  - default project master gain processor should have L/R audio pins connected to default audio output pins
  - Loading with Push 2 connected and on shows "Ableton Push 2 Live Port" in midi input devices
  - Loading with Push 2 off and then turning on after app makes "Ableton Push 2 Live Port" appear in midi input devices
  - (No other midi inputs are automatically enabled)

* For both MIDI and audio devices:
  - Enabling another midi input in device preferences makes the input appear in the graph
  - Disabling a midi input in device preferences makes the input disappear in the graph
  - All available midi IO can be enabled and still fit on screen
  - Closing and re-opening application (without changing device connectivity) saves all midi device preferences
  - Enabling a connected midi device (both input and output), then disconnecting and reconnecting the device,
    restores the enabled preference and makes the device appear in the graph again (and persists through restart)
  - Enabling a connected midi device (both input and output), closing the app, disconnecting the enabled device,
    restarting the app, and then reconnecting the device after startup, should restore the enabled preference and
    make the device appear in the graph again

