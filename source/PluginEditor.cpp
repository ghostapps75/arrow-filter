#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel(&customLookAndFeel);
    
    faceplateBg = juce::ImageCache::getFromMemory (BinaryData::faceplate_bg_png, BinaryData::faceplate_bg_pngSize);

    auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& name, juce::Component* parent = nullptr) {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        
        if (parent) parent->addAndMakeVisible(slider);
        else addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.attachToComponent (&slider, false);
        
        if (parent) parent->addAndMakeVisible(label);
        else addAndMakeVisible (label);
    };

    setupSlider (driveSlider, driveLabel, "Drive");
    setupSlider (colorSlider, colorLabel, "Color");
    setupSlider (motionSlider, motionLabel, "Motion");
    
    setupSlider (lfoRateSlider, lfoRateLabel, "LFO Rate", &proPanel);
    setupSlider (resonanceSlider, resonanceLabel, "Resonance", &proPanel);

    driveAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "drive", driveSlider);
    colorAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "color", colorSlider);
    motionAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "motion", motionSlider);
    lfoRateAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "lfoRate", lfoRateSlider);
    resonanceAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "resonance", resonanceSlider);

    addAndMakeVisible (proPanelToggle);
    proPanelToggle.onClick = [this] {
        isProPanelVisible = proPanelToggle.getToggleState();
        proPanel.setVisible (isProPanelVisible);
        resized();
    };

    addChildComponent (proPanel);

    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&] {
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
    setLookAndFeel(nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    if (faceplateBg.isValid())
        g.drawImage (faceplateBg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour (0xff222222)); // Fallback

    if (isProPanelVisible)
    {
        auto proBounds = proPanel.getBounds();
        g.setColour (juce::Colour (0xaa111111)); // Translucent overlay
        g.fillRect (proBounds);
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();

    // Utility strip: inspect button pinned to top-right, 30px tall.
    inspectButton.setBounds (area.removeFromTop (30).removeFromRight (120).reduced (2));

    // Pro Panel toggle strip: fixed 34px, anchored to the bottom of the editor.
    proPanelToggle.setBounds (area.removeFromBottom (34).reduced (5));

    // Pro Panel region: occupies the bottom half of remaining space when visible,
    // or collapses to zero so getLocalBounds() is always valid before the inner layout.
    if (isProPanelVisible)
        proPanel.setBounds (area.removeFromBottom (area.getHeight() / 2));
    else
        proPanel.setBounds ({});

    // --- Macro Faceplate: row FlexBox filling all remaining area ---
    // Labels are attached ABOVE each slider via attachToComponent(&slider, false).
    // The 28px top margin ensures the label always clears the FlexItem boundary.
    // withMinHeight(100) covers the 80px knob minimum + 20px text box below.
    const juce::FlexItem::Margin knobMargin (28.0f, 8.0f, 8.0f, 8.0f); // top, right, bottom, left

    juce::FlexBox macroFb;
    macroFb.flexDirection  = juce::FlexBox::Direction::row;
    macroFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    macroFb.alignItems     = juce::FlexBox::AlignItems::center;

    macroFb.items.add (juce::FlexItem (driveSlider) .withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));
    macroFb.items.add (juce::FlexItem (colorSlider) .withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));
    macroFb.items.add (juce::FlexItem (motionSlider).withFlex (1).withMinWidth (80.0f).withMinHeight (100.0f).withMargin (knobMargin));

    macroFb.performLayout (area.toFloat());

    // --- Pro Panel inner layout (only performed when the panel is open) ---
    // Labels above pro knobs also get the 28px top margin budget.
    if (isProPanelVisible)
    {
        const juce::FlexItem::Margin proKnobMargin (28.0f, 15.0f, 8.0f, 15.0f);

        juce::FlexBox proFb;
        proFb.flexDirection  = juce::FlexBox::Direction::row;
        proFb.justifyContent = juce::FlexBox::JustifyContent::center;
        proFb.alignItems     = juce::FlexBox::AlignItems::center;

        proFb.items.add (juce::FlexItem (lfoRateSlider)  .withWidth (120.0f).withHeight (120.0f).withMargin (proKnobMargin));
        proFb.items.add (juce::FlexItem (resonanceSlider).withWidth (120.0f).withHeight (120.0f).withMargin (proKnobMargin));

        proFb.performLayout (proPanel.getLocalBounds().toFloat());
    }
}
