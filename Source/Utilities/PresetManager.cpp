#include "PresetManager.h"
#include "Constants.h"

namespace OmniQ
{

// ─────────────────────────────────────────────────────────────────────────────
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    buildFactoryPresets();
    numFactoryPresets = static_cast<int>(allPresets.size());
    scanUserPresetsFromDisk();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray names;
    for (auto& p : allPresets)
        names.add(p.name);
    return names;
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(allPresets.size())) return;
    currentIndex = index;
    applyPreset(allPresets[static_cast<size_t>(index)]);
}

void PresetManager::loadNextPreset()
{
    loadPreset((currentIndex + 1) % static_cast<int>(allPresets.size()));
}

void PresetManager::loadPreviousPreset()
{
    int n = static_cast<int>(allPresets.size());
    loadPreset((currentIndex - 1 + n) % n);
}

void PresetManager::saveCurrentAsUserPreset(const juce::String& name,
                                             const juce::String& category)
{
    // Build a Preset from the current APVTS state by reading each parameter
    Preset p;
    p.name     = name;
    p.category = category;
    p.bands.resize(static_cast<size_t>(MaxBands));

    for (int i = 0; i < MaxBands; ++i)
    {
        auto& b = p.bands[static_cast<size_t>(i)];
        auto get = [&](const char* suffix) -> float
        {
            auto* raw = apvts.getRawParameterValue(paramID(suffix, i));
            return raw ? raw->load(std::memory_order_relaxed) : 0.0f;
        };

        b.active       = get("active")       >= 0.5f;
        b.bypassed     = get("bypass")       >= 0.5f;
        b.freq         = get("freq");
        b.gain         = get("gain");
        b.q            = get("q");
        b.type         = static_cast<int>(get("type"));
        b.slope        = static_cast<int>(get("slope"));
        b.route        = static_cast<int>(get("route"));
        b.dynEnabled   = get("dyn_enabled")  >= 0.5f;
        b.dynExtSc     = get("dyn_ext_sc")   >= 0.5f;
        b.dynThreshold = get("dyn_threshold");
        b.dynRange     = get("dyn_range");
        b.dynAttack    = get("dyn_attack");
        b.dynRelease   = get("dyn_release");
    }

    // Overwrite if same name exists in user presets
    for (int i = numFactoryPresets; i < static_cast<int>(allPresets.size()); ++i)
    {
        if (allPresets[static_cast<size_t>(i)].name == name)
        {
            allPresets[static_cast<size_t>(i)] = p;
            currentIndex = i;
            saveUserPresetsToDisk();
            return;
        }
    }

    allPresets.push_back(p);
    currentIndex = static_cast<int>(allPresets.size()) - 1;
    saveUserPresetsToDisk();
}

void PresetManager::deleteUserPreset(int index)
{
    if (index < numFactoryPresets || index >= static_cast<int>(allPresets.size())) return;
    allPresets.erase(allPresets.begin() + index);
    currentIndex = juce::jlimit(0, static_cast<int>(allPresets.size()) - 1, currentIndex);
    saveUserPresetsToDisk();
}

