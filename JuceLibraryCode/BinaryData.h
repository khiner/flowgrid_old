/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   AbletonSansBoldRegular_otf;
    const int            AbletonSansBoldRegular_otfSize = 43920;

    extern const char*   AbletonSansLightRegular_otf;
    const int            AbletonSansLightRegular_otfSize = 42784;

    extern const char*   AbletonSansMediumRegular_otf;
    const int            AbletonSansMediumRegular_otfSize = 43100;

    extern const char*   Audio_png;
    const int            Audio_pngSize = 3038;

    extern const char*   CircleWithLineDiagonal_png;
    const int            CircleWithLineDiagonal_pngSize = 27723;

    extern const char*   CircleWithLineHorizontal_png;
    const int            CircleWithLineHorizontal_pngSize = 27197;

    extern const char*   CircleWithLineVertical_png;
    const int            CircleWithLineVertical_pngSize = 30564;

    extern const char*   Midi_png;
    const int            Midi_pngSize = 825;

    extern const char*   PushStartup_png;
    const int            PushStartup_pngSize = 16112;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 9;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
