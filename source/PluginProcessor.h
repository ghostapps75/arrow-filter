#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    juce::AudioProcessorValueTreeState apvts;

    // Read-only from the UI thread (written by audio thread, polled at ~30 Hz by LfoLight)
    std::atomic<float> lfoOutput { 0.0f };

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP chain
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> filters;
    juce::dsp::WaveShaper<float>                            waveShaper;
    juce::dsp::Oscillator<float>                            lfo;
    juce::dsp::DryWetMixer<float>                           dryWetMixer;

    // Presence high shelf — one IIR filter per channel
    std::array<juce::IIRFilter, 2>                           presenceFilters;

    // Parameter pointers (audio-thread atomic reads)
    std::atomic<float>* driveParam     = nullptr;
    std::atomic<float>* colorParam     = nullptr;
    std::atomic<float>* motionParam    = nullptr;
    std::atomic<float>* lfoRateParam   = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* mixParam       = nullptr;
    std::atomic<float>* presenceParam  = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
