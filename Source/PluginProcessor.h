#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

namespace ParamIDs
{
    inline constexpr const char* time     = "time";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* tone     = "tone";
    inline constexpr const char* thin     = "thin";
    inline constexpr const char* wobble   = "wobble";
    inline constexpr const char* spread   = "spread";
    inline constexpr const char* spring   = "spring";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* tension  = "tension";
    inline constexpr const char* drip     = "drip";
    inline constexpr const char* feed     = "feed";
    inline constexpr const char* shake    = "shake";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* noise    = "noise";
    inline constexpr const char* filter   = "filter";
    inline constexpr const char* in       = "in";
    inline constexpr const char* out      = "out";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* sync     = "sync";
    inline constexpr const char* hold     = "hold";
    inline constexpr const char* altec    = "altec";
    inline constexpr const char* open     = "open";
    inline constexpr const char* dub      = "dub";
    inline constexpr const char* kick     = "kick";
}

class TubbyProcessor : public juce::AudioProcessor
{
public:
    TubbyProcessor();
    ~TubbyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TUBBY"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    // Factory presets: they set the sound, never IN, OUT, MIX or the switches you play.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // TIME reads differently in SYNC and FREE, so its text needs to see the SYNC switch.
    struct SyncLink { std::atomic<std::atomic<float>*> raw { nullptr }; };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (std::shared_ptr<SyncLink>);

    std::shared_ptr<SyncLink> syncLink = std::make_shared<SyncLink>();
    juce::AudioProcessorValueTreeState apvts;
    tby::Engine engine;   // the editor reads its telemetry

    // The boot sequence plays the first time the window opens, not every time.
    bool booted = false;

private:
    juce::AudioBuffer<float> inBuf, wetBuf;
    juce::SmoothedValue<float> inGainS, outGainS, mixS;

    enum P { pTime, pFeedback, pTone, pThin, pWobble, pSpread, pSpring, pDecay, pTension, pDrip, pFeed, pShake,
             pDrive, pNoise, pFilter, pIn, pOut, pMix, pSync, pHold, pAltec, pOpen, pDub, pKick, kNumP };
    std::atomic<float>* p[kNumP] {};
    float get (P which) const { return p[which]->load(); }
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubbyProcessor)
};
