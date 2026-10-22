#include "GlitchRelic.h"
#include "PlaygroundWidgets.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::ui
{
namespace
{
// The chrome environment: a dark violet sky, an ultraviolet band, a pale-chrome sun, a hard horizon, a graphite floor and a pink bounce. u in 0..1.
juce::Colour environment (float u)
{
    static const juce::ColourGradient grad = []
    {
        juce::ColourGradient g (juce::Colour (0xff06030c), 0.0f, 0.0f, juce::Colour (0xff06030c), 1.0f, 0.0f, false);
        g.clearColours();
        g.addColour (0.00, juce::Colour (0xff050209));
        g.addColour (0.10, juce::Colour (0xff1a0838));
        g.addColour (0.22, juce::Colour (0xff7a1fd0));
        g.addColour (0.30, juce::Colour (0xffc026ff));
        g.addColour (0.36, juce::Colour (0xfff0e6ff));
        g.addColour (0.455, juce::Colour (0xffffffff));
        g.addColour (0.49, juce::Colour (0xffd8c8e8));
        g.addColour (0.50, juce::Colour (0xff0b0712)); // the horizon: a hard step from white to near black
        g.addColour (0.56, juce::Colour (0xff2a1c3a));
        g.addColour (0.66, juce::Colour (0xff6a4a8c));
        g.addColour (0.76, juce::Colour (0xffff3dae));
        g.addColour (0.82, juce::Colour (0xffb42a86));
        g.addColour (0.90, juce::Colour (0xff3a1044));
        g.addColour (1.00, juce::Colour (0xff050209));
        return g;
    }();
    return grad.getColourAtPosition (juce::jlimit (0.0, 1.0, (double) u));
}

float smooth (float a, float b, float x)
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

struct Sample { juce::Point<float> p, n; float z, twist, halfWidth; };

// One knotted ribbon: a trefoil path stretched to the plate, with a width that breathes along its length.
std::vector<Sample> knot (float W, float H, float stretch, float phase, float widthScale)
{
    constexpr int count = 2200;
    std::vector<Sample> out ((size_t) count);
    const float sy = H * 0.13f, sx = sy * stretch, cx = W * 0.5f, cy = H * 0.468f; // placed so the two ends, which the display plate leaves visible, sit at mid-height
    auto pt = [&] (float t)
    {
        const float x = std::sin (t + phase) + 2.0f * std::sin (2.0f * t + 2.0f * phase);
        const float y = std::cos (t + phase) - 2.0f * std::cos (2.0f * t + 2.0f * phase);
        return juce::Point<float> (cx + sx * x, cy + sy * y);
    };
    for (int i = 0; i < count; ++i)
    {
        const float t = juce::MathConstants<float>::twoPi * (float) i / (float) count;
        const auto p = pt (t), a = pt (t - 0.002f), b = pt (t + 0.002f);
        auto tangent = b - a;
        const float len = std::max (1.0e-4f, std::hypot (tangent.x, tangent.y));
        tangent = { tangent.x / len, tangent.y / len };
        auto& s = out[(size_t) i];
        s.p = p;
        s.n = { -tangent.y, tangent.x };
        s.z = -std::sin (3.0f * (t + phase));
        s.twist = 3.0f * t + std::atan2 (tangent.y, tangent.x);
        s.halfWidth = H * 0.062f * widthScale * (0.70f + 0.30f * std::sin (2.0f * t + 0.8f));
    }
    return out;
}

void drawRibbon (juce::Graphics& g, const std::vector<Sample>& s, float rimAlpha)
{
    struct Item { int i; float z; };
    std::vector<Item> order;
    order.reserve (s.size());
    for (size_t i = 0; i < s.size(); ++i)
        order.push_back ({ (int) i, s[i].z });
    std::stable_sort (order.begin(), order.end(), [] (const Item& a, const Item& b) { return a.z < b.z; }); // far parts first, so the ribbon passes over and under itself
    const int n = (int) s.size();
    constexpr int slices = 5;
    for (const auto& it : order)
    {
        const auto& a = s[(size_t) it.i];
        const auto& b = s[(size_t) ((it.i + 1) % n)];
        const float ua = 0.5f + std::asin (std::sin (a.twist)) / juce::MathConstants<float>::pi, ub = 0.5f + std::asin (std::sin (b.twist)) / juce::MathConstants<float>::pi; // a triangle wave: every part of the environment is reflected equally often
        const auto la = a.p - a.n * a.halfWidth, lb = b.p - b.n * b.halfWidth, ra = a.p + a.n * a.halfWidth, rb = b.p + b.n * b.halfWidth;
        // five slices across the width, each reflecting a different part of the environment: a rounded, polished surface rather than a flat strip
        for (int k = 0; k < slices; ++k)
        {
            const float f0 = (float) k / slices, f1 = (float) (k + 1) / slices;
            const auto p0 = la + (ra - la) * f0, p1 = la + (ra - la) * f1, q0 = lb + (rb - lb) * f0, q1 = lb + (rb - lb) * f1;
            juce::Path quad;
            quad.startNewSubPath (p0); quad.lineTo (p1); quad.lineTo (q1); quad.lineTo (q0); quad.closeSubPath();
            const float across = ((float) k + 0.5f) / slices - 0.5f; // -0.5 .. 0.5
            const float u = 0.5f * (ua + ub) + 0.34f * across + 0.10f * std::sin (6.0f * across);
            g.setColour (environment (u - 0.008f).interpolatedWith (environment (u + 0.008f), 0.5f)); // two taps: the hard horizon is anti-aliased instead of stair-stepped
            g.fillPath (quad);
            g.strokePath (quad, juce::PathStrokeType (0.6f));
        }
        // a specular streak on the edge that faces the light (upper left): a thin pale line, strongest where the ribbon turns toward it
        {
            const juce::Point<float> light (-0.5f, -0.86f);
            const float d = a.n.x * light.x + a.n.y * light.y, e = b.n.x * light.x + b.n.y * light.y;
            const float k = std::pow (std::abs (d), 3.0f) * 0.85f;
            const float fa = d > 0.0f ? 0.8f : 0.2f, fb = e > 0.0f ? 0.8f : 0.2f;
            const auto sa = la + (ra - la) * fa, sb = lb + (rb - lb) * fb;
            g.setColour (juce::Colour (0xffffffff).withAlpha (juce::jmin (0.9f, k)));
            g.drawLine (sa.x, sa.y, sb.x, sb.y, 1.5f);
        }
        // rim light: ultraviolet on one edge, cyber pink on the other
        g.setColour (juce::Colour (0xffc026ff).withAlpha (rimAlpha));
        g.drawLine (la.x, la.y, lb.x, lb.y, 1.2f);
        g.setColour (juce::Colour (0xffff3dae).withAlpha (rimAlpha));
        g.drawLine (ra.x, ra.y, rb.x, rb.y, 1.2f);
    }
}
}

juce::Image renderGlitchRelic (int width, int height)
{
    width = std::max (64, width);
    height = std::max (32, height);
    const float W = (float) width, H = (float) height;

    // the ribbons on their own transparent layer
    juce::Image ribbons (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (ribbons);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        drawRibbon (g, knot (W, H, 5.35f, 0.0f, 1.9f), 0.85f);
        // a second, thinner ribbon through the first: more depth, a different phase
        drawRibbon (g, knot (W, H, 5.35f, 1.05f, 0.9f), 0.6f);
    }

    juce::Image out (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (out);
        // a soft violet haze behind the form
        juce::ColourGradient haze (juce::Colour (0xffc026ff).withAlpha (0.20f), W * 0.5f, H * 0.5f, juce::Colour (0xff08060d).withAlpha (0.0f), W * 0.5f, 0.0f, true);
        haze.addColour (0.6, juce::Colour (0xff6a1aa8).withAlpha (0.07f));
        g.setGradientFill (haze);
        g.fillRect (0.0f, 0.0f, W, H);
        // depth: the form casts a soft dark shadow downward onto the haze
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), (int) (H * 0.035f), { 0, (int) (H * 0.03f) }).drawForImage (g, ribbons);
        // glow: a blurred copy of the form in ultraviolet, then a smaller one in pink
        juce::DropShadow (juce::Colour (0xffc026ff).withAlpha (0.38f), (int) (H * 0.06f), { 0, 0 }).drawForImage (g, ribbons);
        juce::DropShadow (juce::Colour (0xffff3dae).withAlpha (0.22f), (int) (H * 0.02f), { (int) (H * 0.012f), (int) (H * 0.02f) }).drawForImage (g, ribbons);
        g.setOpacity (1.0f); // (setColour above set the drawing opacity to the shadow's alpha)
        g.drawImageAt (ribbons, 0, 0);
    }

    // fade to nothing at the very border so the form floats in its frame instead of being cut off by it
    juce::Image::BitmapData data (out, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < height; ++y)
    {
        const float fy = smooth (0.0f, 0.07f, (float) y / H) * smooth (0.0f, 0.07f, 1.0f - (float) y / H);
        for (int x = 0; x < width; ++x)
        {
            const float fx = smooth (0.0f, 0.04f, (float) x / W) * smooth (0.0f, 0.04f, 1.0f - (float) x / W);
            auto c = data.getPixelColour (x, y);
            data.setPixelColour (x, y, c.withMultipliedAlpha (fx * fy));
        }
    }
    return out;
}

namespace
{
juce::Image& cachedRelic()
{
    static juce::Image image; // message thread only
    return image;
}
}

void GlitchRelicLoader::start (std::function<void (const juce::Image&)> onReady)
{
    shared->ready = std::move (onReady);
    if (cachedRelic().isValid())
    {
        shared->ready (cachedRelic());
        return;
    }
    startThread (juce::Thread::Priority::low);
}

void GlitchRelicLoader::stop()
{
    shared->alive = false; // a message already posted will find this and do nothing
    stopThread (15000);
}

void GlitchRelicLoader::run()
{
    auto image = renderGlitchRelic (1824, 472);
    if (threadShouldExit())
        return;
    juce::MessageManager::callAsync ([sh = shared, img = std::move (image)]
    {
        cachedRelic() = img;
        if (sh->alive && sh->ready)
            sh->ready (img);
    });
}
}
