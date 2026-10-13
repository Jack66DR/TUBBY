// Offline checks for the TUBBY engine. Build with -DTBY_BUILD_TESTS=ON.

#include "Engine.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

static constexpr double sr = 48000.0;
static constexpr int block = 256;
static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

struct Out { std::vector<float> l, r; uint32_t crashes = 0; };

using InFn = std::function<float (double t)>;
using ParFn = std::function<void (tby::Params&, double t)>;

static Out run (double seconds, InFn in, ParFn par, double bpm = 120.0)
{
    tby::Engine e;
    e.prepare (sr, 4321);
    Out o;
    const size_t total = (size_t) (seconds * sr);
    o.l.resize (total);
    o.r.resize (total);
    std::vector<float> il (block), ir (block);
    tby::Params p;
    tby::Clock c;
    c.bpm = bpm;
    for (size_t s = 0; s < total; s += block)
    {
        const int n = (int) std::min<size_t> (block, total - s);
        par (p, (double) s / sr);
        for (int i = 0; i < n; ++i)
            il[(size_t) i] = ir[(size_t) i] = in ((double) (s + (size_t) i) / sr);
        e.process (il.data(), ir.data(), o.l.data() + s, o.r.data() + s, n, p, c);
    }
    o.crashes = e.crashCount();
    return o;
}

static float rms (const std::vector<float>& x, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += (double) x[i] * x[i];
    return (float) std::sqrt (acc / (double) std::max<size_t> (1, b - a));
}

static float peak (const std::vector<float>& x, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    float m = 0.0f;
    for (size_t i = a; i < b; ++i) m = std::max (m, std::abs (x[i]));
    return m;
}

static double onset (const std::vector<float>& x, double t0, float thresh)
{
    for (size_t i = (size_t) (t0 * sr); i < x.size(); ++i)
        if (std::abs (x[i]) > thresh) return (double) i / sr;
    return -1.0;
}

static bool finite (const Out& o)
{
    for (size_t i = 0; i < o.l.size(); ++i)
        if (! std::isfinite (o.l[i]) || ! std::isfinite (o.r[i])) return false;
    return true;
}

// A short click: something with a front edge for the echoes to copy.
static float click (double t) { return t < 0.003 ? (float) (0.6 * std::sin (6.2831853 * 1000.0 * t)) : 0.0f; }

// Clean, dry settings to measure timing against.
static void clean (tby::Params& p)
{
    p.wobble = 0.0f; p.spring = 0.0f; p.noise = 0.0f; p.drive = 0.0f; p.tone = 1.0f; p.thin = 0.0f; p.spread = 1.0f;
}

