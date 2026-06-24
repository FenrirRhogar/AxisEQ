#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utilities/Constants.h"

//──────────────────────────────────────────────────────────────────────────────
// Colour palette — single source of truth for the editor chrome
//──────────────────────────────────────────────────────────────────────────────
namespace
{
    const juce::Colour BG_TOP     { 0xFF0D0D1A };
    const juce::Colour BG_BOT     { 0xFF070710 };
    const juce::Colour ACCENT_1   { 0xFF3D84FF }; // Blue
    const juce::Colour ACCENT_2   { 0xFF9B5FFF }; // Purple
    const juce::Colour PANEL_BG   { 0xFF111122 };
    const juce::Colour TEXT_HI    { 0xFFE8E8FF };
    const juce::Colour TEXT_LO    { 0xFF6868A0 };
    const juce::Colour BORDER     { 0xFF252540 };
}

//──────────────────────────────────────────────────────────────────────────────
// Constructor / destructor
//──────────────────────────────────────────────────────────────────────────────
OmniQAudioProcessorEditor::OmniQAudioProcessorEditor(OmniQAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setResizable(true, true);
    setResizeLimits(800, 440, 2560, 1440);

    // ── Preset system ────────────────────────────────────────────────────
    setLookAndFeel(&customLookAndFeel);
    
    presetManager = std::make_unique<OmniQ::PresetManager>(processorRef.apvts);

    // ── Spectrum background thread ────────────────────────────────────────
    analyzer = std::make_unique<OmniQ::SpectrumAnalyzer>(processorRef.inputFifo, processorRef.outputFifo);

    // ── EQ Curve ──────────────────────────────────────────────────────────
    eqCurve = std::make_unique<OmniQ::EQCurveComponent>(processorRef, *analyzer);
    addAndMakeVisible(*eqCurve);

    // ── Control strip ─────────────────────────────────────────────────────
    controlPanel = std::make_unique<OmniQ::ControlPanelComponent>(processorRef);
    addAndMakeVisible(*controlPanel);

    eqCurve->onNodeSelected = [this](int bandIndex)
    {
        controlPanel->setSelectedBand(bandIndex);
    };

    // ── Top bar ───────────────────────────────────────────────────────────
    buildTopBar();

    // setSize MUST come last so resized() can find all children
    setSize(960, 560);
}

OmniQAudioProcessorEditor::~OmniQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//──────────────────────────────────────────────────────────────────────────────
// Top-bar construction helpers
//──────────────────────────────────────────────────────────────────────────────
void OmniQAudioProcessorEditor::buildTopBar()
{
    // ── Preset ComboBox ───────────────────────────────────────────────────
    presetCombo.setJustificationType(juce::Justification::centred);
    presetCombo.setColour(juce::ComboBox::backgroundColourId, PANEL_BG);
    presetCombo.setColour(juce::ComboBox::textColourId,       TEXT_HI);
    presetCombo.setColour(juce::ComboBox::outlineColourId,    BORDER);
    presetCombo.setColour(juce::ComboBox::arrowColourId,      ACCENT_1);
    populatePresetCombo();
    presetCombo.onChange = [this] { onPresetComboChanged(); };
    addAndMakeVisible(presetCombo);

    // ── Navigation buttons ────────────────────────────────────────────────
    auto styleBtn = [&](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId,   PANEL_BG);
        btn.setColour(juce::TextButton::buttonOnColourId, ACCENT_1);
        btn.setColour(juce::TextButton::textColourOffId,  TEXT_HI);
        btn.setColour(juce::ComboBox::outlineColourId,    BORDER);
        addAndMakeVisible(btn);
    };

    styleBtn(prevPresetBtn);
    styleBtn(nextPresetBtn);
    styleBtn(savePresetBtn);
    styleBtn(resetBtn);

    prevPresetBtn.onClick = [this]
    {
        presetManager->loadPreviousPreset();
        populatePresetCombo();
        presetCombo.setSelectedId(presetManager->getCurrentIndex() + 1,
                                  juce::dontSendNotification);
    };

    nextPresetBtn.onClick = [this]
    {
        presetManager->loadNextPreset();
        populatePresetCombo();
        presetCombo.setSelectedId(presetManager->getCurrentIndex() + 1,
                                  juce::dontSendNotification);
    };

    savePresetBtn.onClick = [this] { onSavePreset(); };
    resetBtn.onClick      = [this] { onResetPreset(); };
}

void OmniQAudioProcessorEditor::populatePresetCombo()
{
    presetCombo.clear(juce::dontSendNotification);

    // Group presets by category using header items
    juce::String lastCat;
    int id = 1;
    for (int i = 0; i < presetManager->getNumPresets(); ++i)
    {
        const auto& p = presetManager->getPreset(i);
        if (p.category != lastCat)
        {
            presetCombo.addSectionHeading(p.category);
            lastCat = p.category;
        }
        presetCombo.addItem(p.name, id++);
    }

    // Reflect current selection
    presetCombo.setSelectedId(presetManager->getCurrentIndex() + 1,
                              juce::dontSendNotification);
}

