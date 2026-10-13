#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace tby::ui
{
    const juce::Colour tube { 0xff0d0b02 };   // the tube, switched on: olive black
    const juce::Colour yel  { 0xffffe23a };   // lemon yellow phosphor (INTERUPT's amber is the orange one)

    inline constexpr float kFontScale = 1.25f;
    juce::Font mono (float height);   // Share Tech Mono

    // A clock for blinking things. Owned by the editor.
    struct Skin
    {
        double t = 0.0;
        bool blink() const { return std::fmod (t, 1.0) < 0.55; }
    };

    // Text with a soft phosphor halo: a few faint offset copies, then the crisp one.
    void glowText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification,
                   const juce::Font&, juce::Colour = yel, float alpha = 1.0f);
    void glowStroke (juce::Graphics&, const juce::Path&, float width, juce::Colour = yel, float alpha = 1.0f);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (const Skin&);
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;

        const Skin& skin;
    };

    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label, bool bipolar = false);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::RangedAudioParameter* param = nullptr;
        juce::String label;
    };

    // The preset selector: < NAME >. Click the arrows, or the name for the next one.
    class PresetSelector : public juce::Component
    {
    public:
        explicit PresetSelector (TubbyProcessor& p) : proc (p) { setTitle ("Preset"); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void tick();

    private:
        TubbyProcessor& proc;
        int shown = -1;
    };

    // The monitor: the last four seconds of tape with the heads marked, and the two springs.
    class Monitor : public juce::Component
    {
    public:
        Monitor (TubbyProcessor&, const Skin&);
        void paint (juce::Graphics&) override;
        void tick();

    private:
        void paintFace (juce::Graphics&);

        TubbyProcessor& proc;
        const Skin& skin;
        std::array<float, tby::kViewCols> cols {};
        float delayMs = 375.0f, targetMs = 375.0f, loop = 0.0f, spring = 0.0f, send = 1.0f, bpm = 120.0f, fb = 0.6f;
        float crashGlow = 0.0f, coilPhase = 0.0f;
        bool sync = true, hold = false, dub = false;
        uint32_t lastCrashes = 0;
        juce::Random random;
    };

    // The name, with its own echoes trailing off to the right. They light up when you throw.
    class Title : public juce::Component
    {
    public:
        explicit Title (const Skin& s) : skin (s) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void tick (bool throwing, float loopLevel);
        static juce::Font font();

    private:
        const Skin& skin;
        float glow = 0.0f;
        bool lastBlink = false;
    };

    // CRT glass on top of everything: scan lines, vignette, and the boot sequence.
    class Glass : public juce::Component
    {
    public:
        explicit Glass (const Skin&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void boot (double now) { bootStart = now; }
        bool booting() const { return skin.t - bootStart < kBootSecs; }

        static constexpr double kBootSecs = 2.9;

    private:
        const Skin& skin;
        juce::Image lines;
        double bootStart = -100.0;
    };
}

class TubbyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TubbyEditor (TubbyProcessor&);
    ~TubbyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void pump (double dt = 1.0 / 30.0) { advance (dt); }   // for the offline snapshot
    void boot() { glass.boot (skin.t); }
    double clock() const { return skin.t; }

private:
    class Panel : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override { advance (1.0 / 30.0); }
    void advance (double dt);
    void momentary (juce::TextButton&, const char* paramId);

    TubbyProcessor& proc;
    tby::ui::Skin skin;
    tby::ui::LookAndFeel lnf { skin };
    Panel panel;
    tby::ui::Monitor monitor;
    tby::ui::PresetSelector presets;
    tby::ui::Title title { skin };
    tby::ui::Glass glass { skin };

    std::vector<std::unique_ptr<tby::ui::Knob>> knobs;
    tby::ui::Knob* timeKnob = nullptr;
    juce::TextButton syncButton { "SYNC" }, holdButton { "HOLD" }, altecButton { "ALTEC" };
    juce::TextButton openButton { "OPEN" }, dubButton { "DUB" }, kickButton { "KICK" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    bool lastBlink = false, wasBooting = false, lastSync = true;

    static constexpr int kBaseW = 1200, kBaseH = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubbyEditor)
};
