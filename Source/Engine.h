// The TUBBY engine. Header only, no JUCE, so the offline test can drive it directly.
//
//   TAPE    two delay lines feeding each other: a ping-pong echo on one loop of tape. Moving TIME glides
//           like a tape machine changing speed, so the repeats bend. Wow and flutter on the heads.
//   LOOP    every repeat goes through tape saturation, a dark low pass, a thinning high pass and a bit of
//           hiss. FEEDBACK runs past 100 %, where it sings on its own; the saturation keeps it on the leash.
//   SPRING  two springs, slightly different lengths, one per side. A chain of stretched allpasses makes
//           the drip. KICK crashes them; SHAKE lets loud hits in the input crash them by themselves.
//   DESK    the big filter on the returns: high pass one way, low pass the other. ALTEC makes it click
//           between ten positions like the old stepped one.
//
// Wet only. The processor handles input gain, mix and output gain.
#pragma once

#include "Dsp.h"
#include <array>
#include <atomic>
#include <vector>

namespace tby
{
    constexpr double kTapeSeconds = 5.0;
    constexpr double kMaxDelaySeconds = 4.5;
    constexpr double kViewSeconds = 4.0;
    constexpr int kViewCols = 256;

    // TIME in SYNC mode picks one of these divisions (in quarter notes).
    constexpr int kDivs = 12;
    inline constexpr double kDivQ[kDivs] = { 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 3.0, 4.0 };
    inline constexpr const char* kDivName[kDivs] = { "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/2D", "1 BAR" };
    inline int divIndex (float t) { return std::clamp ((int) std::lround (t * (kDivs - 1)), 0, kDivs - 1); }
    // TIME in FREE mode: 30 ms to 2 s, log scaled.
    inline double freeMs (float t) { return 30.0 * std::pow (2000.0 / 30.0, (double) std::clamp (t, 0.0f, 1.0f)); }

    struct Params
    {
        // ECHO
        float time = 5.0f / 11.0f;   // 1/8D
        float feedback = 0.68f;      // 0 .. 1.25
        float tone = 0.35f;          // loop low pass
        float thin = 0.3f;           // loop high pass
        float wobble = 0.3f;
        float spread = 0.7f;
        // SPRING
        float spring = 0.35f, decay = 0.55f, tension = 0.5f, drip = 0.6f, feed = 0.6f, shake = 0.0f;
        // DESK
        float drive = 0.3f, noise = 0.25f, filter = 0.0f;
        bool sync = true, altec = false, hold = false, open = true, dub = false, kick = false;
    };

    struct Clock { double bpm = 120.0; };

    inline double delaySeconds (const Params& p, double bpm)
    {
        const double s = p.sync ? kDivQ[divIndex (p.time)] * 60.0 / std::max (20.0, bpm) : freeMs (p.time) / 1000.0;
        return std::clamp (s, 0.02, kMaxDelaySeconds);
    }

    // Tape saturation: soft, a little lopsided, never louder than about 0.9.
    inline float tapeSat (float x)
    {
        constexpr float bias = 0.08f;
        static const float off = std::tanh (bias);
        return (std::tanh (1.1f * x + bias) - off) / 1.1f;
    }

    // One spring: dispersion (a chain of stretched first-order allpasses) inside a delay loop.
    class Spring
    {
    public:
        static constexpr int kStages = 40;
        static constexpr int kMaxK = 8;

        void prepare (float sampleRate, float lengthMs, uint32_t seed)
        {
            sr = sampleRate;
            baseMs = lengthMs;
            buf.assign ((size_t) (0.16f * sr) + 8, 0.0f);
            K = std::clamp ((int) std::lround (4.0f * sr / 48000.0f), 2, kMaxK);
            rng.seed (seed);
            lfo = rng.uni();
            reset();
            inLp.setCutoff (4500.0f, sr);
            inHp.setCutoff (110.0f, sr);
            loopLp.setCutoff (3800.0f, sr);
            loopHp.setCutoff (70.0f, sr);
        }

        void reset()
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (auto& s : xh) s.fill (0.0f);
            for (auto& s : yh) s.fill (0.0f);
            idx = 0;
            w = 0;
            inLp.reset(); inHp.reset(); loopLp.reset(); loopHp.reset();
        }

