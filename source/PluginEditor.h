#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class ArrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArrowLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory (BinaryData::knob_graphic_png, BinaryData::knob_graphic_pngSize);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        if (knobImage.isValid())
        {
            auto radius = (float) juce::jmin (width / 2, height / 2);
            auto centreX = (float) x + (float) width  * 0.5f;
            auto centreY = (float) y + (float) height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            
            juce::AffineTransform transform = juce::AffineTransform::rotation (angle, (float)knobImage.getWidth() / 2.0f, (float)knobImage.getHeight() / 2.0f);
            
            float scale = rw / (float)knobImage.getWidth();
            transform = transform.scaled(scale).translated(rx, ry);

            g.drawImageTransformed(knobImage, transform, false);
        }
        else
        {
            juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
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
    PluginProcessor& processorRef;
    ArrowLookAndFeel customLookAndFeel;
    
    juce::Image faceplateBg;

    // UI Components
    juce::Slider driveSlider;
    juce::Slider colorSlider;
    juce::Slider motionSlider;
    juce::Label driveLabel;
    juce::Label colorLabel;
    juce::Label motionLabel;
    
    juce::Slider lfoRateSlider;
    juce::Slider resonanceSlider;
    juce::Label lfoRateLabel;
    juce::Label resonanceLabel;
    
    juce::ToggleButton proPanelToggle { "Pro Panel" };
    
    // The collapsible pro panel
    juce::Component proPanel;
    bool isProPanelVisible = false;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> colorAttachment;
    std::unique_ptr<SliderAttachment> motionAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
