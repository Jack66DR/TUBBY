// TUBBY_Tool
//   render <out.wav> [preset number] [open|throw]   plays a one drop riff through the plugin at 75 BPM and writes a WAV
//   snapshot <out.png> [crt|runaway|boot]          renders the editor to a PNG for a layout check

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

// A pretend host transport, so SYNC locks to a tempo like it would in Logic.
struct Transport : juce::AudioPlayHead
{
    double bpm = 75.0, sampleRate = 48000.0;
    int64_t sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        p.setTimeInSamples (sample);
        p.setPpqPosition ((double) sample / sampleRate * bpm / 60.0);
        return p;
    }
};

static void setParam (TubbyProcessor& p, const char* id, float value)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

// One drop at 75 BPM: organ skanks on the offbeats, a rimshot on three.
static float riff (double t)
{
    const double beat = 60.0 / 75.0;
    const double twoPi = juce::MathConstants<double>::twoPi;
    double x = 0.0;

    // skank: a short, choppy chord on every "and"
    const double u = std::fmod (t + beat * 0.5, beat);
    if (u < 0.14)
    {
        const double env = std::min (1.0, u * 600.0) * std::exp (-u * 22.0);
        for (double hz : { 293.66, 349.23, 440.0 })   // D minor
            x += 0.25 * env * (std::sin (twoPi * hz * u) + 0.5 * std::sin (twoPi * hz * 2.0 * u) + 0.25 * std::sin (twoPi * hz * 3.0 * u));
    }

    // rimshot on three of every bar
    const double bar = beat * 4.0;
    const double r = std::fmod (t - beat * 2.0 + bar * 8.0, bar);
    if (r < 0.06)
    {
        static juce::Random rnd (7);
        const double env = std::exp (-r * 90.0);
        x += 0.7 * env * (0.6 * std::sin (twoPi * 1700.0 * r) + 0.4 * (rnd.nextFloat() * 2.0 - 1.0));
    }
    return (float) (0.5 * x);
}

static int render (const juce::File& out, int preset, const juce::String& mode)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    const double seconds = 32.0;
    const double beat = 60.0 / 75.0;

    TubbyProcessor proc;
    Transport transport;
    proc.setPlayHead (&transport);
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.setCurrentProgram (preset);
    setParam (proc, ParamIDs::mix, 0.45f);   // the demo is an insert, so you hear the riff and the dub together
    if (mode == "throw")
        setParam (proc, ParamIDs::open, 0.0f);
    proc.prepareToPlay (sr, block);

    const int total = (int) (seconds * sr);
    const int playFor = (int) (22.0 * sr);   // the band stops; the tape doesn't
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    for (int s = 0; s < total; s += block)
    {
        const int n = juce::jmin (block, total - s);
        const double t = (double) s / sr;
        if (mode == "throw")
        {
            // throw the rimshot on three of every second bar
            const double inBar = std::fmod (t, beat * 8.0);
            setParam (proc, ParamIDs::dub, inBar > beat * 5.9 && inBar < beat * 6.15 ? 1.0f : 0.0f);
        }
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float x = s + i < playFor ? riff ((double) (s + i) / sr) : 0.0f;
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        transport.sample = s;
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            all.copyFrom (ch, s, buf, ch, 0, n);
    }

    const float pk = all.getMagnitude (0, total);
    std::printf ("preset %s (%s): peak %.3f (%.1f dBFS)\n", proc.getProgramName (preset).toRawUTF8(), mode.toRawUTF8(), pk,
                 juce::Decibels::gainToDecibels (pk));
    for (int sec = 0; sec < (int) seconds; sec += 4)
        std::printf ("  %2d-%2d s rms %.1f dBFS\n", sec, sec + 4,
                     juce::Decibels::gainToDecibels (all.getRMSLevel (0, (int) (sec * sr), (int) (4 * sr)), -120.0f));

    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (out.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0));
    if (w == nullptr)
        return 1;
    w->writeFromAudioSampleBuffer (all, 0, total);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}

static int snapshot (const juce::File& out, const juce::String& look)
{
    juce::ScopedJuceInitialiser_GUI gui;
    TubbyProcessor proc;
    Transport transport;
    proc.setPlayHead (&transport);
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.setCurrentProgram (look == "runaway" ? 4 : 1);
    if (look == "runaway")
        setParam (proc, ParamIDs::altec, 1.0f);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    proc.booted = true;   // the boot snapshot asks for it explicitly
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1200, 800);
    auto* editor = dynamic_cast<TubbyEditor*> (ed.get());

    const int blocks = 700;
    for (int b = 0; b < blocks; ++b)
    {
        if (look == "boot" && editor != nullptr && b == blocks - 120)
            editor->boot();
        if (b == blocks - 20)
            setParam (proc, ParamIDs::kick, 1.0f);   // a crash, so the coils are moving
        for (int i = 0; i < 512; ++i)
        {
            const float x = riff ((double) (b * 512 + i) / 48000.0);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        transport.sample = (int64_t) b * 512;
        proc.processBlock (buf, midi);
        if (b % 3 == 0 && editor != nullptr)
            editor->pump();
    }

    // land on the "on" half of the blink so lamps show
    if (editor != nullptr && look != "boot")
        for (int k = 0; k < 30; ++k)
        {
            editor->pump (0.02);
            if (std::fmod (editor->clock(), 1.0) < 0.3) break;
        }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: TUBBY_Tool render <out.wav> [preset number] [open|throw]\n"
                     "       TUBBY_Tool snapshot <out.png> [crt|runaway|boot]\n");
        return 2;
    }
    const juce::String mode (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));
    if (mode == "render")
        return render (out, argc > 3 ? juce::String (argv[3]).getIntValue() : 0,
                       argc > 4 ? juce::String (argv[4]) : juce::String ("open"));
    if (mode == "snapshot")
        return snapshot (out, argc > 3 ? juce::String (argv[3]) : juce::String ("crt"));
    return 2;
}