        void set (float decay, float tension, float drip)
        {
            g = 0.55f + std::clamp (decay, 0.0f, 1.0f) * 0.38f;
            lenSamples = baseMs * (0.6f + 0.8f * std::clamp (tension, 0.0f, 1.0f)) * 0.001f * sr;
            a = 0.35f + std::clamp (drip, 0.0f, 1.0f) * 0.5f;
        }

        float process (float x)
        {
            const int size = (int) buf.size();
            x = inHp.process (inLp.process (x));

            lfo += 0.37f / sr;
            if (lfo >= 1.0f) lfo -= 1.0f;
            const double readPos = (double) w - (double) lenSamples * (1.0 + 0.0025 * std::sin (kTwoPi * lfo));
            const float r = readCircular (buf.data(), size, readPos);

            float v = x + g * loopHp.process (loopLp.process (r));
            const int back = (idx + 1) % K;   // the slot written K samples ago
            for (int s = 0; s < kStages; ++s)
            {
                const float y = a * v + xh[(size_t) s][(size_t) back] - a * yh[(size_t) s][(size_t) back];
                xh[(size_t) s][(size_t) idx] = v;
                yh[(size_t) s][(size_t) idx] = y;
                v = y;
            }
            idx = (idx + 1) % K;

            buf[(size_t) w] = v;
            if (++w >= size) w = 0;
            return 0.5f * (v + r);
        }

