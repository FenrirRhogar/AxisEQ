#pragma once

#include <JuceHeader.h>
#include <vector>

namespace AxisEQ
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
    // User presets only: the file it was read from, so deleting removes exactly that file.
    juce::File file;
};

/// Manages factory and user presets.
/// Factory presets are hard-coded in the .cpp.
/// User presets are stored as .xml files in ~/Documents/AxisEQ/Presets/.
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    // ─── Preset list ─────────────────────────────────────────────────────────
    int            getNumPresets()     const noexcept { return static_cast<int>(allPresets.size()); }
    const Preset&  getPreset(int idx)  const          { return allPresets.at(static_cast<size_t>(idx)); }
    int            getCurrentIndex()   const noexcept { return currentIndex.load(); }
    /// Safe to call from any thread (hosts ask for program names from their own).
    juce::String   getPresetName(int idx) const;
    int            getNumFactoryPresets() const noexcept { return numFactoryPresets; }

    /// After a session restore: points the current index at the preset saved with the session.
    void syncFromState();
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
    /// Re-reads the user preset folder. Other plugin instances may have changed it.
    void scanUserPresetsFromDisk();

    /// Folder holding the user presets (created if missing).
    juce::File getUserPresetsDirectory() const;

private:
    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Preset> allPresets;
    mutable juce::CriticalSection listLock;   // held while the list is rebuilt and by getPresetName
    std::atomic<int> currentIndex { 0 };
    int numFactoryPresets = 0;

    void buildFactoryPresets();

    /// Apply a Preset's band data directly to the APVTS parameters.
    void applyPreset(const Preset& p);

    int findUserPreset(const juce::String& name) const;

    // Helpers for disk serialisation
    static juce::XmlElement* presetToXml(const Preset& p);
    static bool xmlToPreset(const juce::XmlElement& xml, Preset& out);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace AxisEQ
