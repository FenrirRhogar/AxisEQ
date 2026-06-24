#pragma once

#include <JuceHeader.h>
#include <vector>

namespace OmniQ
{

/// Per-band parameter data for a preset — plain POD, easy to construct.
struct PresetBand
{
    bool  active       = false;
    bool  bypassed     = false;
    float freq         = 1000.0f;
    float gain         = 0.0f;
    float q            = 0.707f;
    int   type         = 0;   // FilterType int
    int   slope        = 1;   // FilterSlope int (12 dB/oct)
    int   route        = 0;   // RoutingMode int (Stereo)
    bool  dynEnabled   = false;
    bool  dynExtSc     = false;
    float dynThreshold = -20.0f;
    float dynRange     = 0.0f;
    float dynAttack    = 10.0f;
    float dynRelease   = 100.0f;
};

/// A single named preset.
struct Preset
{
    juce::String name;
    juce::String category;

    // One entry per MaxBands (24).  Bands beyond the defined list are inactive.
    std::vector<PresetBand> bands;
};

/// Manages factory and user presets.
/// Factory presets are hard-coded in the .cpp.
/// User presets are stored as .xml files in ~/Documents/OmniQ/Presets/.
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    // ─── Preset list ─────────────────────────────────────────────────────────
    int            getNumPresets()     const noexcept { return static_cast<int>(allPresets.size()); }
    const Preset&  getPreset(int idx)  const          { return allPresets.at(static_cast<size_t>(idx)); }
    int            getCurrentIndex()   const noexcept { return currentIndex; }
    juce::StringArray getAllPresetNames() const;

    // ─── Load / Save ─────────────────────────────────────────────────────────
    void loadPreset(int index);
    void loadNextPreset();
    void loadPreviousPreset();

    /// Capture the current APVTS state and store it as a user preset.
    void saveCurrentAsUserPreset(const juce::String& name,
                                 const juce::String& category = "User");

    /// Delete a user preset (factory presets are protected).
    void deleteUserPreset(int index);

    // ─── Persistence ─────────────────────────────────────────────────────────
    void scanUserPresetsFromDisk();
    void saveUserPresetsToDisk();

private:
    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Preset> allPresets;
    int currentIndex      = 0;
    int numFactoryPresets = 0;

    void buildFactoryPresets();

    /// Apply a Preset's band data directly to the APVTS parameters.
    void applyPreset(const Preset& p);

    juce::File getUserPresetsDirectory() const;

    // Helpers for disk serialisation
    static juce::XmlElement* presetToXml(const Preset& p);
    static bool xmlToPreset(const juce::XmlElement& xml, Preset& out);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace OmniQ
