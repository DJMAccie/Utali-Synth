#include "SynthVoice.h"

SynthVoice::SynthVoice() {
    filter.setMode(juce::dsp::LadderFilterMode::LPF12);
}

void SynthVoice::setVoiceIndex(int index) {
    vIndex = index;
    random.setSeed(static_cast<juce::int64>(index * 1337 + 42));
    float spread = (static_cast<float>(index) / 7.0f) - 0.5f;
    setPan(spread * 0.6f);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) {
    currentFreq = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    level = velocity * 0.5f;

    adsr.reset();
    for (int i = 0; i < MAX_UNISON; ++i) {
        float detune = 1.0f + ((static_cast<float>(i) - (static_cast<float>(currentUnison) - 1.0f) / 2.0f) * 0.0015f);
        unisonOscs[static_cast<size_t>(i)].setFrequency(currentFreq * detune, currentSampleRate);
        unisonOscs[static_cast<size_t>(i)].reset();
    }

    subOsc.setFrequency(currentFreq * 0.5f, currentSampleRate);
    subOsc.reset();

    adsr.noteOn();
    isFastFading = false;
    fastFadeGain = 1.0f;

    float randomPan = (random.nextFloat() - 0.5f) * 0.2f;
    setPan(basePan + randomPan);
}

void SynthVoice::stopNote(float, bool allowTailOff) {
    adsr.noteOff();
    if (!allowTailOff) {
        isFastFading = true;
        fastFadeGain = 1.0f;
    }
}

void SynthVoice::updateParams(const SynthVoiceParameters& params) {
    smoothedCutoff.setTargetValue(params.cutoff);
    smoothedResonance.setTargetValue(params.resonance);
    driftIntensity = params.age;
    smoothedSubMix.setTargetValue(params.subLevel);
    currentUnison = juce::jlimit(1, MAX_UNISON, params.unisonCount);
    currentWaveType = params.waveType;

    adsrParams.attack = std::max(params.attack, 0.002f);
    adsrParams.decay = params.decay;
    // Quadratic taper: prevents 1% from sounding disproportionately loud
    adsrParams.sustain = params.sustain * params.sustain;
    adsrParams.release = std::max(params.release, 0.01f);
    adsr.setParameters(adsrParams);
}

void SynthVoice::prepareToPlay(double sampleRate, int samplesPerBlock, int) {
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1 };
    filter.prepare(spec);
    adsr.setSampleRate(sampleRate);

    smoothedCutoff.reset(sampleRate, 0.02);
    smoothedSubMix.reset(sampleRate, 0.02);
    smoothedResonance.reset(sampleRate, 0.02);
    smoothedPanLeft.reset(sampleRate, 0.02);
    smoothedPanRight.reset(sampleRate, 0.02);

    fastFadeDecrement = 1.0f / static_cast<float>(std::max(1.0, 0.005 * sampleRate));
    tempBuffer.setSize(1, std::max(samplesPerBlock * 2, 8192));
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) {
    if (!isVoiceActive()) return;

    jassert(numSamples <= tempBuffer.getNumSamples());
    numSamples = std::min(numSamples, tempBuffer.getNumSamples());

    tempBuffer.clear(0, 0, numSamples);

    driftPhase += driftRate * static_cast<float>(numSamples);
    if (driftPhase >= juce::MathConstants<float>::twoPi)
        driftPhase = std::fmod(driftPhase, juce::MathConstants<float>::twoPi);
    float drift = std::sin(driftPhase) * driftIntensity * 2.0f;

    for (int i = 0; i < currentUnison; ++i) {
        float detune = 1.0f + ((static_cast<float>(i) - (static_cast<float>(currentUnison) - 1.0f) / 2.0f) * 0.0015f);
        unisonOscs[static_cast<size_t>(i)].setFrequency(currentFreq * detune + drift, currentSampleRate);
    }

    auto* mainData = tempBuffer.getWritePointer(0);
    const float unisonGain = 1.0f / static_cast<float>(currentUnison);

    for (int s = 0; s < numSamples; ++s) {
        float unisonSum = 0.0f;
        for (int u = 0; u < currentUnison; ++u) {
            unisonSum += unisonOscs[static_cast<size_t>(u)].renderSample(currentWaveType);
        }
        unisonSum *= unisonGain;
        float subSample = subOsc.renderSample(1) * smoothedSubMix.getNextValue();
        mainData[s] = unisonSum + subSample;
    }

    // Process filter in 32-sample sub-blocks for sample-accurate smooth cutoff tracking
    int samplesRemaining = numSamples;
    int subOffset = 0;
    while (samplesRemaining > 0) {
        int chunkSize = std::min(samplesRemaining, 32);
        smoothedCutoff.skip(chunkSize);
        smoothedResonance.skip(chunkSize);
        filter.setCutoffFrequencyHz(smoothedCutoff.getCurrentValue());
        filter.setResonance(smoothedResonance.getCurrentValue());

        juce::dsp::AudioBlock<float> subBlock(tempBuffer.getArrayOfWritePointers(), 1, static_cast<size_t>(subOffset), static_cast<size_t>(chunkSize));
        filter.process(juce::dsp::ProcessContextReplacing<float>(subBlock));

        subOffset += chunkSize;
        samplesRemaining -= chunkSize;
    }

    adsr.applyEnvelopeToBuffer(tempBuffer, 0, numSamples);
    tempBuffer.applyGain(0, 0, numSamples, level);

    if (isFastFading) {
        for (int s = 0; s < numSamples; ++s) {
            mainData[s] *= fastFadeGain;
            fastFadeGain -= fastFadeDecrement;
            if (fastFadeGain <= 0.0f) {
                fastFadeGain = 0.0f;
                juce::FloatVectorOperations::clear(mainData + s + 1, numSamples - (s + 1));
                break;
            }
        }
    }

    const int numOutChannels = outputBuffer.getNumChannels();
    auto* outL = outputBuffer.getWritePointer(0, startSample);
    auto* outR = (numOutChannels > 1) ? outputBuffer.getWritePointer(1, startSample) : nullptr;

    for (int s = 0; s < numSamples; ++s) {
        const float panL = smoothedPanLeft.getNextValue();
        const float panR = smoothedPanRight.getNextValue();
        const float sample = mainData[s];

        outL[s] += sample * panL;
        if (outR != nullptr)
            outR[s] += sample * panR;
    }

    if (!adsr.isActive() || (isFastFading && fastFadeGain <= 0.0f)) {
        isFastFading = false;
        clearCurrentNote();
    }
}

void SynthVoice::setPan(float panPosition) {
    panPosition = juce::jlimit(-1.0f, 1.0f, panPosition);
    basePan = panPosition;
    float angle = (panPosition + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    smoothedPanLeft.setTargetValue(std::cos(angle));
    smoothedPanRight.setTargetValue(std::sin(angle));
}