void OmniQAudioProcessorEditor::onPresetComboChanged()
{
    const int selectedId = presetCombo.getSelectedId();
    if (selectedId > 0)
        presetManager->loadPreset(selectedId - 1);
}

void OmniQAudioProcessorEditor::onSavePreset()
{
    auto* dialog = new juce::AlertWindow("Save Preset",
                                          "Enter a name for this preset:",
                                          juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", "My Preset");
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
                    populatePresetCombo();
                    presetCombo.setSelectedId(presetManager->getCurrentIndex() + 1,
                                             juce::dontSendNotification);
                }
            }
            delete dialog;
        }), true);
}

void OmniQAudioProcessorEditor::onResetPreset()
{
    presetManager->loadPreset(0); // Load "Flat"
    presetCombo.setSelectedId(1, juce::dontSendNotification);
}

//──────────────────────────────────────────────────────────────────────────────
// paint — draws the full chrome: background, header bar, borders
//──────────────────────────────────────────────────────────────────────────────
void OmniQAudioProcessorEditor::paintBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Rich dark gradient base
    g.setGradientFill(juce::ColourGradient(
        BG_TOP, bounds.getCentreX(), bounds.getY(),
        BG_BOT, bounds.getCentreX(), bounds.getBottom(),
        false));
    g.fillRect(bounds);

    // Header bar fill
    auto headerRect = bounds.removeFromTop(48.0f);
    g.setColour(PANEL_BG.withAlpha(0.95f));
    g.fillRect(headerRect);

    // Accent gradient line below header
    g.setGradientFill(juce::ColourGradient(
        ACCENT_1.withAlpha(0.0f), 0.0f, 0.0f,
        ACCENT_2,                 bounds.getCentreX(), 0.0f,
        false));
    g.fillRect(juce::Rectangle<float>(0.0f, 48.0f, static_cast<float>(getWidth()), 2.0f));

    // Footer divider above control strip
    const float footerY = static_cast<float>(getHeight()) - OmniQ::ControlPanelComponent::PanelHeight - 1.0f;
    g.setColour(BORDER);
    g.fillRect(juce::Rectangle<float>(0.0f, footerY, static_cast<float>(getWidth()), 1.0f));
}

void OmniQAudioProcessorEditor::paint(juce::Graphics& g)
{
    paintBackground(g);

    // Logo — left of header
    g.setColour(TEXT_HI);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("Omni-Q", 16, 0, 120, 48, juce::Justification::centredLeft, false);

    // Logo sub-caption
    g.setColour(TEXT_LO);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("24-Band Parametric", 16, 27, 140, 14, juce::Justification::centredLeft, false);

    // Accent dot between logo sections
    g.setColour(ACCENT_1);
    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f, 5.0f, 5.0f).withY(21.5f).withX(8.0f));
}

//──────────────────────────────────────────────────────────────────────────────
// resized — layout all children
//──────────────────────────────────────────────────────────────────────────────
void OmniQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ── Top bar (48 px) ───────────────────────────────────────────────────
    auto headerArea = area.removeFromTop(48);
    layoutTopBar(headerArea);

    // ── Bottom control strip ──────────────────────────────────────────────
    auto footerArea = area.removeFromBottom(OmniQ::ControlPanelComponent::PanelHeight);
    if (controlPanel)
        controlPanel->setBounds(footerArea);

    // ── EQ canvas fills the rest ──────────────────────────────────────────
    if (eqCurve)
        eqCurve->setBounds(area);
}

void OmniQAudioProcessorEditor::layoutTopBar(juce::Rectangle<int>& area)
{
    // Logo occupies fixed left region — skip 160 px
    area.removeFromLeft(160);

    // Right-aligned utility buttons
    const int btnW = 54, btnH = 26, margin = 6;
    auto right = area.removeFromRight(160).reduced(0, 11);

    resetBtn.setBounds(right.removeFromRight(btnW).withSizeKeepingCentre(btnW, btnH));
    right.removeFromRight(margin);
    savePresetBtn.setBounds(right.removeFromRight(btnW).withSizeKeepingCentre(btnW, btnH));

    // Nav + preset combo in the remaining centre
    auto centre = area.reduced(0, 10);
    const int navW = 28;

    prevPresetBtn.setBounds(centre.removeFromLeft(navW).withSizeKeepingCentre(navW, 26));
    centre.removeFromLeft(4);
    nextPresetBtn.setBounds(centre.removeFromRight(navW).withSizeKeepingCentre(navW, 26));
    centre.removeFromRight(4);
    presetCombo.setBounds(centre);
}
