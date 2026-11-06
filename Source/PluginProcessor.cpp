#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <vector>

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout ZCrushAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crush", 1 }, "Crush",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    parameters.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "bits", 1 }, "Bit Depth",
        NormalisableRange<float> (1.0f, 16.0f, 0.1f), 8.0f));

    parameters.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "rate", 1 }, "Sample Rate",
        NormalisableRange<float> (1.0f, 32.0f, 0.1f, 0.5f), 4.0f));

    parameters.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Dry/Wet",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    parameters.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "out", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    parameters.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    return { parameters.begin(), parameters.end() };
}

ZCrushAudioProcessor::ZCrushAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pCrush  = apvts.getRawParameterValue ("crush");
    pBits   = apvts.getRawParameterValue ("bits");
    pRate   = apvts.getRawParameterValue ("rate");
    pMix    = apvts.getRawParameterValue ("mix");
    pOut    = apvts.getRawParameterValue ("out");
    pBypass = apvts.getRawParameterValue ("bypass");
}

void ZCrushAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const auto safeSampleRate = jmax (1.0, sampleRate);

    crushSm.reset (safeSampleRate, 0.02);
    bitsSm.reset  (safeSampleRate, 0.02);
    rateSm.reset  (safeSampleRate, 0.02);
    mixSm.reset   (safeSampleRate, 0.02);
    gainSm.reset  (safeSampleRate, 0.02);
    bypassSm.reset (safeSampleRate, 0.02);

    crushSm.setCurrentAndTargetValue (pCrush->load());
    bitsSm.setCurrentAndTargetValue  (pBits->load());
    rateSm.setCurrentAndTargetValue  (pRate->load());
    mixSm.setCurrentAndTargetValue   (pMix->load() * 0.01f);
    gainSm.setCurrentAndTargetValue  (Decibels::decibelsToGain (pOut->load()));
    bypassSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    held[0] = held[1] = 0.0f;
    phase[0] = phase[1] = 1.0f;
    level.store (0.0f);
}

bool ZCrushAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input  = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (output != AudioChannelSet::mono() && output != AudioChannelSet::stereo())
        return false;

    return input == output;
}

void ZCrushAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numChannels = jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    // Clear any output channels that do not have a matching input channel.
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    crushSm.setTargetValue (pCrush->load());
    bitsSm.setTargetValue  (pBits->load());
    rateSm.setTargetValue  (pRate->load());
    mixSm.setTargetValue   (pMix->load() * 0.01f);
    gainSm.setTargetValue  (Decibels::decibelsToGain (pOut->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    float* channelData[2] { nullptr, nullptr };
    for (int channel = 0; channel < numChannels; ++channel)
        channelData[channel] = buffer.getWritePointer (channel);

    float peak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float crush = jlimit (0.0f, 1.0f, crushSm.getNextValue() * 0.01f);
        const float bits  = bitsSm.getNextValue();
        const float rate  = rateSm.getNextValue();
        const float mix   = mixSm.getNextValue();
        const float gain  = gainSm.getNextValue();
        const float bypass = bypassSm.getNextValue();

        // Crush at zero is genuinely transparent: no quantisation or sample holding.
        // At higher settings the user value controls both bit depth and sample-rate reduction.
        float step = 0.0f;
        float increment = 1.0f;

        if (crush > 0.00001f)
        {
            const float effectiveBits = 16.0f + (bits - 16.0f) * crush;
            const float rateFactor = std::pow (jmax (1.0f, rate), crush);
            const float levels = std::pow (2.0f, effectiveBits);

            step = 2.0f / levels;
            increment = 1.0f / jmax (1.0f, rateFactor);
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = channelData[channel][sample];
            float crushed = dry;

            if (crush > 0.00001f)
            {
                phase[channel] += increment;
                if (phase[channel] >= 1.0f)
                {
                    phase[channel] -= 1.0f;
                    held[channel] = step * std::round (dry / step);
                }
                crushed = held[channel];
            }
            else
            {
                // Keep the sample-hold state current so re-engaging Crush cannot
                // briefly replay a stale sample from the previous setting.
                held[channel] = dry;
                phase[channel] = 1.0f;
            }

            const float effected = (dry * (1.0f - mix) + crushed * mix) * gain;
            const float output = effected + (dry - effected) * bypass;

            channelData[channel][sample] = output;
            peak = jmax (peak, std::abs (output));
        }
    }

    // Fast attack, block-rate release. The editor applies a second visual smoothing pass.
    const float previousPeak = level.load();
    level.store (jmax (peak, previousPeak * 0.82f));
}

AudioProcessorEditor* ZCrushAudioProcessor::createEditor()
{
    return new ZCrushAudioProcessorEditor (*this);
}

void ZCrushAudioProcessor::getStateInformation (MemoryBlock& destination)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void ZCrushAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZCrushAudioProcessor();
}