// ─────────────────────────────────────────────────────────────────────────────
// Core: apply preset bands → APVTS parameters directly
// ─────────────────────────────────────────────────────────────────────────────
void PresetManager::applyPreset(const Preset& p)
{
    const int numDefined = static_cast<int>(p.bands.size());

    for (int i = 0; i < MaxBands; ++i)
    {
        // For bands beyond what the preset defines, reset to inactive defaults
        PresetBand b;
        if (i < numDefined)
            b = p.bands[static_cast<size_t>(i)];
        else
            b = PresetBand{};   // default-constructed → inactive

        auto set = [&](const char* suffix, float value)
        {
            if (auto* param = apvts.getParameter(paramID(suffix, i)))
                param->setValueNotifyingHost(param->convertTo0to1(value));
        };

        auto setChoice = [&](const char* suffix, int index)
        {
            if (auto* param = apvts.getParameter(paramID(suffix, i)))
                // Choice params: normalised value = index / (numChoices - 1)
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        };

        auto setBool = [&](const char* suffix, bool on)
        {
            if (auto* param = apvts.getParameter(paramID(suffix, i)))
                param->setValueNotifyingHost(on ? 1.0f : 0.0f);
        };

        // Apply in dependency order: deactivate first, set params, then activate
        setBool("active",        false);      // deactivate while changing
        setBool("bypass",        b.bypassed);
        set    ("freq",          b.freq);
        set    ("gain",          b.gain);
        set    ("q",             b.q);
        setChoice("type",        b.type);
        setChoice("slope",       b.slope);
        setChoice("route",       b.route);
        setBool("dyn_enabled",   b.dynEnabled);
        setBool("dyn_ext_sc",    b.dynExtSc);
        set    ("dyn_threshold", b.dynThreshold);
        set    ("dyn_range",     b.dynRange);
        set    ("dyn_attack",    b.dynAttack);
        set    ("dyn_release",   b.dynRelease);
        setBool("active",        b.active);   // activate last
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Disk persistence — XML per user preset file
// ─────────────────────────────────────────────────────────────────────────────
juce::File PresetManager::getUserPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("OmniQ").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

juce::XmlElement* PresetManager::presetToXml(const Preset& p)
{
    auto* root = new juce::XmlElement("OmniQPreset");
    root->setAttribute("name",     p.name);
    root->setAttribute("category", p.category);

    for (int i = 0; i < static_cast<int>(p.bands.size()); ++i)
    {
        const auto& b = p.bands[static_cast<size_t>(i)];
        auto* bandEl = root->createNewChildElement("Band");
        bandEl->setAttribute("index",        i);
        bandEl->setAttribute("active",       b.active       ? 1 : 0);
        bandEl->setAttribute("bypassed",     b.bypassed     ? 1 : 0);
        bandEl->setAttribute("freq",         b.freq);
        bandEl->setAttribute("gain",         b.gain);
        bandEl->setAttribute("q",            b.q);
        bandEl->setAttribute("type",         b.type);
        bandEl->setAttribute("slope",        b.slope);
        bandEl->setAttribute("route",        b.route);
        bandEl->setAttribute("dynEnabled",   b.dynEnabled   ? 1 : 0);
        bandEl->setAttribute("dynExtSc",     b.dynExtSc     ? 1 : 0);
        bandEl->setAttribute("dynThreshold", b.dynThreshold);
        bandEl->setAttribute("dynRange",     b.dynRange);
        bandEl->setAttribute("dynAttack",    b.dynAttack);
        bandEl->setAttribute("dynRelease",   b.dynRelease);
    }
    return root;
}

bool PresetManager::xmlToPreset(const juce::XmlElement& xml, Preset& out)
{
    if (! xml.hasTagName("OmniQPreset")) return false;

    out.name     = xml.getStringAttribute("name",     "Unnamed");
    out.category = xml.getStringAttribute("category", "User");
    out.bands.clear();
    out.bands.resize(static_cast<size_t>(MaxBands));

    for (auto* bandEl : xml.getChildWithTagNameIterator("Band"))
    {
        int idx = bandEl->getIntAttribute("index", -1);
        if (idx < 0 || idx >= MaxBands) continue;

        auto& b = out.bands[static_cast<size_t>(idx)];
        b.active       = bandEl->getIntAttribute("active",       0) != 0;
        b.bypassed     = bandEl->getIntAttribute("bypassed",     0) != 0;
        b.freq         = static_cast<float>(bandEl->getDoubleAttribute("freq",         1000.0));
        b.gain         = static_cast<float>(bandEl->getDoubleAttribute("gain",         0.0));
        b.q            = static_cast<float>(bandEl->getDoubleAttribute("q",            0.707));
        b.type         = bandEl->getIntAttribute("type",         0);
        b.slope        = bandEl->getIntAttribute("slope",        1);
        b.route        = bandEl->getIntAttribute("route",        0);
        b.dynEnabled   = bandEl->getIntAttribute("dynEnabled",   0) != 0;
        b.dynExtSc     = bandEl->getIntAttribute("dynExtSc",     0) != 0;
        b.dynThreshold = static_cast<float>(bandEl->getDoubleAttribute("dynThreshold", -20.0));
        b.dynRange     = static_cast<float>(bandEl->getDoubleAttribute("dynRange",     0.0));
        b.dynAttack    = static_cast<float>(bandEl->getDoubleAttribute("dynAttack",    10.0));
        b.dynRelease   = static_cast<float>(bandEl->getDoubleAttribute("dynRelease",   100.0));
    }
    return true;
}

void PresetManager::scanUserPresetsFromDisk()
{
    if (static_cast<int>(allPresets.size()) > numFactoryPresets)
        allPresets.erase(allPresets.begin() + numFactoryPresets, allPresets.end());

    auto files = getUserPresetsDirectory().findChildFiles(juce::File::findFiles, false, "*.xml");
    for (auto& f : files)
    {
        if (auto xml = juce::XmlDocument::parse(f))
        {
            Preset p;
            if (xmlToPreset(*xml, p))
                allPresets.push_back(p);
        }
    }
}

void PresetManager::saveUserPresetsToDisk()
{
    auto dir = getUserPresetsDirectory();
    // Remove stale files
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        f.deleteFile();

    for (int i = numFactoryPresets; i < static_cast<int>(allPresets.size()); ++i)
    {
        std::unique_ptr<juce::XmlElement> xml(presetToXml(allPresets[static_cast<size_t>(i)]));
        auto file = dir.getChildFile(allPresets[static_cast<size_t>(i)].name + ".xml");
        xml->writeTo(file);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory preset library — built from PresetBand structs (no XML hand-writing)
// ─────────────────────────────────────────────────────────────────────────────

// Helper: quickly build one active bell/cut/shelf band
static PresetBand band(float freq, float gain, float q,
                       int type = 0 /*Bell*/, int slope = 1 /*12dB*/)
{
    PresetBand b;
    b.active = true;
    b.freq   = freq;
    b.gain   = gain;
    b.q      = q;
    b.type   = type;
    b.slope  = slope;
    return b;
}

// Place bands into the preset's array at specific slot indices
static Preset makePreset(const juce::String& name, const juce::String& category,
                         std::initializer_list<std::pair<int, PresetBand>> slots)
{
    Preset p;
    p.name     = name;
    p.category = category;
    p.bands.resize(static_cast<size_t>(MaxBands));  // all inactive by default

    for (auto& [idx, b] : slots)
        if (idx >= 0 && idx < MaxBands)
            p.bands[static_cast<size_t>(idx)] = b;

    return p;
}

void PresetManager::buildFactoryPresets()
{
    // ── Flat ─────────────────────────────────────────────────────────────
    {
        Preset p;
        p.name     = "Flat";
        p.category = "Utility";
        p.bands.resize(static_cast<size_t>(MaxBands)); // all inactive
        allPresets.push_back(p);
    }

    // ── Vocal Presence ────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Vocal Presence", "Vocals", {
        {0, band(80.0f,   0.0f, 0.707f, 1 /*LowCut*/,   2 /*24dB*/)},
        {1, band(300.0f, -2.5f, 1.5f)},
        {2, band(2500.0f, 1.5f, 1.2f)},
        {3, band(5000.0f, 3.0f, 1.0f)},
        {4, band(10000.0f,2.0f, 0.707f, 4 /*HighShelf*/)},
    }));

    // ── Acoustic Guitar ────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Acoustic Guitar", "Instruments", {
        {0, band(60.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(200.0f, -2.0f, 1.0f)},
        {2, band(800.0f, -1.5f, 1.5f)},
        {3, band(3000.0f, 2.5f, 1.0f)},
        {4, band(8000.0f, 2.0f, 0.707f, 4)},
        {5, band(16000.0f,0.0f, 0.707f, 2 /*HighCut*/)},
    }));

    // ── Bass Guitar ───────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Bass Guitar", "Instruments", {
        {0, band(40.0f,   0.0f, 0.707f, 1, 1)},
        {1, band(80.0f,   3.0f, 1.2f)},
        {2, band(250.0f, -2.0f, 1.0f)},
        {3, band(800.0f,  1.5f, 1.5f)},
        {4, band(3500.0f,-2.0f, 1.0f)},
    }));

    // ── Kick Drum ─────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Kick Drum", "Drums", {
        {0, band(50.0f,   4.0f, 2.0f)},
        {1, band(300.0f, -4.0f, 1.5f)},
        {2, band(3000.0f, 3.0f, 1.5f)},
        {3, band(8000.0f,-2.0f, 1.0f)},
    }));

    // ── Snare Drum ────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Snare Drum", "Drums", {
        {0, band(100.0f,  0.0f, 0.707f, 1, 1)},
        {1, band(200.0f, -2.0f, 1.5f)},
        {2, band(900.0f,  2.0f, 1.5f)},
        {3, band(5000.0f, 4.0f, 1.2f)},
        {4, band(10000.0f,2.0f, 0.707f, 4)},
    }));

    // ── Drum Bus ──────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Drum Bus", "Drums", {
        {0, band(40.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(120.0f,  2.0f, 1.5f)},
        {2, band(350.0f, -2.5f, 1.0f)},
        {3, band(2500.0f, 2.0f, 1.2f)},
        {4, band(12000.0f,3.0f, 0.707f, 4)},
    }));

    // ── Piano ─────────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Piano", "Instruments", {
        {0, band(50.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(200.0f, -1.5f, 1.0f)},
        {2, band(3000.0f, 1.5f, 1.0f)},
        {3, band(8000.0f, 1.5f, 0.707f, 4)},
    }));

    // ── Electric Guitar ───────────────────────────────────────────────────
    allPresets.push_back(makePreset("Electric Guitar", "Instruments", {
        {0, band(80.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(250.0f, -3.0f, 1.2f)},
        {2, band(700.0f,  1.5f, 1.5f)},
        {3, band(2500.0f, 2.5f, 1.2f)},
        {4, band(8000.0f,-2.0f, 1.0f)},
    }));

    // ── Mix Bus Polish ────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Mix Bus Polish", "Mastering", {
        {0, band(25.0f,   0.0f, 0.707f, 1, 1)},
        {1, band(200.0f, -0.5f, 0.8f)},
        {2, band(3000.0f, 0.5f, 0.8f)},
        {3, band(8000.0f, 1.0f, 0.707f, 4)},
    }));

    // ── Mastering Loudness ────────────────────────────────────────────────
    allPresets.push_back(makePreset("Mastering Loudness", "Mastering", {
        {0, band(20.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(80.0f,   1.0f, 1.2f)},
        {2, band(250.0f, -1.0f, 0.7f)},
        {3, band(1000.0f, 0.5f, 0.8f)},
        {4, band(5000.0f, 1.5f, 0.8f)},
        {5, band(12000.0f,2.0f, 0.707f, 4)},
    }));

    // ── Broadcast / Radio ─────────────────────────────────────────────────
    allPresets.push_back(makePreset("Broadcast", "Vocals", {
        {0, band(100.0f,  0.0f, 0.707f, 1, 2)},
        {1, band(300.0f, -3.0f, 1.5f)},
        {2, band(800.0f,  1.5f, 1.0f)},
        {3, band(2500.0f, 3.0f, 1.2f)},
        {4, band(6000.0f,-2.0f, 1.5f)},
        {5, band(10000.0f,0.0f, 0.707f, 2)},
    }));

    // ── Lo-Fi Warmth ──────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Lo-Fi Warmth", "Creative", {
        {0, band(80.0f,   0.0f, 0.707f, 1, 1)},
        {1, band(120.0f,  3.0f, 1.0f)},
        {2, band(400.0f,  1.5f, 0.8f,  3 /*LowShelf*/)},
        {3, band(5000.0f, 0.0f, 0.8f,  2 /*HighCut*/)},
    }));

    // ── Telephone ────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Telephone", "Creative", {
        {0, band(300.0f,  0.0f, 0.707f, 1, 2)},
        {1, band(3200.0f, 0.0f, 0.707f, 2, 2)},
        {2, band(800.0f,  5.0f, 1.5f)},
    }));

    // ── Deep Sub Bass ─────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Deep Sub Bass", "Creative", {
        {0, band(50.0f,   5.0f, 2.0f)},
        {1, band(150.0f, -2.0f, 1.5f)},
        {2, band(6000.0f, 0.0f, 0.707f, 2)},
    }));

    // ── Air Shelf ─────────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Air Shelf", "Mastering", {
        {0, band(30.0f,   0.0f, 0.707f, 1, 2)},
        {1, band(8000.0f, 2.5f, 0.707f, 4)},
        {2, band(16000.0f,3.0f, 0.707f, 4)},
    }));

    // ── Vintage Warmth ────────────────────────────────────────────────────
    allPresets.push_back(makePreset("Vintage Warmth", "Creative", {
        {0, band(30.0f,   0.0f, 0.707f, 1, 1)},
        {1, band(100.0f,  2.0f, 0.707f, 3 /*LowShelf*/)},
        {2, band(3000.0f,-1.5f, 0.8f)},
        {3, band(12000.0f,0.0f, 0.707f, 2)},
    }));

    // ── External Sidechain: Kick Ducking ─────────────────────────────────
    {
        Preset p = makePreset("Kick Ducking (Sidechain)", "Dynamics", {
            { 0, band(60.f, 0.f, 0.707f, 0, 1) }
        });
        p.bands[0].dynEnabled = true;
        p.bands[0].dynExtSc   = true;
        p.bands[0].dynThreshold = -30.0f;
        p.bands[0].dynRange   = -12.0f;
        p.bands[0].dynAttack  = 5.0f;
        p.bands[0].dynRelease = 60.0f;
        allPresets.push_back(p);
    }

    // ── External Sidechain: Vocal Rider ──────────────────────────────────
    {
        Preset p = makePreset("Vocal Rider (Sidechain)", "Dynamics", {
            { 0, band(2000.f, 0.f, 0.3f, 0, 1) }
        });
        p.bands[0].dynEnabled = true;
        p.bands[0].dynExtSc   = true;
        p.bands[0].dynThreshold = -40.0f;
        p.bands[0].dynRange   = -6.0f;
        p.bands[0].dynAttack  = 2.0f;
        p.bands[0].dynRelease = 150.0f;
        allPresets.push_back(p);
    }
}

} // namespace OmniQ