    private:
        float sr = 48000.0f, baseMs = 40.0f, lenSamples = 1900.0f, g = 0.8f, a = 0.6f, lfo = 0.0f;
        int K = 4, idx = 0, w = 0;
        std::vector<float> buf;
        std::array<std::array<float, kMaxK>, kStages> xh {}, yh {};
        OnePoleLP inLp, loopLp;
        OnePoleHP inHp, loopHp;
        Rng rng;
    };

    class Engine
    {
    public:
        void prepare (double sampleRate, uint32_t seed)
        {
            sr = sampleRate;
            const size_t len = (size_t) (kTapeSeconds * sr) + 8;
            tapeA.assign (len, 0.0f);
            tapeB.assign (len, 0.0f);
            w = 0;
            rng.seed (seed);
            springL.prepare ((float) sr, 37.0f, seed + 11u);
            springR.prepare ((float) sr, 43.5f, seed + 29u);
            for (auto* f : { &lpA1, &lpA2, &lpB1, &lpB2 }) f->reset();
            hpA.reset(); hpB.reset();
            dcA.reset(); dcB.reset();
            dcA.set ((float) sr); dcB.set ((float) sr);
            filtL.reset(); filtR.reset();
            fast.set (0.5f, 25.0f, (float) sr);
            slow.set (25.0f, 400.0f, (float) sr);
            fast.reset(); slow.reset();
            curD = -1.0;
            send = -1.0f;   // set from the switches on the first block
            wowPhase = rng.uni(); flutPhase = rng.uni();
            drift = driftTarget = 0.0f;
            crashT = 1.0e9f;
            cooldown = 0.0f;
            lastKick = false;
            humPhase = 0.0;
            colPeak = 0.0f; colCount = 0;
            samplesPerCol = std::max (1, (int) (kViewSeconds * sr / kViewCols));
            for (auto& c : uiCols) c.store (0.0f);
        }

        uint32_t crashCount() const { return uiCrashes.load(); }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p, const Clock& clock)
        {
            const float fsr = (float) sr;
            const double targetD = delaySeconds (p, clock.bpm) * sr;
            if (curD < 0.0) curD = targetD;
            const double glide = 1.0 - std::exp (-1.0 / (0.18 * sr));   // the tape changes speed, it doesn't jump

            // the loop, set once per block
            const float lpHz = 600.0f * std::pow (15.0f, std::clamp (p.tone, 0.0f, 1.0f));   // 600 Hz .. 9 kHz
            const float hpHz = 20.0f * std::pow (45.0f, std::clamp (p.thin, 0.0f, 1.0f));    // 20 Hz .. 900 Hz
            for (auto* f : { &lpA1, &lpA2, &lpB1, &lpB2 }) f->setCutoff (p.hold ? std::max (lpHz, 6000.0f) : lpHz, fsr);
            hpA.setCutoff (p.hold ? 20.0f : hpHz, fsr);
            hpB.setCutoff (p.hold ? 20.0f : hpHz, fsr);
            const float fb = p.hold ? 1.0f : std::clamp (p.feedback, 0.0f, 1.25f);
            const float sendTarget = p.hold ? 0.0f : (p.open || p.dub) ? 1.0f : 0.0f;
            const float sendK = 1.0f - std::exp (-1.0f / (0.008f * fsr));
            if (send < 0.0f) send = sendTarget;
            const float drive = 1.0f + std::clamp (p.drive, 0.0f, 1.0f) * 6.0f;
            const float driveNorm = 1.0f / std::sqrt (drive);
            const float hiss = std::clamp (p.noise, 0.0f, 1.0f);
            const float wowDepth = std::clamp (p.wobble, 0.0f, 1.0f) * 0.0045f * fsr;
            const float flutDepth = std::clamp (p.wobble, 0.0f, 1.0f) * 0.00022f * fsr;
            const float spread = std::clamp (p.spread, 0.0f, 1.0f);
            const float springAmt = std::clamp (p.spring, 0.0f, 1.0f) * 1.3f;
            const float feed = std::clamp (p.feed, 0.0f, 1.0f);
            springL.set (p.decay, p.tension, p.drip);
            springR.set (p.decay * 0.97f, p.tension, p.drip);

            // the big filter
            float f = std::clamp (p.filter, -1.0f, 1.0f);
            if (p.altec) f = std::round (f * 10.0f) / 10.0f;
            const bool filtOn = std::abs (f) > 0.015f;
            const bool high = f > 0.0f;
            if (high)
            {
                filtL.set (40.0f * std::pow (100.0f, f), 0.9f + 0.7f * f, fsr);
                filtR.set (40.0f * std::pow (100.0f, f), 0.9f + 0.7f * f, fsr);
            }
            else
            {
                filtL.set (14000.0f * std::pow (350.0f / 14000.0f, -f), 0.8f + 0.6f * -f, fsr);
                filtR.set (14000.0f * std::pow (350.0f / 14000.0f, -f), 0.8f + 0.6f * -f, fsr);
            }

            // KICK: crash the springs on the press
            if (p.kick && ! lastKick)
                crash (1.0f);
            lastKick = p.kick;

            const int size = (int) tapeA.size();
            float loopEnv = uiLoop.load (std::memory_order_relaxed), springEnv = uiSpring.load (std::memory_order_relaxed);
            const float envFall = std::exp (-1.0f / (0.25f * fsr));

            for (int i = 0; i < n; ++i)
            {
                // ---- the heads: glide toward the target, then wow and flutter
                curD += (targetD - curD) * glide;
                wowPhase += 0.55f / fsr;            if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                flutPhase += 6.3f / fsr;            if (flutPhase >= 1.0f) flutPhase -= 1.0f;
                if (++driftCount >= 2400) { driftCount = 0; driftTarget = rng.bi(); }
                drift += (driftTarget - drift) * (1.0f / (0.6f * fsr));
                const float mod = wowDepth * (0.5f + 0.5f * std::sin (kTwoPi * wowPhase) * 0.7f + 0.3f * drift)
                                + flutDepth * (1.0f + std::sin (kTwoPi * flutPhase));
                const double rp = (double) w - curD - (double) mod;
                const float rA = readCircular (tapeA.data(), size, rp);
                const float rB = readCircular (tapeB.data(), size, rp);

                // ---- input: the preamp, then the send (open, or only while DUB is held)
                const float inMono = 0.5f * (inL[i] + inR[i]);
                const float pre = std::tanh (inMono * drive) * driveNorm;
                send += (sendTarget - send) * sendK;

                // ---- the loop: each line hears the other, through the tape
                float loopA = tapeSat (rB * fb);
                loopA = hpA.process (lpA2.process (lpA1.process (loopA)));
                loopA = dcA.process (loopA) + hiss * 0.0025f * rng.bi();
                float loopB = tapeSat (rA * fb);
                loopB = hpB.process (lpB2.process (lpB1.process (loopB)));
                loopB = dcB.process (loopB) + hiss * 0.0025f * rng.bi();

                const float wA = pre * send + loopA;
                tapeA[(size_t) w] = wA;
                tapeB[(size_t) w] = loopB;
                if (++w >= size) w = 0;

                // ---- the returns, ping to pong
                const float mid = 0.5f * (rA + rB), side = 0.5f * (rA - rB) * spread;
                float eL = mid + side, eR = mid - side;

                // ---- springs: hear the dry input, the echoes, or a mix, plus any crash
                float crashS = 0.0f;
                if (crashT < 0.2f)
                {
                    crashS = crashAmp * (rng.bi() * std::exp (-crashT / 0.006f) * 0.9f
                                         + std::sin (kTwoPi * 110.0f * crashT) * std::exp (-crashT / 0.02f) * 0.6f);
                    crashT += 1.0f / fsr;
                }
                const float sIn = (1.0f - feed) * pre * 0.8f + feed * mid + crashS;
                const float sL = springL.process (sIn), sR = springR.process (sIn);
                float wL = eL + sL * springAmt, wR = eR + sR * springAmt;

                // SHAKE: a sharp hit in the input knocks the springs
                const float fe = fast.process (inMono), se = slow.process (inMono);
                cooldown -= 1.0f / fsr;
                if (p.shake > 0.02f && cooldown <= 0.0f && fe > 0.06f && fe > se * 2.5f)
                    crash (std::clamp (p.shake * fe * 2.5f, 0.0f, 1.0f));

                // ---- the desk
                filtL.process (wL);
                filtR.process (wR);
                if (filtOn)
                {
                    wL = high ? filtL.hp : filtL.lp;
                    wR = high ? filtR.hp : filtR.lp;
                }

                // ---- the room the old gear lives in: hiss and mains hum under everything
                humPhase += 50.0 / sr;
                if (humPhase >= 1.0) humPhase -= 1.0;
                const float hum = std::sin (kTwoPi * (float) humPhase) + 0.35f * std::sin (kTwoPi * 3.0f * (float) humPhase);
                const float floorN = hiss * (0.0012f * hum + 0.0018f * rng.bi());
                outL[i] = softClip (wL + floorN);
                outR[i] = softClip (wR + floorN);

                // ---- telemetry
                const float aw = std::abs (wA);
                colPeak = std::max (colPeak, aw);
                if (++colCount >= samplesPerCol)
                {
                    const int c = uiColWrite.load (std::memory_order_relaxed);
                    uiCols[(size_t) c].store (colPeak, std::memory_order_relaxed);
                    uiColWrite.store ((c + 1) % kViewCols, std::memory_order_release);
                    colPeak = 0.0f; colCount = 0;
                }
                loopEnv = std::max (std::abs (loopA) + std::abs (loopB), loopEnv * envFall);
                springEnv = std::max (std::abs (sL) + std::abs (sR), springEnv * envFall);
            }

            uiLoop.store (loopEnv, std::memory_order_relaxed);
            uiSpring.store (springEnv, std::memory_order_relaxed);
            uiDelayMs.store ((float) (curD / sr * 1000.0), std::memory_order_relaxed);
            uiTargetMs.store ((float) (targetD / sr * 1000.0), std::memory_order_relaxed);
            uiSend.store (send, std::memory_order_relaxed);
            uiBpm.store ((float) clock.bpm, std::memory_order_relaxed);
        }

        // ---- telemetry for the editor
        std::array<std::atomic<float>, kViewCols> uiCols {};
        std::atomic<int> uiColWrite { 0 };
        std::atomic<float> uiLoop { 0.0f }, uiSpring { 0.0f }, uiDelayMs { 375.0f }, uiTargetMs { 375.0f };
        std::atomic<float> uiSend { 1.0f }, uiBpm { 120.0f };
        std::atomic<uint32_t> uiCrashes { 0 };

    private:
        void crash (float amp)
        {
            crashT = 0.0f;
            crashAmp = amp;
            cooldown = 0.12f;
            uiCrashes.fetch_add (1, std::memory_order_relaxed);
        }

        double sr = 48000.0, curD = -1.0, humPhase = 0.0;
        std::vector<float> tapeA, tapeB;
        int w = 0, driftCount = 0, colCount = 0, samplesPerCol = 750;
        float send = 1.0f, wowPhase = 0.0f, flutPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f;
        float crashT = 1.0e9f, crashAmp = 0.0f, cooldown = 0.0f, colPeak = 0.0f;
        bool lastKick = false;
        OnePoleLP lpA1, lpA2, lpB1, lpB2;
        OnePoleHP hpA, hpB;
        DcBlockLite dcA, dcB;
        Spring springL, springR;
        Svf filtL, filtR;
        EnvFollower fast, slow;
        Rng rng;
    };
}
