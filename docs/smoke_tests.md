# Smoke Tests

These smoke test cases are verified for every release (major, minor or patch).

**This is not an exhaustive list of all app behavior.**
It is an evolving set of cases representing a subset of application behavior verified to function exactly as documented _at the time of the last release tag_.

For example, to see the verified functionality for release `0.0.1`, check out the `0.0.1` tag and view the contents of this file.

## Audio and MIDI IO device settings

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
  - Disabling a device while it is connected to a processor removes its connections
  - Undo/Redo works appropriately for device enablement/disablement
    * Undo after disabling device re-enables it (adds it to graph, etc)
    * Undo after enabling device disables it (removes it from graph, etc)
    * Undos/redos around automatic device removal as a result of disconnecting devices all works appropriately
      (IO processors show back up in the graph, but are greyed-out and don't do any processing.
      When devices are plugged back in, the grey-out is removed and they begin processing again, but this is not
      an undoable event.) (TODO this is mostly there, but need to add greying-out)

## Push 2 MIDI behavior

* When midi output pin of "Push 2 Live Port" input device is connected to any destination midi pin:
  - Push 2 enters note-mode and pads light up
  - Playing pads sends note data appropriately to destination processor
  - Only pads from Push 2 are sent to destination processor
    (ontrols such as turning a knob or pushing a non-pad control button are _not_ sent along)
  - Holding down a pad, then switching "Session" button on Push 2, disables note pads (entering session mode),
    but then releasing the pad still allows note-off messages to reach destination
  - Pressing a pad in session-mode does not send note data to destination processor
* When midi output pin of "Push 2 Live Port" input device is disconnected from its destination
  - Push 2 enters session-mode and pad lights turn off
