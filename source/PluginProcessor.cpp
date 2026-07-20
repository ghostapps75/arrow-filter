#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Drive: tamed to 1–6 (was 10); default 1.5 gives warm saturation without nuclear clipping
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (1.0f, 6.0f, 0.01f), 1.5f));

    // Color: HPF floor raised to 80 Hz; default at 8 kHz (vocal presence zone, not wide open)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "color", 1 }, "Color",
        juce::NormalisableRange<float> (80.0f, 18000.0f, 1.0f, 0.3f), 8000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "motion", 1 }, "Motion",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoRate", 1 }, "LFO Rate",
        juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.5f), 1.0f));

    // Resonance: capped at 4.0 (was 10.0) — eliminates the blip/chirp at high Q
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float> (0.5f, 4.0f, 0.01f), 0.8f));

    // Mix: dry/wet blend — the core "mask imperfect singing" control
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    // Presence: high shelf at 8 kHz — adds air, draws ear away from pitch issues
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "presence", 1 }, "Presence",
        juce::NormalisableRange<float> (-6.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    driveParam     = apvts.getRawParameterValue ("drive");
    colorParam     = apvts.getRawParameterValue ("color");
    motionParam    = apvts.getRawParameterValue ("motion");
    lfoRateParam   = apvts.getRawParameterValue ("lfoRate");
    resonanceParam = apvts.getRawParameterValue ("resonance");
    mixParam       = apvts.getRawParameterValue ("mix");
    presenceParam  = apvts.getRawParameterValue ("presence");

    waveShaper.functionToUse = [] (float x) { return std::tanh (x); };
    lfo.initialise ([] (float x) { return std::sin (x); });
}

PluginProcessor::~PluginProcessor() {}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const { return 0.0; }

int PluginProcessor::getNumPrograms()    { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }

void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels     = (juce::uint32) getTotalNumOutputChannels();

    waveShaper.prepare (spec);
    lfo.prepare (spec);
    dryWetMixer.prepare (spec);

    for (auto& filter : filters)
    {
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    // Presence filters: clear internal state on prepare (juce::IIRFilter needs no spec)
    for (auto& pf : presenceFilters)
        pf.reset();
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Load all parameters once per block (realtime-safe atomic reads)
    const float drive      = driveParam->load     (std::memory_order_relaxed);
    const float color      = colorParam->load     (std::memory_order_relaxed);
    const float motion     = motionParam->load    (std::memory_order_relaxed);
    const float lfoRateVal = lfoRateParam->load   (std::memory_order_relaxed);
    const float resVal     = resonanceParam->load (std::memory_order_relaxed);
    const float mixVal     = mixParam->load       (std::memory_order_relaxed);
    const float presenceDb = presenceParam->load  (std::memory_order_relaxed);

    // Block-rate DSP state updates
    lfo.setFrequency (lfoRateVal);
    dryWetMixer.setWetMixProportion (mixVal);

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) filters.size());

    // Pre-compute presence high shelf coefficients once per block
    const float presenceGain = juce::Decibels::decibelsToGain (presenceDb);
    const auto presCoeffs = juce::IIRCoefficients::makeHighShelf (
        getSampleRate(), 8000.0f, 0.707f, presenceGain);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        filters[ch].setResonance (resVal);
        presenceFilters[ch].setCoefficients (presCoeffs);
    }

    // --- Signal chain ---

    // 1. Capture dry signal BEFORE any processing (DryWetMixer internal copy)
    juce::dsp::AudioBlock<float> block (buffer);
    dryWetMixer.pushDrySamples (block);

    // 2. Drive → tanh WaveShaper → gain compensation (block-level)
    juce::dsp::ProcessContextReplacing<float> context (block);
    buffer.applyGain (drive);
    waveShaper.process (context);
    buffer.applyGain (1.0f / drive);

    // 3. LFO → exponential filter cutoff modulation (sample-by-sample)
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float lfoVal = lfo.processSample (0.0f); // [-1.0, 1.0]
        lfoOutput.store (lfoVal, std::memory_order_relaxed); // expose to UI (last sample per block is fine at 30 Hz)
        // ±4 octaves max sweep depth, scaled by Motion
        float modulatedCutoff = color * std::pow (2.0f, lfoVal * motion * 4.0f);
        modulatedCutoff = juce::jlimit (20.0f, 20000.0f, modulatedCutoff);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            filters[ch].setCutoffFrequency (modulatedCutoff);
            const float in  = buffer.getSample (ch, sample);
            const float out = filters[ch].processSample (0, in);
            buffer.setSample (ch, sample, out);
        }
    }

    // 4. Presence high shelf (block-level, in-place per channel)
    for (int ch = 0; ch < numChannels; ++ch)
        presenceFilters[ch].processSamples (buffer.getWritePointer (ch), buffer.getNumSamples());

    // 5. Blend processed wet back with captured dry
    dryWetMixer.mixWetSamples (block);
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
