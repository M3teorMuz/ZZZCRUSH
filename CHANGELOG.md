# Changelog

## 1.1.0

- Replaced the JUCE FetchContent Git clone with a pinned JUCE 7.0.12 release archive.
- Enabled the CMake MSVC runtime policy before configuring the project.
- Added an explicit CMake source path and a Windows x64 VST3 output verification step to GitHub Actions.
- Smoothed Crush, Bit Depth, Sample Rate, Dry/Wet, Output and Bypass changes in the audio thread.
- Made `CRUSH = 0%` transparent and reset the sample-hold state to prevent stale-sample bursts when re-enabling the effect.
- Reworked the rotary controls, panel contrast, value displays and small interface details while preserving the original press/character animation.
- Added tooltips, local Windows build helper and expanded Russian installation instructions.
