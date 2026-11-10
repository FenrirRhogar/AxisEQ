#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/Theme.h"
#include "Utilities/Constants.h"

using namespace AxisEQ;

//──────────────────────────────────────────────────────────────────────────────
// Constructor / destructor
//──────────────────────────────────────────────────────────────────────────────
AxisEQAudioProcessorEditor::AxisEQAudioProcessorEditor(AxisEQAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&customLookAndFeel);

    presetManager = processorRef.presetManager.get();
    presetManager->scanUserPresetsFromDisk();   // pick up presets saved by other instances

    // ── Spectrum background thread ────────────────────────────────────────
    analyzer = std::make_unique<SpectrumAnalyzer>(processorRef.inputFifo, processorRef.outputFifo);

    // ── EQ Curve ──────────────────────────────────────────────────────────
    eqCurve = std::make_unique<EQCurveComponent>(processorRef, *analyzer);
    addAndMakeVisible(*eqCurve);

    // ── Control strip ─────────────────────────────────────────────────────
    controlPanel = std::make_unique<ControlPanelComponent>(processorRef);
    addAndMakeVisible(*controlPanel);

    eqCurve->onNodeSelected = [this](int bandIndex)
    {
        controlPanel->setSelectedBand(bandIndex);
    };

    // ── Header: presets and power ─────────────────────────────────────────
    presetBar = std::make_unique<PresetBar>(*presetManager);
    presetBar->onSaveRequested = [this] { onSavePreset(); };
    addAndMakeVisible(*presetBar);

    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, "master_bypass", power);
    addAndMakeVisible(power);

    updateBadge = std::make_unique<fenrikos::UpdateBadge>("AXISEQ " + juce::String(JucePlugin_VersionString),
                                                          juce::FontOptions(10.0f, juce::Font::bold).withKerningFactor(0.25f),
                                                          Theme::textDim.withAlpha(0.45f), Theme::accent,
                                                          juce::Justification::centredLeft);
    addAndMakeVisible(*updateBadge);

    // Resize limits and size come last: both trigger resized(), which needs every child.
    setResizable(true, true);
    setResizeLimits(860 + 2 * Ear, 560, 2560, 1440);
    setSize(980 + 2 * Ear, 660);
}

AxisEQAudioProcessorEditor::~AxisEQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//──────────────────────────────────────────────────────────────────────────────
// Presets
//──────────────────────────────────────────────────────────────────────────────
void AxisEQAudioProcessorEditor::onSavePreset()
{
    const int current = presetManager->getCurrentIndex();
    const auto suggestion = current >= presetManager->getNumFactoryPresets()
                                ? presetManager->getPreset(current).name : juce::String("My Preset");

    auto* dialog = new juce::AlertWindow("Save Preset", "Enter a name for this preset:",
                                         juce::MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel(&customLookAndFeel);
    dialog->addTextEditor("name", suggestion);
    dialog->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dialog](int result)
        {
            if (result == 1)
            {
                auto name = dialog->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                {
                    presetManager->saveCurrentAsUserPreset(name);
                    presetBar->refresh();
                }
            }
            // No delete here: enterModalState(..., true) deletes the dialog once this returns.
        }), true);
}

//──────────────────────────────────────────────────────────────────────────────
// paint — everything static lives in a cached image, redrawn only on resize
//──────────────────────────────────────────────────────────────────────────────
void AxisEQAudioProcessorEditor::paintBackground(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plate = bounds.reduced(static_cast<float>(Ear), 0.0f);
    rack::faceplate(g, bounds);

    // Nameplate: Ansuz mark, model name and tagline silkscreened at the top left.
    const float x = plate.getX();
    Theme::fillRune(g, Theme::ansuz({ x + 16.0f, 16.0f, 34.0f, 34.0f }));
    const auto wordFont = juce::FontOptions(24.0f, juce::Font::bold).withKerningFactor(0.28f);
    g.setFont(wordFont);
    rack::silk(g, "AXIS", { x + 56.0f, 13.0f, 140.0f, 26.0f }, juce::Justification::centredLeft);
    const float eqX = x + 56.0f + juce::GlyphArrangement::getStringWidth(wordFont, "AXIS") + 10.0f;
    rack::silk(g, "EQ", { eqX, 13.0f, 60.0f, 26.0f }, juce::Justification::centredLeft, Theme::accent.brighter(0.3f));
    g.setFont(juce::FontOptions(10.5f).withKerningFactor(0.08f));
    g.setColour(Theme::textDim);
    g.drawText("24-BAND DYNAMIC EQUALISER", juce::Rectangle<float>(x + 57.0f, 40.0f, 300.0f, 14.0f), juce::Justification::centredLeft);

    rack::seam(g, plate.getX() + 16.0f, plate.getRight() - 16.0f, 66.0f);

    rack::bezel(g, canvasCard.toFloat(), 9.0f);
    Theme::card(g, panelCard.toFloat(), 6.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold).withKerningFactor(0.3f));
    const auto footer = plate.withTop(bounds.getBottom() - static_cast<float>(FooterHeight));
    g.setColour(Theme::textDim);
    g.drawText("FENRIKOS", footer.withTrimmedRight(24.0f), juce::Justification::centredRight);
}

void AxisEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.drawImageAt(background, 0, 0);
}

//──────────────────────────────────────────────────────────────────────────────
// resized — layout all children
//──────────────────────────────────────────────────────────────────────────────
void AxisEQAudioProcessorEditor::resized()
{
    if (eqCurve == nullptr || controlPanel == nullptr || presetBar == nullptr || updateBadge == nullptr)
        return;   // still constructing

    const int w = getWidth(), h = getHeight();
    const auto plate = getLocalBounds().reduced(Ear, 0);
    const int x = plate.getX() + Margin, pw = plate.getWidth() - 2 * Margin;

    power.setBounds(plate.getRight() - 70, 20, 50, 28);
    presetBar->setBounds(juce::jmax(x + 300, plate.getRight() - 526), 20, 436, 28);

    const int panelHeight = ControlPanelComponent::PanelHeight + 20;
    panelCard  = { x, h - FooterHeight - panelHeight, pw, panelHeight };
    canvasCard = { x, HeaderHeight + 14, pw, panelCard.getY() - 22 - (HeaderHeight + 14) };

    eqCurve->setBounds(canvasCard.reduced(8));
    controlPanel->setBounds(panelCard.reduced(12, 10));
    updateBadge->setBounds(plate.getX() + 24, h - FooterHeight, w / 2, FooterHeight);

    background = juce::Image(juce::Image::RGB, w, h, false);
    juce::Graphics g(background);
    paintBackground(g);
}