int main()
{
    std::printf ("TUBBY offline engine test\n");

    {
        std::printf ("echo\n");
        auto o = run (3.0, click, [] (tby::Params& p, double) { clean (p); p.time = 6.0f / 11.0f; p.feedback = 0.6f; });   // 1/4 at 120
        const double first = onset (o.l, 0.0, 0.02f), second = onset (o.r, 0.0, 0.02f);
        std::printf ("    first echo %.4f s (left), second %.4f s (right)\n", first, second);
        check (std::abs (first - 0.5) < 0.002, "SYNC 1/4 at 120 BPM: first repeat at 500 ms");
        check (std::abs (second - 1.0) < 0.002, "ping-pong: the second repeat is on the other side, 500 ms later");
        check (peak (o.l, 0.0, 0.45) < 1.0e-4f, "wet only: nothing before the first repeat");
        const float p1 = peak (o.l, 0.45, 0.6), p3 = peak (o.l, 1.45, 1.6);
        std::printf ("    repeat 1 %.3f, repeat 3 %.3f\n", p1, p3);
        check (p3 < p1 && p3 > p1 * 0.1f, "FEEDBACK 60 %: repeats fall away, but not too fast");

        auto mono = run (1.5, click, [] (tby::Params& p, double) { clean (p); p.spread = 0.0f; p.time = 6.0f / 11.0f; });
        check (std::abs (peak (mono.l, 0.45, 0.6) - peak (mono.r, 0.45, 0.6)) < 1.0e-4f, "SPREAD 0: both sides the same");

        auto fr = run (1.0, click, [] (tby::Params& p, double) { clean (p); p.sync = false;
                                                                  p.time = (float) (std::log (300.0 / 30.0) / std::log (2000.0 / 30.0)); });
        const double t300 = onset (fr.l, 0.0, 0.02f);
        std::printf ("    FREE 300 ms: %.4f s\n", t300);
        check (std::abs (t300 - 0.3) < 0.002, "FREE: time in milliseconds");
    }

    {
        std::printf ("loop\n");
        auto run_ = run (24.0, click, [] (tby::Params& p, double) { p.feedback = 1.25f; p.noise = 0.0f; });
        const float lateRms = rms (run_.l, 20.0, 24.0);
        std::printf ("    runaway rms after 20 s %.3f, peak %.3f\n", lateRms, peak (run_.l, 0.0, 24.0));
        check (finite (run_), "runaway: finite");
        check (lateRms > 0.02f, "FEEDBACK 125 %: it sings on its own");
        check (peak (run_.l, 0.0, 24.0) <= 1.0f && peak (run_.r, 0.0, 24.0) <= 1.0f, "runaway: never over full scale");

        // darkness: sample to sample movement in the fifth repeat, bright loop against dark loop
        auto edge = [] (const Out& o, double t0, double t1)
        {
            double d = 0.0, a = 0.0;
            for (size_t i = (size_t) (t0 * sr) + 1; i < (size_t) (t1 * sr); ++i)
            { d += std::abs (o.l[i] - o.l[i - 1]); a += std::abs (o.l[i]); }
            return d / std::max (1.0e-9, a);
        };
        auto noiseIn = [] (double t) { static tby::Rng r; return t < 0.02 ? 0.5f * r.bi() : 0.0f; };
        auto bright = run (3.0, noiseIn, [] (tby::Params& p, double) { clean (p); p.feedback = 0.7f; p.time = 3.0f / 11.0f; p.tone = 1.0f; });
        auto dark = run (3.0, noiseIn, [] (tby::Params& p, double) { clean (p); p.feedback = 0.7f; p.time = 3.0f / 11.0f; p.tone = 0.1f; });
        const double eb = edge (bright, 1.2, 1.4), ed = edge (dark, 1.2, 1.4);
        std::printf ("    edge bright %.3f dark %.3f\n", eb, ed);
        check (ed < eb * 0.5, "TONE down: every repeat gets darker");

        auto held = run (12.0, [] (double t) { return t < 0.02 ? click (t) : (float) (0.5 * std::sin (6.28 * 220.0 * t)); },
                         [] (tby::Params& p, double t) { clean (p); p.feedback = 0.8f; p.time = 3.0f / 11.0f; p.hold = t > 0.3; });
        const float heldRms = rms (held.l, 10.0, 12.0);
        std::printf ("    held rms at 10 s %.4f\n", heldRms);
        check (heldRms > 0.01f && heldRms < 0.5f, "HOLD: the loop keeps going, the new input stays out");
    }

    {
        std::printf ("sends\n");
        auto shut = run (1.5, click, [] (tby::Params& p, double) { clean (p); p.open = false; });
        check (peak (shut.l, 0.0, 1.5) < 1.0e-4f, "OPEN off, DUB not held: nothing gets in");
        auto thrown = run (1.5, click, [] (tby::Params& p, double t) { clean (p); p.open = false; p.dub = t < 0.1; });
        check (peak (thrown.l, 0.3, 1.0) > 0.1f, "DUB held: the throw goes to tape");
        auto late = run (1.5, click, [] (tby::Params& p, double t) { clean (p); p.open = false; p.dub = t > 0.2; });
        check (peak (late.l, 0.0, 1.5) < 1.0e-4f, "DUB pressed after the hit: it missed, as it should");
    }

    {
        std::printf ("spring\n");
        auto sp = run (4.0, click, [] (tby::Params& p, double) { clean (p); p.feedback = 0.0f; p.open = false; p.spring = 1.0f; p.feed = 0.0f; });
        const float body = rms (sp.l, 0.05, 0.6), tail = rms (sp.l, 3.0, 4.0);
        std::printf ("    spring rms 50-600 ms %.4f, 3-4 s %.6f\n", body, tail);
        check (finite (sp) && body > 0.004f, "the spring answers a click");
        check (tail < body * 0.05f, "and dies away");
        check (std::abs (rms (sp.l, 0.05, 0.6) - rms (sp.r, 0.05, 0.6)) > 1.0e-5f, "two springs, not one");

        auto kicked = run (2.0, [] (double) { return 0.0f; }, [] (tby::Params& p, double t) { clean (p); p.spring = 1.0f; p.kick = t > 0.5 && t < 0.6; });
        check (kicked.crashes == 1 && rms (kicked.l, 0.5, 1.0) > 0.01f, "KICK crashes the springs, once per press");

        auto drums = [] (double t) { const double u = std::fmod (t, 0.5); return (float) (u < 0.004 ? 0.9 : 0.0); };
        auto shook = run (3.0, drums, [] (tby::Params& p, double) { clean (p); p.spring = 1.0f; p.shake = 1.0f; });
        auto still = run (3.0, drums, [] (tby::Params& p, double) { clean (p); p.spring = 1.0f; p.shake = 0.0f; });
        std::printf ("    shake crashes %u\n", shook.crashes);
        check (shook.crashes >= 4 && still.crashes == 0, "SHAKE: hard hits knock the springs");
    }

    {
        std::printf ("desk\n");
        auto low = [] (double t) { return t < 0.3 ? (float) (0.5 * std::sin (6.2831853 * 80.0 * t)) : 0.0f; };
        auto flat = run (1.2, low, [] (tby::Params& p, double) { clean (p); p.feedback = 0.0f; p.time = 3.0f / 11.0f; });
        auto hp = run (1.2, low, [] (tby::Params& p, double) { clean (p); p.feedback = 0.0f; p.time = 3.0f / 11.0f; p.filter = 0.8f; });
        std::printf ("    80 Hz echo flat %.3f, high passed %.4f\n", rms (flat.l, 0.3, 0.55), rms (hp.l, 0.3, 0.55));
        check (rms (hp.l, 0.3, 0.55) < rms (flat.l, 0.3, 0.55) * 0.1f, "FILTER right: the bass leaves the echoes");

        auto silent = run (1.0, [] (double) { return 0.0f; }, [] (tby::Params& p, double) { p.noise = 0.0f; });
        auto noisy = run (1.0, [] (double) { return 0.0f; }, [] (tby::Params& p, double) { p.noise = 0.6f; });
        std::printf ("    floor with NOISE 0.6: %.1f dBFS\n", 20.0 * std::log10 (std::max (1.0e-9f, rms (noisy.l, 0.2, 1.0))));
        check (rms (silent.l, 0.0, 1.0) < 1.0e-6f, "NOISE 0: silence in, silence out");
        check (rms (noisy.l, 0.2, 1.0) > 2.0e-4f, "NOISE up: the old gear hums and hisses");

        // a TIME move mid-note glides; no clicks
        auto sweep = run (3.0, [] (double t) { return (float) (0.4 * std::sin (6.2831853 * 330.0 * t)); },
                          [] (tby::Params& p, double t) { clean (p); p.feedback = 0.5f; p.time = t < 1.5 ? 3.0f / 11.0f : 9.0f / 11.0f; });
        float jump = 0.0f;
        for (size_t i = 1; i < sweep.l.size(); ++i) jump = std::max (jump, std::abs (sweep.l[i] - sweep.l[i - 1]));
        std::printf ("    biggest step during a TIME move %.3f\n", jump);
        check (finite (sweep) && jump < 0.25f, "moving TIME bends the pitch instead of clicking");
    }

    {
        std::printf ("abuse\n");
        auto w = run (10.0, [] (double t) { static tby::Rng r; return 0.9f * r.bi() * (std::fmod (t, 0.25) < 0.05 ? 1.0f : 0.0f); },
                      [] (tby::Params& p, double t) { p.feedback = 1.25f; p.drive = 1.0f; p.noise = 1.0f; p.spring = 1.0f; p.decay = 1.0f;
                                                      p.drip = 1.0f; p.shake = 1.0f; p.wobble = 1.0f; p.filter = (float) std::sin (t);
                                                      p.time = (float) (0.5 + 0.5 * std::sin (t * 3.0)); p.altec = t > 5.0; });
        check (finite (w) && peak (w.l, 0.0, 10.0) <= 1.0f, "everything up and moving: finite, under full scale");

        const auto t0 = std::chrono::steady_clock::now();
        run (60.0, [] (double t) { return (float) (0.3 * std::sin (t * 900.0)); }, [] (tby::Params& p, double) { p.spring = 0.6f; });
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("    60 s of audio in %.0f ms (%.0fx real time)\n", ms, 60000.0 / ms);
        check (ms < 15000.0, "cheap enough for a send");
    }

    std::printf (failures == 0 ? "ALL PASS\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
