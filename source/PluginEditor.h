#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
// LfoLight — pulsating cyan LED that tracks the LFO oscillator at ~30 Hz.
// Must be a child of proPanel so it appears/disappears with the panel.
class LfoLight : public juce::Component, private juce::Timer
{
public:
    LfoLight()  { startTimerHz (30); }
    ~LfoLight() override { stopTimer(); }

    void setLfoSource (const std::atomic<float>* src) noexcept { lfoSource = src; }

    void paint (juce::Graphics& g) override
    {
        // Safely read LFO value written by the audio thread
        const float lfoVal = lfoSource ? lfoSource->load (std::memory_order_relaxed) : 0.0f;
        const float t      = (lfoVal + 1.0f) * 0.5f; // remap [-1, 1] → [0, 1]

        auto  bounds = getLocalBounds().toFloat().reduced (1.5f);
        auto  centre = bounds.getCentre();
        float r      = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        // Outer diffuse glow
        g.setColour (juce::Colour (0xff00f5ff).withAlpha (t * 0.35f));
        g.fillEllipse (bounds);

        // Bright core
        g.setColour (juce::Colour (0xff00f5ff).withAlpha (0.25f + t * 0.75f));
        g.fillEllipse (bounds.reduced (r * 0.3f));

        // White specular highlight (moves in sync with brightness)
        g.setColour (juce::Colours::white.withAlpha (t * 0.55f));
        g.fillEllipse (centre.x - r * 0.25f, centre.y - r * 0.35f,
                       r * 0.35f, r * 0.35f);
    }

    void timerCallback() override { repaint(); }

private:
    const std::atomic<float>* lfoSource = nullptr;
};

//==============================================================================
// ArrowLookAndFeel — custom knob renderer with 3-pass neon glow arc.
class ArrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArrowLookAndFeel()
    {
        // Use the newly provided high-res knob asset
        knobImage = juce::ImageCache::getFromMemory (BinaryData::knob_png,
                                                      BinaryData::knob_pngSize);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override
    {
        const float radius  = (float) juce::jmin (width / 2, height / 2);
        const float centreX = (float) x + (float) width  * 0.5f;
        const float centreY = (float) y + (float) height * 0.5f;
        const float angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // ── 1. NEON ARC (drawn before the knob so it glows underneath) ──────────
        // Only draw an arc when the slider is above its minimum position
        if (sliderPos > 0.001f)
        {
            juce::Path arcPath;
            arcPath.addCentredArc (centreX, centreY,
                                   radius + 4.0f,   // 4 px outside the knob edge
                                   radius + 4.0f,
                                   0.0f,             // no additional rotation on the arc
                                   rotaryStartAngle,
                                   angle,
                                   true);            // startAsNewSubPath

            // Pass 1 — wide magenta outer halo (diffuse atmospheric glow)
            g.setColour (juce::Colour (0xffff00ff).withAlpha (0.12f));
            g.strokePath (arcPath, juce::PathStrokeType (9.0f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 2 — medium cyan mid-glow
            g.setColour (juce::Colour (0xff00f5ff).withAlpha (0.35f));
            g.strokePath (arcPath, juce::PathStrokeType (5.0f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 3 — thin bright cyan core (the actual neon tube)
            g.setColour (juce::Colour (0xff00f5ff));
            g.strokePath (arcPath, juce::PathStrokeType (1.5f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── 2. ROTATED KNOB IMAGE (drawn on top of the arc glow) ───────────────
        if (knobImage.isValid())
        {
            const float rx = centreX - radius;
            const float ry = centreY - radius;
            const float rw = radius * 2.0f;

            // Rotate around the exact centre of the source image (square asset assumed)
            juce::AffineTransform transform =
                juce::AffineTransform::rotation (angle,
                                                  (float) knobImage.getWidth()  * 0.5f,
                                                  (float) knobImage.getHeight() * 0.5f);

            // Scale to fit slider bounds, then translate to correct screen position
            const float scale = rw / (float) knobImage.getWidth();
            transform = transform.scaled (scale).translated (rx, ry);

            g.drawImageTransformed (knobImage, transform, false);
        }
        else
        {
            // Fallback if binary data is missing
            juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height,
                                                     sliderPos, rotaryStartAngle,
                                                     rotaryEndAngle, slider);
        }
    }

private:
    juce::Image knobImage;
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applyPreset (int index);

    PluginProcessor& processorRef;
    ArrowLookAndFeel customLookAndFeel;

    juce::Image faceplateBg;

    // Macro faceplate knobs (always visible)
    juce::Slider driveSlider,  colorSlider,  motionSlider;
    juce::Label  driveLabel,   colorLabel,   motionLabel;

    // Pro panel knobs (hidden until toggle)
    juce::Slider resonanceSlider, lfoRateSlider, mixSlider, presenceSlider;
    juce::Label  resonanceLabel,  lfoRateLabel,  mixLabel,  presenceLabel;

    // LFO LED — child of proPanel, pulses in sync with the LFO oscillator
    LfoLight lfoLight;

    // Top strip controls
    juce::ComboBox   presetBox;
    juce::TextButton resetButton { "Reset" };

    // Pro panel toggle + container
    juce::ToggleButton proPanelToggle { "Pro Panel" };
    juce::Component    proPanel;
    bool isProPanelVisible = false;

    // APVTS attachments (declared after sliders — destroyed first)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> colorAttachment;
    std::unique_ptr<SliderAttachment> motionAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> presenceAttachment;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
