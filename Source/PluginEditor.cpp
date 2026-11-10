#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/Theme.h"
#include "GUI/RackUI.h"
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

    updateBadge = std::make_unique<fenrikos::UpdateBadge>(juce::String(juce::CharPointer_UTF8("AXISEQ  \xc2\xb7  V")) + JucePlugin_VersionString,
                                                          rackui::font(rackui::Face::textMedium, 8.0f, 0.3f),
                                                          rackui::colour::inkFaint, Theme::accent,
                                                          juce::Justification::centredRight);
    addAndMakeVisible(*updateBadge);

    // Resize limits and size come last: both trigger resized(), which needs every child.
    setResizable(true, true);
    setResizeLimits(1040 + 2 * Ear, 600, 2560, 1440);
    setSize(1100 + 2 * Ear, 680);
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
    using namespace rackui;
    const auto bounds = getLocalBounds().toFloat();
    const auto plateArea = bounds.reduced(static_cast<float>(Ear), 0.0f);
    plate(g, plateArea);
    ears(g, bounds);

    // Nameplate: the Ansuz rune, AXIS EQ, and the sub-legend.
    const float x = plateArea.getX() + (float) Margin;
    g.setColour(colour::ink);
    g.fillPath(rune({ { 7, 2, 7, 18 }, { 7, 2.4f, 15, 8 }, { 7, 7.6f, 15, 13.2f } }), juce::AffineTransform::scale(1.3f).translated(x, 22.0f));
    const auto wordFont = font(Face::legendBold, 30.0f, 0.08f);
    print(g, "AXIS", { x + 36.0f, 18.0f, 120.0f, 30.0f }, Face::legendBold, 30.0f, 0.08f, colour::ink, juce::Justification::centredLeft);
    print(g, "EQ", { x + 44.0f + juce::GlyphArrangement::getStringWidth(wordFont, "AXIS"), 18.0f, 60.0f, 30.0f }, Face::legendBold, 30.0f, 0.08f,
          juce::Colour(0xFF8FB6FF), juce::Justification::centredLeft);
    print(g, "24-BAND DYNAMIC EQUALISER", { x + 37.0f, 48.0f, 260.0f, 12.0f }, Face::textMedium, 9.0f, 0.2f, colour::inkDim,
          juce::Justification::centredLeft);
    divider(g, (float) headerDividerX, 26.0f, 34.0f);

    // The screen sits in a bezel; the band strip below is open plate.
    bezel(g, canvasCard.toFloat());
    rule(g, plateArea.getX() + (float) Margin, plateArea.getRight() - (float) Margin, (float) panelCard.getY() - 12.0f);

    print(g, "fenrikos", { x, bounds.getBottom() - 20.0f, 200.0f, 14.0f }, Face::textMedium, 8.0f, 0.3f, colour::inkFaint,
          juce::Justification::centredLeft);
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
    const auto plateBounds = getLocalBounds().reduced(Ear, 0);
    const int x = plateBounds.getX() + Margin, pw = plateBounds.getWidth() - 2 * Margin;

    // Header: preset panel and the IN switch, right-aligned on the nameplate's line.
    power.setBounds(plateBounds.getRight() - Margin - 64, 24, 64, 30);
    headerDividerX = power.getX() - 20;
    presetBar->setBounds(headerDividerX - 20 - 440, 22, 440, 34);

    panelCard  = { x, h - FooterHeight - ControlPanelComponent::PanelHeight - 6, pw, ControlPanelComponent::PanelHeight };
    canvasCard = { x, HeaderHeight + 6, pw, panelCard.getY() - 30 - (HeaderHeight + 6) };

    eqCurve->setBounds(canvasCard.reduced(7));
    controlPanel->setBounds(panelCard);
    updateBadge->setBounds(plateBounds.getRight() - Margin - 300, h - 20, 300, 14);

    background = juce::Image(juce::Image::RGB, w, h, false);
    juce::Graphics g(background);
    paintBackground(g);
}
