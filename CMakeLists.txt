# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
# and https://github.com/juce-framework/JUCE/blob/master/examples/CMake/GuiApp/CMakeLists.txt
# for information on `juce` functions.

cmake_minimum_required(VERSION 3.12)
project(FlowGrid VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(modules/JUCE)

juce_set_vst2_sdk_path($ENV{HOME}/SDKs/VST_SDK/VST2_SDK)

juce_add_gui_app(FlowGrid
    BUNDLE_ID com.odangludo.flowgrid
    MICROPHONE_PERMISSION_ENABLED TRUE
    DOCUMENT_BROWSER_ENABLED TRUE
    DOCUMENT_EXTENSIONS smp
    COMPANY_NAME "Karl Hiner"
    COMPANY_WEBSITE www.karlhiner.com
    COMPANY_EMAIL karl.hiner@gmail.com
    PRODUCT_NAME FlowGrid
    PLUGINHOST_AU TRUE
)

juce_generate_juce_header(FlowGrid)

target_include_directories(FlowGrid PRIVATE
    Source
    modules/libusb/libusb
)

target_sources(FlowGrid PRIVATE
    Source/Main.cpp
    Source/processors/DefaultAudioProcessor.cpp
    Source/usb/libusb/libusb_platform_wrapper.c
)

target_compile_definitions(FlowGrid PRIVATE
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_gui_app` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_gui_app` call
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_VST3=1
)

juce_add_binary_data(FlowGridBinaryData SOURCES
    Resources/AbletonSansBold-Regular.otf
    Resources/AbletonSansLight-Regular.otf
    Resources/AbletonSansMedium-Regular.otf
    Resources/Audio.png
    Resources/CircleWithLineDiagonal.png
    Resources/CircleWithLineHorizontal.png
    Resources/CircleWithLineVertical.png
    Resources/Midi.png
    Resources/PushStartup.png
)

target_link_libraries(FlowGrid
    PRIVATE
    FlowGridBinaryData
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
