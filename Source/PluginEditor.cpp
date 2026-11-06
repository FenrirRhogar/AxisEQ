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
    setResizeLimits(860, 520, 2560, 1440);
    setSize(980, 620);
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
    g.setGradientFill(juce::ColourGradient(Theme::plateTop, 0.0f, 0.0f, Theme::plateBottom, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Soft accent glow from the top edge.
    g.setGradientFill(juce::ColourGradient(Theme::accent.withAlpha(0.16f), bounds.getCentreX(), 0.0f,
                                           Theme::accent.withAlpha(0.0f), bounds.getCentreX(), 90.0f, false));
    g.fillRect(bounds.withHeight(90.0f));
    g.setColour(Theme::accent);
    g.fillRect(bounds.withHeight(2.0f));

    // Header: Ansuz mark, wordmark and tagline.
    Theme::fillRune(g, Theme::ansuz({ 16.0f, 16.0f, 34.0f, 34.0f }));
    const auto wordFont = juce::FontOptions(23.0f, juce::Font::bold).withKerningFactor(0.2f);
    g.setFont(wordFont);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawText("AXIS", juce::Rectangle<float>(56.0f, 15.0f, 120.0f, 26.0f), juce::Justification::centredLeft);
    g.setColour(Theme::text);
    g.drawText("AXIS", juce::Rectangle<float>(56.0f, 14.0f, 120.0f, 26.0f), juce::Justification::centredLeft);
    const float eqX = 56.0f + juce::GlyphArrangement::getStringWidth(wordFont, "AXIS") + 10.0f;
    g.setColour(Theme::accent.brighter(0.25f));
    g.drawText("EQ", juce::Rectangle<float>(eqX, 14.0f, 60.0f, 26.0f), juce::Justification::centredLeft);
    g.setFont(Theme::font(11.5f));
    g.setColour(Theme::textDim);
    g.drawText("24-band dynamic equaliser", juce::Rectangle<float>(57.0f, 40.0f, 300.0f, 14.0f), juce::Justification::centredLeft);

    g.setColour(Theme::cardEdge);
    g.fillRect(20.0f, 66.0f, bounds.getWidth() - 40.0f, 1.0f);

    Theme::card(g, canvasCard.toFloat(), 12.0f);
    Theme::card(g, panelCard.toFloat(), 12.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold).withKerningFactor(0.25f));
    const auto footer = bounds.withTop(bounds.getBottom() - static_cast<float>(FooterHeight));
    g.setColour(Theme::textDim.withAlpha(0.8f));
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

    power.setBounds(w - 68, 21, 46, 24);
    presetBar->setBounds(juce::jmax(300, w - 520), 19, 430, 28);

    const int panelHeight = ControlPanelComponent::PanelHeight + 16;
    panelCard  = { Margin, h - FooterHeight - panelHeight, w - 2 * Margin, panelHeight };
    canvasCard = { Margin, HeaderHeight + 12, w - 2 * Margin, panelCard.getY() - 12 - (HeaderHeight + 12) };

    eqCurve->setBounds(canvasCard.reduced(6));
    controlPanel->setBounds(panelCard.reduced(10, 8));
    updateBadge->setBounds(24, h - FooterHeight, w / 2, FooterHeight);

    background = juce::Image(juce::Image::RGB, w, h, false);
    juce::Graphics g(background);
    paintBackground(g);
}
