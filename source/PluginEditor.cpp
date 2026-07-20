#include "PluginEditor.h"

//==============================================================================
// Preset table — values in real units (not normalised)
// Applied via apvts.getParameterRange().convertTo0to1() in applyPreset()
struct PresetValues
{
    const char* name;
    float drive, color, motion, lfoRate, resonance, mix, presence;
};

static const std::array<PresetValues, 7> presets
{{
    //  name          drive  color    motion  rate   res    mix    presence
    { "Default",      1.5f,  8000.0f, 0.0f,  1.0f,  0.8f,  0.8f,  0.0f },
    { "Cassette",     3.5f,  4000.0f, 0.25f, 0.8f,  0.9f,  0.85f, 3.0f },
    { "Radio",        2.0f,  3200.0f, 0.0f,  1.0f,  1.5f,  0.9f,  2.0f },
    { "Lo-Fi",        4.5f,  2800.0f, 0.15f, 0.5f,  1.2f,  0.7f,  4.0f },
    { "Shimmer",      1.8f, 12000.0f, 0.1f,  2.0f,  0.6f,  0.9f,  9.0f },
    { "Chaos",        4.0f,  5000.0f, 0.85f, 7.0f,  2.5f,  0.65f, 2.0f },
    { "Arrow",        5.0f,  3500.0f, 0.6f,  1.5f,  1.8f,  0.72f, 6.0f },
}};

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&customLookAndFeel);

    faceplateBg = juce::ImageCache::getFromMemory (BinaryData::faceplate_bg_png,
                                                    BinaryData::faceplate_bg_pngSize);

    // Helper: configure a rotary slider + attached label, adding to parent or editor
    auto setupSlider = [this] (juce::Slider& slider, juce::Label& label,
                               const juce::String& name, juce::Component* parent = nullptr)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);

        if (parent) parent->addAndMakeVisible (slider);
        else        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.attachToComponent (&slider, false); // false = label above slider

        if (parent) parent->addAndMakeVisible (label);
        else        addAndMakeVisible (label);
    };

    // Macro faceplate (always visible)
    setupSlider (driveSlider,  driveLabel,  "Drive");
    setupSlider (colorSlider,  colorLabel,  "Color");
    setupSlider (motionSlider, motionLabel, "Motion");

    // Pro panel (added to proPanel child component — hidden by default)
    setupSlider (resonanceSlider, resonanceLabel, "Resonance", &proPanel);
    setupSlider (lfoRateSlider,   lfoRateLabel,   "LFO Rate",  &proPanel);
    setupSlider (mixSlider,       mixLabel,       "Mix",       &proPanel);
    setupSlider (presenceSlider,  presenceLabel,  "Presence",  &proPanel);

    // APVTS attachments
    driveAttachment     = std::make_unique<SliderAttachment> (processorRef.apvts, "drive",     driveSlider);
    colorAttachment     = std::make_unique<SliderAttachment> (processorRef.apvts, "color",     colorSlider);
    motionAttachment    = std::make_unique<SliderAttachment> (processorRef.apvts, "motion",    motionSlider);
    resonanceAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "resonance", resonanceSlider);
    lfoRateAttachment   = std::make_unique<SliderAttachment> (processorRef.apvts, "lfoRate",   lfoRateSlider);
    mixAttachment       = std::make_unique<SliderAttachment> (processorRef.apvts, "mix",       mixSlider);
    presenceAttachment  = std::make_unique<SliderAttachment> (processorRef.apvts, "presence",  presenceSlider);

    // Pro panel toggle
    addAndMakeVisible (proPanelToggle);
    proPanelToggle.onClick = [this]
    {
        isProPanelVisible = proPanelToggle.getToggleState();
        proPanel.setVisible (isProPanelVisible);
        resized();
    };

    addChildComponent (proPanel); // hidden by default; shown via toggle

    // LFO LED — child of proPanel so it appears/disappears with the pro panel
    lfoLight.setLfoSource (&processorRef.lfoOutput);
    proPanel.addAndMakeVisible (lfoLight); // addAndMakeVisible so it shows when proPanel is shown

    // Preset ComboBox — IDs are 1-based (JUCE ComboBox convention)
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    // Reset button — restores all parameters to their default values
    resetButton.onClick = [this]
    {
        for (auto* param : processorRef.getParameters())
            param->setValueNotifyingHost (param->getDefaultValue());
        presetBox.setSelectedId (1, juce::dontSendNotification);
    };
    addAndMakeVisible (resetButton);

    // Melatonin inspector (dev utility)
    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&]
    {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    setSize (600, 400);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void PluginEditor::applyPreset (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return;

    const auto& p = presets[(size_t) index];

    // Convert real-unit values → normalised [0,1] before calling setValueNotifyingHost
    auto setParam = [this] (const juce::String& id, float value)
    {
        if (auto* param = processorRef.apvts.getParameter (id))
            param->setValueNotifyingHost (
                processorRef.apvts.getParameterRange (id).convertTo0to1 (value));
    };

    setParam ("drive",     p.drive);
    setParam ("color",     p.color);
    setParam ("motion",    p.motion);
    setParam ("lfoRate",   p.lfoRate);
    setParam ("resonance", p.resonance);
    setParam ("mix",       p.mix);
    setParam ("presence",  p.presence);
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    if (faceplateBg.isValid())
        g.drawImage (faceplateBg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour (0xff222222));

    if (isProPanelVisible)
    {
        g.setColour (juce::Colour (0xaa111111));
        g.fillRect (proPanel.getBounds());
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();

    // Top utility strip (30px): Reset | Preset ComboBox | Inspect
    {
        auto topStrip = area.removeFromTop (30);
        resetButton.setBounds   (topStrip.removeFromLeft  (60).reduced (2));
        inspectButton.setBounds (topStrip.removeFromRight (120).reduced (2));
        presetBox.setBounds     (topStrip.reduced (2));
    }

    // Pro panel toggle strip: fixed 34px anchored to bottom
    proPanelToggle.setBounds (area.removeFromBottom (34).reduced (5));

    // Pro panel region: bottom half when visible, zero-size when hidden
    if (isProPanelVisible)
        proPanel.setBounds (area.removeFromBottom (area.getHeight() / 2));
    else
        proPanel.setBounds ({});

    // --- Macro Faceplate: row FlexBox filling all remaining area ---
    // Labels attach ABOVE sliders (attachToComponent(&slider, false)).
    // 28px top margin guarantees the label clears the FlexItem boundary.
    const juce::FlexItem::Margin knobMargin (28.0f, 8.0f, 8.0f, 8.0f);

    juce::FlexBox macroFb;
    macroFb.flexDirection  = juce::FlexBox::Direction::row;
    macroFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    macroFb.alignItems     = juce::FlexBox::AlignItems::center;

    macroFb.items.add (juce::FlexItem (driveSlider) .withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));
    macroFb.items.add (juce::FlexItem (colorSlider) .withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));
    macroFb.items.add (juce::FlexItem (motionSlider).withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));

    macroFb.performLayout (area.toFloat());

    // --- Pro Panel: 4 knobs in a single row ---
    if (isProPanelVisible)
    {
        // Smaller knobs (min 70px) so all four fit comfortably at 600px width
        const juce::FlexItem::Margin proKnobMargin (28.0f, 6.0f, 8.0f, 6.0f);

        juce::FlexBox proFb;
        proFb.flexDirection  = juce::FlexBox::Direction::row;
        proFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        proFb.alignItems     = juce::FlexBox::AlignItems::center;

        proFb.items.add (juce::FlexItem (resonanceSlider).withFlex (1).withMinWidth (70.0f).withMinHeight (80.0f).withMargin (proKnobMargin));
        proFb.items.add (juce::FlexItem (lfoRateSlider)  .withFlex (1).withMinWidth (70.0f).withMinHeight (80.0f).withMargin (proKnobMargin));
        proFb.items.add (juce::FlexItem (mixSlider)      .withFlex (1).withMinWidth (70.0f).withMinHeight (80.0f).withMargin (proKnobMargin));
        proFb.items.add (juce::FlexItem (presenceSlider) .withFlex (1).withMinWidth (70.0f).withMinHeight (80.0f).withMargin (proKnobMargin));

        proFb.performLayout (proPanel.getLocalBounds().toFloat());

        // Place LFO LED as a 14×14 badge at top-right of LFO Rate knob
        // (both are children of proPanel, so .getBounds() is in proPanel-local coords)
        const auto lfoArea = lfoRateSlider.getBounds();
        lfoLight.setBounds (lfoArea.getRight() - 16,
                            lfoArea.getY() + 2,
                            14, 14);
    } // end if (isProPanelVisible)
}
