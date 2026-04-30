#include "PluginProcessor.h"
#include "PluginEditor.h"

ImpulseDeltaMIDI8000XTAudioProcessor::ImpulseDeltaMIDI8000XTAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ImpulseDeltaMIDI8000XTAudioProcessor::~ImpulseDeltaMIDI8000XTAudioProcessor()
{
}

const juce::String ImpulseDeltaMIDI8000XTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ImpulseDeltaMIDI8000XTAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ImpulseDeltaMIDI8000XTAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ImpulseDeltaMIDI8000XTAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ImpulseDeltaMIDI8000XTAudioProcessor::getTailLengthSeconds() const
{
    return 0.;
}

int ImpulseDeltaMIDI8000XTAudioProcessor::getNumPrograms()
{
    return 1;
}

int ImpulseDeltaMIDI8000XTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ImpulseDeltaMIDI8000XTAudioProcessor::setCurrentProgram (int)
{
}

const juce::String ImpulseDeltaMIDI8000XTAudioProcessor::getProgramName (int)
{
    return {};
}

void ImpulseDeltaMIDI8000XTAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void ImpulseDeltaMIDI8000XTAudioProcessor::prepareToPlay (double, int)
{
}

void ImpulseDeltaMIDI8000XTAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ImpulseDeltaMIDI8000XTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
	const auto set = layouts.getMainOutputChannelSet();
	const auto mono = juce::AudioChannelSet::mono();
	const auto stereo = juce::AudioChannelSet::stereo();
	return set == mono || set == stereo;
}
#endif

void ImpulseDeltaMIDI8000XTAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;
    buffer.clear();
	const auto numChannels = buffer.getNumChannels();
    auto samples = buffer.getArrayOfWritePointers();
	for (const auto& it : midi)
	{
		const auto ts = it.samplePosition;
		const auto msg = it.getMessage();
		if (msg.isNoteOn())
			for (auto ch = 0; ch < numChannels; ++ch)
				samples[ch][ts] = 1.f;
	}
}

bool ImpulseDeltaMIDI8000XTAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ImpulseDeltaMIDI8000XTAudioProcessor::createEditor()
{
    return new ImpulseDeltaMIDI8000XTAudioProcessorEditor (*this);
}

void ImpulseDeltaMIDI8000XTAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
}

void ImpulseDeltaMIDI8000XTAudioProcessor::setStateInformation (const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ImpulseDeltaMIDI8000XTAudioProcessor();
}
