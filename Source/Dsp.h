// Small DSP helpers shared by the TUBBY engine. Header only, no JUCE needed.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tby
{
    constexpr float kPi = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717959f;

    // Fast deterministic random, so every instance can be seeded differently.
    struct Rng
    {
        uint32_t s = 0x9e3779b9u;
        void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; }
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
        float bi() { return uni() * 2.0f - 1.0f; }                               // -1..1
        int below (int n) { return (int) (uni() * (float) n) % std::max (1, n); }
    };

    // Clean up to the knee, then a smooth ceiling at 1. The last line of defence on the output
    // and inside the feedback loop.
    inline float softClip (float x)
    {
        constexpr float knee = 0.8f;
        const float a = std::abs (x);
        if (a <= knee)
            return x;
        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }

    // 4-point, 3rd-order Hermite.
    inline float hermite (float xm1, float x0, float x1, float x2, float t)
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }

    // Circular buffer read with fractional position (any real index, wraps).
    inline float readCircular (const float* buf, int size, double pos)
    {
        const double fl = std::floor (pos);
        const float t = (float) (pos - fl);
        int i = (int) std::fmod (fl, (double) size);
        if (i < 0) i += size;
        const int im1 = i == 0 ? size - 1 : i - 1;
        const int i1 = i + 1 == size ? 0 : i + 1;
        const int i2 = i1 + 1 == size ? 0 : i1 + 1;
        return hermite (buf[im1], buf[i], buf[i1], buf[i2], t);
    }

    inline double wrap (double x, double size)
    {
        x = std::fmod (x, size);
        return x < 0.0 ? x + size : x;
    }

    struct OnePoleLP
    {
        float z = 0.0f, a = 1.0f;
        void setCutoff (float hz, float sr) { a = 1.0f - std::exp (-kTwoPi * std::min (hz, sr * 0.45f) / sr); }
        float process (float x) { z += a * (x - z); return z; }
        void reset() { z = 0.0f; }
    };

    struct OnePoleHP
    {
        OnePoleLP lp;
        void setCutoff (float hz, float sr) { lp.setCutoff (hz, sr); }
        float process (float x) { return x - lp.process (x); }
        void reset() { lp.reset(); }
    };

    // Topology-preserving state variable filter (Zavalishin), low pass and high pass outputs.
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, g = 0.1f, k = 1.4f;
        float lp = 0.0f, hp = 0.0f;
        void set (float hz, float q, float sr)
        {
            g = std::tan (kPi * std::clamp (hz, 10.0f, sr * 0.45f) / sr);
            k = 1.0f / q;
        }
        void process (float x)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + g * a1 * v3;
            const float v2 = ic2 + g * v1;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            hp = x - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = lp = hp = 0.0f; }
    };

    struct EnvFollower
    {
        float env = 0.0f, att = 0.0f, rel = 0.0f;
        void set (float attackMs, float releaseMs, float sr)
        {
            att = std::exp (-1.0f / (0.001f * attackMs * sr));
            rel = std::exp (-1.0f / (0.001f * releaseMs * sr));
        }
        float process (float x)
        {
            const float a = std::abs (x);
            const float c = a > env ? att : rel;
            env = a + c * (env - a);
            return env;
        }
        void reset() { env = 0.0f; }
    };

    // Equal power pan, p in -1..1.
    inline void panGains (float p, float& gl, float& gr)
    {
        const float a = (std::clamp (p, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
        gl = std::cos (a);
        gr = std::sin (a);
    }

    // Gentle DC blocker for inside the echo loop, so the lopsided tape saturation can't walk off centre.
    struct DcBlockLite
    {
        float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
        void set (float sr) { r = 1.0f - kTwoPi * 8.0f / sr; }
        float process (float x)
        {
            const float y = x - x1 + r * y1;
            x1 = x;
            y1 = y;
            return y;
        }
        void reset() { x1 = y1 = 0.0f; }
    };
}
