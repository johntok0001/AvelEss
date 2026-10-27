#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Theme.h"
#include "../DSP/Engine.h"

namespace avel::ui
{
    namespace ess
    {
        const juce::Colour teal  { 0xff66d4cf };
        const juce::Colour blue  { 0xff0a84ff };
        const juce::Colour gr    { 0xffff375f };
        constexpr float labelStrip = 36.0f;   // right-hand dB label column shared by both graphs
    }

    //==============================================================================
    // Scrolling history: input level, detection level, gain reduction and a draggable threshold line.
    class HistoryView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int capacity = 2048;
        static constexpr float minDb = -60.0f;

        HistoryView() { setTooltip ("Drag the line to set the threshold. Double-click resets it."); }

        void bindThreshold (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            thrParam = &p;
            thr = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { thresholdDb = v; repaint(); }, um);
            thr->sendInitialUpdate();
        }

        void push (const avel::dsp::Frame& f)
        {
            frames[(size_t) head] = f;
            head = (head + 1) % capacity;
            count = juce::jmin (capacity, count + 1);
            peakGr = juce::jmax (peakGr, f.grDb);
        }

        void tick()
        {
            approach (hoverAmt, (hoverLine || dragging) ? 1.0f : 0.0f, 0.3f);
            repaint();
        }

        juce::Rectangle<float> plotArea() const
        {
            return getLocalBounds().toFloat().withTrimmedRight (ess::labelStrip).withTrimmedTop (34.0f).withTrimmedBottom (6.0f);
        }

        void paint (juce::Graphics& g) override
        {
            const auto pa = plotArea();
            const float pxDb = pa.getHeight() / -minDb;
            auto yOf = [&] (float db) { return pa.getY() + juce::jlimit (0.0f, -minDb, -db) * pxDb; };

            // grid
            for (int db = 0; db >= (int) minDb; db -= 6)
            {
                const float y = yOf ((float) db);
                const bool major = db % 12 == 0;
                g.setColour (juce::Colours::white.withAlpha (major ? 0.06f : 0.03f));
                g.fillRect (pa.getX(), std::round (y), pa.getWidth(), 1.0f);
                if (major)
                {
                    g.setColour (col::label3);
                    g.setFont (font (10.5f, Weight::medium));
                    g.drawText (juce::String (db), juce::Rectangle<float> (pa.getRight() + 4.0f, y - 7.0f, ess::labelStrip - 8.0f, 14.0f),
                                juce::Justification::centredRight, false);
                }
            }

            const float step = 1.6f;
            const int visible = juce::jmin (count, (int) (pa.getWidth() / step) + 2);
            if (visible > 1)
            {
                juce::Path in, det, red;
                auto frameAt = [&] (int age) -> const avel::dsp::Frame& { return frames[(size_t) ((head - 1 - age + capacity * 2) % capacity)]; };
                const float right = pa.getRight();
                in.startNewSubPath (right, pa.getBottom());
                det.startNewSubPath (right, pa.getBottom());
                red.startNewSubPath (right, pa.getY());
                for (int a = 0; a < visible; ++a)
                {
                    const auto& f = frameAt (a);
                    const float x = right - (float) a * step;
                    in.lineTo (x, yOf (f.inDb));
                    det.lineTo (x, yOf (f.detDb));
                    red.lineTo (x, pa.getY() + juce::jmin (f.grDb, 30.0f) * pxDb);
                }
                const float left = right - (float) (visible - 1) * step;
                in.lineTo (left, pa.getBottom()); in.closeSubPath();
                det.lineTo (left, pa.getBottom()); det.closeSubPath();
                red.lineTo (left, pa.getY()); red.closeSubPath();

                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (pa.toNearestInt());

                // input
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), 0, pa.getY(), juce::Colours::white.withAlpha (0.03f), 0, pa.getBottom(), false));
                g.fillPath (in);
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.strokePath (in, juce::PathStrokeType (1.0f));

                // detection: dim below the threshold, bright above it
                g.setColour (ess::teal.withAlpha (0.10f));
                g.fillPath (det);
                {
                    juce::Graphics::ScopedSaveState s2 (g);
                    g.reduceClipRegion (pa.withBottom (yOf (thresholdDb)).toNearestInt());
                    g.setGradientFill (juce::ColourGradient (ess::teal.withAlpha (0.55f), 0, pa.getY(), ess::teal.withAlpha (0.25f), 0, yOf (thresholdDb), false));
                    g.fillPath (det);
                }
                g.setColour (ess::teal.withAlpha (0.9f));
                g.strokePath (det, juce::PathStrokeType (1.3f));

                // reduction from the top
                g.setGradientFill (juce::ColourGradient (ess::gr.withAlpha (0.65f), 0, pa.getY(), ess::gr.withAlpha (0.25f), 0, pa.getY() + 24.0f * pxDb, false));
                g.fillPath (red);
                g.setColour (ess::gr);
                g.strokePath (red, juce::PathStrokeType (1.2f));
            }

            // threshold line + handle
            {
                const float y = yOf (thresholdDb);
                g.setColour (juce::Colours::white.withAlpha (0.55f + 0.35f * hoverAmt));
                g.fillRect (pa.getX(), y - 0.5f, pa.getWidth(), 1.0f + hoverAmt * 0.6f);
                const juce::String t = "Threshold  " + juce::String (thresholdDb, 1) + " dB";
                const auto f = font (11.5f, Weight::semibold);
                const float w = textWidth (f, t) + 22.0f;
                auto pill = juce::Rectangle<float> (pa.getX() + 10.0f, y - 11.0f, w, 22.0f);
                if (pill.getY() < pa.getY() + 2) pill.setY (y + 4.0f);
                drawPill (g, pill, juce::Colour (0xf0232326).brighter (0.08f * hoverAmt));
                g.setColour (col::label);
                g.setFont (f);
                g.drawText (t, pill, juce::Justification::centred, false);
                handleArea = pill;
            }

            // legend + peak readout
            {
                auto top = getLocalBounds().toFloat().withTrimmedRight (ess::labelStrip).removeFromTop (34.0f).reduced (4.0f, 8.0f);
                float x = top.getX() + 6.0f;
                g.setFont (caption());
                auto item = [&] (juce::Colour c, const char* s)
                {
                    g.setColour (c);
                    g.fillEllipse (x, top.getCentreY() - 3.5f, 7.0f, 7.0f);
                    x += 13.0f;
                    g.setColour (col::label3);
                    const float w = textWidth (caption(), s) + 2.0f;
                    g.drawText (s, juce::Rectangle<float> (x, top.getY(), w, top.getHeight()), juce::Justification::centredLeft, false);
                    x += w + 16.0f;
                };
                item (juce::Colours::white.withAlpha (0.45f), "INPUT");
                item (ess::teal, "DETECTION");
                item (ess::gr, "REDUCTION");

                const juce::String t = "Max reduction  " + juce::String (peakGr, 1) + " dB";
                const auto f = font (11.5f, Weight::semibold);
                const float w = textWidth (f, t) + 24.0f;
                peakArea = juce::Rectangle<float> (top.getRight() - w, top.getCentreY() - 11.0f, w, 22.0f);
                drawPill (g, peakArea);
                g.setColour (peakGr > 0.05f ? col::label : col::label2);
                g.setFont (f);
                g.drawText (t, peakArea, juce::Justification::centred, false);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const bool h = nearLine (e.position);
            if (h != hoverLine) { hoverLine = h; setMouseCursor (h ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverLine = false; }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (peakArea.contains (e.position)) { peakGr = 0.0f; repaint(); return; }
            if (thr == nullptr) return;
            dragging = true;
            startDb = nearLine (e.position) ? thresholdDb : dbAt (e.position.y);
            thr->beginGesture();
            if (! nearLine (e.position)) thr->setValueAsPartOfGesture (startDb);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging) return;
            const float pxDb = plotArea().getHeight() / -minDb;
            const float d = -(float) e.getDistanceFromDragStartY() / pxDb * (e.mods.isShiftDown() ? 0.2f : 1.0f);
            thr->setValueAsPartOfGesture (juce::jlimit (minDb, 0.0f, std::round ((startDb + d) * 10.0f) / 10.0f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging && thr != nullptr) thr->endGesture();
            dragging = false;
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (thr != nullptr && nearLine (e.position))
                thr->setValueAsCompleteGesture (thrParam->convertFrom0to1 (thrParam->getDefaultValue()));
        }

    private:
        float dbAt (float y) const
        {
            const auto pa = plotArea();
            return juce::jlimit (minDb, 0.0f, -(y - pa.getY()) / (pa.getHeight() / -minDb));
        }
        bool nearLine (juce::Point<float> p) const
        {
            const auto pa = plotArea();
            const float y = pa.getY() + -thresholdDb * pa.getHeight() / -minDb;
            return handleArea.contains (p) || (std::abs (p.y - y) < 6.0f && pa.getX() <= p.x && p.x <= pa.getRight());
        }

        std::array<avel::dsp::Frame, capacity> frames {};
        int head = 0, count = 0;
        float thresholdDb = -26.0f, peakGr = 0.0f, hoverAmt = 0.0f, startDb = 0.0f;
        bool hoverLine = false, dragging = false;
        juce::Rectangle<float> handleArea, peakArea;
        juce::RangedAudioParameter* thrParam = nullptr;
        std::unique_ptr<juce::ParameterAttachment> thr;
    };

    //==============================================================================
    // Input spectrum with the detection band: drag the edges (or the band) to move it.
    class BandView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int order = 12, size = 1 << order;

        BandView() : fft (order), window ((size_t) size, juce::dsp::WindowingFunction<float>::hann)
        {
            setTooltip ("Detection band. Drag an edge or the band itself, Shift for fine moves, double-click an edge to reset.");
            ring.assign ((size_t) size, 0.0f);
            spec.assign ((size_t) size / 2, -120.0f);
            work.assign ((size_t) size * 2, 0.0f);
        }

        void bind (juce::RangedAudioParameter& lo, juce::RangedAudioParameter& hi, juce::UndoManager* um)
        {
            loP = &lo; hiP = &hi;
            loA = std::make_unique<juce::ParameterAttachment> (lo, [this] (float v) { lowHz = v; repaint(); }, um);
            hiA = std::make_unique<juce::ParameterAttachment> (hi, [this] (float v) { highHz = v; repaint(); }, um);
            loA->sendInitialUpdate(); hiA->sendInitialUpdate();
        }

        void setSampleRate (double s) { sr = s; }
        void setSplit (bool s)        { split = s; repaint(); }
        void setReduction (float db)  { grNow = db; }

        void pushSamples (const float* x, int n)
        {
            for (int i = 0; i < n; ++i) { ring[(size_t) pos] = x[i]; pos = (pos + 1) % size; }
            fresh = true;
        }

        void tick()
        {
            if (fresh)
            {
                for (int i = 0; i < size; ++i) work[(size_t) i] = ring[(size_t) ((pos + i) % size)];
                window.multiplyWithWindowingTable (work.data(), (size_t) size);
                fft.performFrequencyOnlyForwardTransform (work.data());
                const float norm = 2.0f / (float) size * 2.0f;   // hann gain
                for (size_t b = 1; b < spec.size(); ++b)
                {
                    const float f = (float) b * (float) sr / (float) size;
                    const float db = juce::Decibels::gainToDecibels (work[b] * norm, -120.0f) + 4.5f * std::log2 (juce::jmax (20.0f, f) / 1000.0f);
                    spec[b] = db > spec[b] ? db : spec[b] - 1.2f;
                }
                fresh = false;
            }
            else
                for (auto& s : spec) s -= 1.2f;

            approach (grShown, grNow, 0.25f);
            for (int i = 0; i < 2; ++i) approach (hoverAmt[i], (hoverEdge == i || dragEdge == i || dragEdge == 2) ? 1.0f : 0.0f, 0.3f);
            repaint();
        }

        juce::Rectangle<float> plotArea() const
        {
            return getLocalBounds().toFloat().withTrimmedRight (ess::labelStrip).withTrimmedBottom (22.0f);
        }

        void paint (juce::Graphics& g) override
        {
            const auto pa = plotArea();
            auto xOf = [&] (float f) { return pa.getX() + pa.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f); };
            auto yOf = [&] (float db) { return pa.getBottom() - pa.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 84.0f) / 90.0f); };

            // frequency grid + labels
            const float decades[] = { 100, 1000, 10000 };
            for (float f = 20; f <= 20000; f *= 10)
                for (int m = 1; m < 10; ++m)
                {
                    const float ff = f * (float) m; if (ff > 20000) break;
                    const bool dec = std::find (std::begin (decades), std::end (decades), ff) != std::end (decades);
                    g.setColour (juce::Colours::white.withAlpha (dec ? 0.075f : 0.03f));
                    g.fillRect (std::round (xOf (ff)), pa.getY(), 1.0f, pa.getHeight());
                }
            g.setFont (font (10.5f, Weight::medium));
            g.setColour (col::label3);
            const std::pair<float, const char*> labels[] = { { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" },
                                                             { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" } };
            for (auto& l : labels)
            {
                const float x = xOf (l.first);
                auto r = juce::Rectangle<float> (x - 20.0f, pa.getBottom() + 4.0f, 40.0f, 16.0f);
                if (r.getRight() > pa.getRight()) r.setX (pa.getRight() - 40.0f);
                g.drawText (l.second, r, r.getRight() >= pa.getRight() - 0.5f ? juce::Justification::centredRight : juce::Justification::centred, false);
            }
            g.drawText ("Hz", juce::Rectangle<float> (pa.getRight() + 4.0f, pa.getBottom() + 4.0f, ess::labelStrip - 8.0f, 16.0f), juce::Justification::centredRight, false);

            // spectrum
            juce::Path sp;
            sp.startNewSubPath (pa.getX(), pa.getBottom());
            const float binHz = (float) sr / (float) size;
            float lastX = -1;
            for (size_t b = 1; b < spec.size(); ++b)
            {
                const float f = (float) b * binHz;
                if (f < 20.0f) continue;
                if (f > 20000.0f) break;
                const float x = xOf (f);
                if (x - lastX < 1.5f && b + 1 < spec.size()) continue;
                sp.lineTo (x, yOf (spec[b]));
                lastX = x;
            }
            sp.lineTo (pa.getRight(), pa.getBottom());
            sp.closeSubPath();

            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (pa.toNearestInt());
                g.setColour (juce::Colour (0xff8e9aaf).withAlpha (0.16f));
                g.fillPath (sp);
                g.setColour (juce::Colour (0xff8e9aaf).withAlpha (0.35f));
                g.strokePath (sp, juce::PathStrokeType (1.0f));
            }

            // band
            const float x0 = xOf (lowHz), x1 = xOf (highHz >= 19950.0f ? 20000.0f : highHz);
            auto band = juce::Rectangle<float> (x0, pa.getY(), x1 - x0, pa.getHeight());
            g.setGradientFill (juce::ColourGradient (ess::teal.withAlpha (0.13f), 0, pa.getY(), ess::teal.withAlpha (0.04f), 0, pa.getBottom(), false));
            g.fillRect (band);
            if (grShown > 0.05f)
            {
                g.setGradientFill (juce::ColourGradient (ess::gr.withAlpha (juce::jmin (0.4f, grShown / 24.0f * 0.9f)), 0, pa.getY(),
                                                         ess::gr.withAlpha (0.0f), 0, pa.getY() + pa.getHeight() * 0.8f, false));
                g.fillRect (band);
            }
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (band.toNearestInt());
                g.setColour (ess::teal.withAlpha (0.35f));
                g.fillPath (sp);
                g.setColour (ess::teal.withAlpha (0.9f));
                g.strokePath (sp, juce::PathStrokeType (1.2f));
            }

            // edges + nodes
            for (int i = 0; i < 2; ++i)
            {
                const float x = i == 0 ? x0 : x1;
                if (i == 1 && highHz >= 19950.0f) continue;     // open top: no edge
                g.setColour (ess::teal.withAlpha (0.55f + 0.45f * hoverAmt[i]));
                g.fillRect (x - 0.75f, pa.getY(), 1.5f, pa.getHeight());
            }
            for (int i = 0; i < 2; ++i)
            {
                float x = i == 0 ? x0 : x1;
                if (i == 1) x = juce::jmin (x, pa.getRight() - 12.0f);
                const auto c = juce::Point<float> (x, pa.getY() + 16.0f);
                const float r = 9.0f + hoverAmt[i];
                g.setColour (ess::teal.withAlpha (0.18f * (0.5f + hoverAmt[i])));
                g.fillEllipse (c.x - r - 4, c.y - r - 4, (r + 4) * 2, (r + 4) * 2);
                g.setGradientFill (juce::ColourGradient (ess::teal.brighter (0.25f), c.x, c.y - r, ess::teal.darker (0.25f), c.x, c.y + r, false));
                g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
                if (dragEdge == i || dragEdge == 2) { g.setColour (juce::Colours::white); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.5f); }
                g.setColour (juce::Colour (0xff0b2a29));
                g.setFont (font (10.5f, Weight::semibold));
                g.drawText (i == 0 ? "L" : "H", juce::Rectangle<float> (c.x - r, c.y - r, r * 2, r * 2), juce::Justification::centred, false);
                nodes[i] = c;
            }

            // band info pill
            {
                const juce::String t = juce::String (split ? "Split band  " : "Wide band  ") + loP->getText (loP->convertTo0to1 (lowHz), 32)
                                       + juce::String::fromUTF8 ("  \xe2\x80\x93  ") + (highHz >= 19950.0f ? juce::String ("20 kHz") : hiP->getText (hiP->convertTo0to1 (highHz), 32));
                const auto f = font (11.5f, Weight::semibold);
                const float w = textWidth (f, t) + 24.0f;
                float cx = band.getCentreX();
                cx = juce::jlimit (pa.getX() + w * 0.5f + 6, pa.getRight() - w * 0.5f - 6, cx);
                auto pill = juce::Rectangle<float> (cx - w * 0.5f, pa.getY() + 36.0f, w, 22.0f);
                if (pill.getX() < x0 + 8.0f && x0 > pa.getX() + w + 30) pill.setX (x0 - w - 12.0f);  // keep it off the L node
                drawPill (g, pill);
                g.setColour (col::label);
                g.setFont (f);
                g.drawText (t, pill, juce::Justification::centred, false);
            }

            g.setColour (col::stroke);
            g.fillRect (pa.getX(), pa.getY(), pa.getWidth(), 1.0f);
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = hit (e.position);
            if (h != hoverEdge)
            {
                hoverEdge = h;
                setMouseCursor (h < 0 ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
            }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverEdge = -1; }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragEdge = hit (e.position);
            startLo = lowHz; startHi = highHz;
            if (dragEdge == 0 || dragEdge == 2) loA->beginGesture();
            if (dragEdge == 1 || dragEdge == 2) hiA->beginGesture();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragEdge < 0) return;
            const auto pa = plotArea();
            const float octPerPx = std::log2 (1000.0f) / pa.getWidth() * (e.mods.isShiftDown() ? 0.2f : 1.0f);
            const float ratio = std::exp2 ((float) e.getDistanceFromDragStartX() * octPerPx);
            if (dragEdge == 0) loA->setValueAsPartOfGesture (juce::jlimit (1000.0f, juce::jmin (16000.0f, highHz / 1.25f), startLo * ratio));
            if (dragEdge == 1) hiA->setValueAsPartOfGesture (juce::jlimit (juce::jmax (2000.0f, lowHz * 1.25f), 20000.0f, startHi * ratio));
            if (dragEdge == 2)
            {
                float r = ratio;
                r = juce::jlimit (1000.0f / startLo, 16000.0f / startLo, r);
                if (startHi < 19950.0f) r = juce::jlimit (2000.0f / startHi, 20000.0f / startHi, r);
                loA->setValueAsPartOfGesture (startLo * r);
                if (startHi < 19950.0f) hiA->setValueAsPartOfGesture (startHi * r);
            }
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragEdge == 0 || dragEdge == 2) loA->endGesture();
            if (dragEdge == 1 || dragEdge == 2) hiA->endGesture();
            dragEdge = -1;
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int h = hit (e.position);
            if (h == 0) loA->setValueAsCompleteGesture (loP->convertFrom0to1 (loP->getDefaultValue()));
            if (h == 1) hiA->setValueAsCompleteGesture (hiP->convertFrom0to1 (hiP->getDefaultValue()));
        }

    private:
        int hit (juce::Point<float> p) const
        {
            const auto pa = plotArea();
            if (! pa.expanded (0, 2).contains (p)) return -1;
            auto xOf = [&] (float f) { return pa.getX() + pa.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f); };
            const float x0 = xOf (lowHz), x1 = juce::jmin (xOf (highHz), pa.getRight() - 12.0f);
            if (nodes[0].getDistanceFrom (p) < 12.0f || std::abs (p.x - x0) < 6.0f) return 0;
            if (nodes[1].getDistanceFrom (p) < 12.0f || (highHz < 19950.0f && std::abs (p.x - x1) < 6.0f)) return 1;
            if (p.x > x0 && p.x < xOf (highHz)) return 2;
            return -1;
        }

        juce::dsp::FFT fft;
        juce::dsp::WindowingFunction<float> window;
        std::vector<float> ring, spec, work;
        int pos = 0;
        bool fresh = false, split = true;
        double sr = 48000.0;
        float lowHz = 5500, highHz = 20000, grNow = 0, grShown = 0, startLo = 0, startHi = 0;
        float hoverAmt[2] {};
        int hoverEdge = -1, dragEdge = -1;
        juce::Point<float> nodes[2];
        juce::RangedAudioParameter* loP = nullptr, *hiP = nullptr;
        std::unique_ptr<juce::ParameterAttachment> loA, hiA;
    };

    //==============================================================================
    class MeterColumn : public juce::Component
    {
    public:
        void setLevels (float inDb, float grDb, float outDb)
        {
            target[0] = inDb; target[1] = grDb; target[2] = outDb;
            hold[0] = juce::jmax (hold[0], inDb); hold[1] = juce::jmax (hold[1], grDb); hold[2] = juce::jmax (hold[2], outDb);
        }
        void tick()
        {
            for (int i = 0; i < 3; ++i)
            {
                if (i == 1) shown[i] = target[i] > shown[i] ? target[i] : shown[i] + (target[i] - shown[i]) * 0.12f;
                else        shown[i] = target[i] > shown[i] ? target[i] : juce::jmax (target[i], shown[i] - 0.9f);
            }
            if (++holdTimer > 120) { holdTimer = 0; for (int i = 0; i < 3; ++i) hold[i] = i == 1 ? 0.0f : -120.0f; }
            repaint();
        }
        void mouseDown (const juce::MouseEvent&) override { for (int i = 0; i < 3; ++i) hold[i] = i == 1 ? 0.0f : -120.0f; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto capR = r.removeFromTop (16.0f);
            auto valR = r.removeFromBottom (16.0f);
            const float bw = 14.0f, gap = (r.getWidth() - bw * 3) / 4.0f;
            const char* names[] = { "IN", "GR", "OUT" };
            for (int i = 0; i < 3; ++i)
            {
                auto b = juce::Rectangle<float> (r.getX() + gap + (bw + gap) * (float) i, r.getY(), bw, r.getHeight());
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRoundedRectangle (b, 4.0f);
                g.setColour (col::stroke);
                g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
                auto in = b.reduced (2.0f);
                if (i == 1)
                {
                    const float h = in.getHeight() * juce::jlimit (0.0f, 1.0f, shown[i] / 24.0f);
                    g.setGradientFill (juce::ColourGradient (ess::gr, 0, in.getY(), ess::gr.withAlpha (0.6f), 0, in.getBottom(), false));
                    g.fillRoundedRectangle (in.withHeight (h), 2.5f);
                }
                else
                {
                    const float t = juce::jlimit (0.0f, 1.0f, (shown[i] + 60.0f) / 60.0f);
                    auto f = in.withTop (in.getBottom() - in.getHeight() * t);
                    g.setGradientFill (juce::ColourGradient (col::yellow, 0, in.getY(), col::green, 0, in.getY() + in.getHeight() * 0.35f, false));
                    g.fillRoundedRectangle (f, 2.5f);
                }
                g.setFont (caption());
                g.setColour (col::label3);
                g.drawText (names[i], juce::Rectangle<float> (b.getCentreX() - 15, capR.getY(), 30, capR.getHeight()), juce::Justification::centred, false);
                g.setFont (font (9.5f, Weight::semibold));
                g.setColour (col::label2);
                const float hv = hold[i];
                const juce::String t = i == 1 ? juce::String (hv, 1) : (hv <= -99 ? juce::String ("-inf") : juce::String (hv, 1));
                g.drawText (t, juce::Rectangle<float> (b.getCentreX() - 15, valR.getY(), 30, valR.getHeight()), juce::Justification::centred, false);
            }
            // dB ticks between bars
            g.setColour (col::label4);
            for (int db = 0; db >= -60; db -= 12)
            {
                const float y = r.getY() + 2.0f + (r.getHeight() - 4.0f) * (float) -db / 60.0f;
                g.fillRect (r.getX() + 2.0f, y, gap - 4.0f, 1.0f);
            }
        }

    private:
        float target[3] { -120, 0, -120 }, shown[3] { -120, 0, -120 }, hold[3] { -120, 0, -120 };
        int holdTimer = 0;
    };
}
