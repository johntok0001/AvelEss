#pragma once

// Avel design system — shared look for every Avel product.
// Keep this file in sync across products (AvelEQ, AvelEmber, AvelVerb, AvelTime, AvelEss ...).

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BinaryData.h"

namespace avel::ui
{
    namespace col
    {
        const juce::Colour bg0       { 0xff0b0b0d };
        const juce::Colour bg1       { 0xff151518 };
        const juce::Colour bg2       { 0xff1c1c1f };
        const juce::Colour glass     { 0xe6202023 };
        const juce::Colour glassHi   { 0xff2c2c2f };
        const juce::Colour stroke    { 0x17ffffff };
        const juce::Colour strokeHi  { 0x26ffffff };
        const juce::Colour fill      { 0x33767680 };
        const juce::Colour fillHi    { 0x59767680 };
        const juce::Colour thumb     { 0xff636366 };
        const juce::Colour label     { 0xfff5f5f7 };
        const juce::Colour label2    { 0x99ebebf5 };
        const juce::Colour label3    { 0x4debebf5 };
        const juce::Colour label4    { 0x29ebebf5 };
        const juce::Colour accent    { 0xff0a84ff };
        const juce::Colour green     { 0xff30d158 };
        const juce::Colour red       { 0xffff453a };
        const juce::Colour yellow    { 0xffffd60a };

        inline juce::Colour band (int index)
        {
            static const juce::Colour c[] {
                juce::Colour (0xff0a84ff), juce::Colour (0xffff9f0a), juce::Colour (0xff30d158), juce::Colour (0xffbf5af2),
                juce::Colour (0xffff375f), juce::Colour (0xff64d2ff), juce::Colour (0xffffd60a), juce::Colour (0xff5e5ce6),
                juce::Colour (0xff66d4cf), juce::Colour (0xffff6961), juce::Colour (0xffac8e68), juce::Colour (0xffda8fff)
            };
            return c[(size_t) ((index % 12) + 12) % 12];
        }
    }

    enum class Weight { regular, medium, semibold };

    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        return w == Weight::regular ? r : w == Weight::medium ? m : s;
    }

    inline juce::Font font (float size, Weight w = Weight::medium, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (size).withKerningFactor (kerning));
    }

    inline juce::Font caption() { return font (10.0f, Weight::semibold, 0.06f); }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius, float shadow = 1.0f)
    {
        if (shadow > 0.0f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f * shadow), (int) (30 * shadow), { 0, (int) (12 * shadow) }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f * shadow), 4, { 0, 1 }).drawForPath (g, p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xf0262629), r.getX(), r.getY(), juce::Colour (0xf01b1b1e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (40.0f, r.getHeight() * 0.5f), false));
        g.fillRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + radius, r.getY() + 0.75f, r.getRight() - radius, r.getY() + 0.75f, 1.0f);
    }

    // info pill: 0xf0232326 fill, radius 13, hairline
    inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fillColour = juce::Colour (0xf0232326))
    {
        const float rad = juce::jmin (13.0f, r.getHeight() * 0.5f);
        g.setColour (fillColour);
        g.fillRoundedRectangle (r, rad);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }

    inline bool approach (float& v, float target, float speed = 0.25f)
    {
        if (std::abs (v - target) < 0.002f) { const bool changed = ! juce::exactlyEqual (v, target); v = target; return changed; }
        v += (target - v) * speed;
        return true;
    }

    namespace icon
    {
        using P = juce::Path;
        inline P undo() { P p; p.startNewSubPath (9, 6); p.lineTo (5, 10); p.lineTo (9, 14); p.startNewSubPath (5, 10); p.lineTo (14, 10);
                          p.cubicTo (17.5f, 10, 20, 12.5f, 20, 15); p.cubicTo (20, 17.5f, 17.5f, 20, 14, 20); p.lineTo (11, 20); return p; }
        inline P redo() { auto p = undo(); p.applyTransform (juce::AffineTransform::scale (-1, 1).translated (25, 0)); return p; }
        inline P trash() { P p; p.startNewSubPath (4.5f, 7); p.lineTo (19.5f, 7); p.startNewSubPath (9.5f, 7); p.lineTo (10, 4.5f); p.lineTo (14, 4.5f); p.lineTo (14.5f, 7);
                           p.startNewSubPath (6.5f, 7); p.lineTo (7.5f, 19.5f); p.lineTo (16.5f, 19.5f); p.lineTo (17.5f, 7);
                           p.startNewSubPath (10.25f, 10.5f); p.lineTo (10.5f, 16.5f); p.startNewSubPath (13.75f, 10.5f); p.lineTo (13.5f, 16.5f); return p; }
        inline P headphones() { P p; p.startNewSubPath (4.5f, 15); p.cubicTo (4.5f, 8, 8, 4.5f, 12, 4.5f); p.cubicTo (16, 4.5f, 19.5f, 8, 19.5f, 15);
                                p.addRoundedRectangle (4.0f, 13.5f, 4.0f, 6.5f, 1.6f); p.addRoundedRectangle (16.0f, 13.5f, 4.0f, 6.5f, 1.6f); return p; }
        inline P power() { P p; p.addCentredArc (12, 13, 7, 7, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
                           p.startNewSubPath (12, 4); p.lineTo (12, 11.5f); return p; }
        inline P chevronDown() { P p; p.startNewSubPath (7, 10); p.lineTo (12, 15); p.lineTo (17, 10); return p; }
        inline P chevronLeft()  { P p; p.startNewSubPath (14, 6); p.lineTo (8, 12); p.lineTo (14, 18); return p; }
        inline P chevronRight() { P p; p.startNewSubPath (10, 6); p.lineTo (16, 12); p.lineTo (10, 18); return p; }
        inline P copy() { P p; p.addRoundedRectangle (8.5f, 8.5f, 11.0f, 11.0f, 2.5f); p.startNewSubPath (5, 15); p.lineTo (5, 7); p.quadraticTo (5, 5, 7, 5); p.lineTo (15, 5); return p; }
        inline P close() { P p; p.startNewSubPath (7, 7); p.lineTo (17, 17); p.startNewSubPath (17, 7); p.lineTo (7, 17); return p; }
        inline P record() { P p; p.addEllipse (7.5f, 7.5f, 9.0f, 9.0f); return p; }
        inline P pencil() { P p; p.startNewSubPath (5, 19); p.lineTo (6, 15); p.lineTo (15.5f, 5.5f); p.lineTo (18.5f, 8.5f); p.lineTo (9, 18); p.closeSubPath();
                            p.startNewSubPath (13.5f, 7.5f); p.lineTo (16.5f, 10.5f); return p; }
        inline P file() { P p; p.startNewSubPath (7, 4); p.lineTo (14, 4); p.lineTo (18, 8); p.lineTo (18, 20); p.lineTo (7, 20); p.closeSubPath();
                          p.startNewSubPath (14, 4); p.lineTo (14, 8); p.lineTo (18, 8); return p; }
        inline P save() { P p; p.startNewSubPath (12, 4.5f); p.lineTo (12, 14.5f); p.startNewSubPath (8, 10.5f); p.lineTo (12, 14.5f); p.lineTo (16, 10.5f);
                          p.startNewSubPath (5, 15); p.lineTo (5, 19.5f); p.lineTo (19, 19.5f); p.lineTo (19, 15); return p; }
        // Avel Ess glyph: an "s" drawn as a sine that is being flattened
        inline P ess() { P p; p.startNewSubPath (4, 12); p.cubicTo (6.5f, 4.5f, 9.5f, 4.5f, 12, 12); p.cubicTo (14.5f, 19.5f, 17.5f, 19.5f, 20, 12); return p; }

        inline void draw (juce::Graphics& g, const P& p, juce::Rectangle<float> area, juce::Colour c, float stroke = 1.6f, float designSize = 24.0f)
        {
            const float s = juce::jmin (area.getWidth(), area.getHeight()) / designSize;
            auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - designSize * 0.5f * s, area.getCentreY() - designSize * 0.5f * s);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }
    }

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf8232326));
            setColour (juce::PopupMenu::textColourId, col::label);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a2a2d));
            setColour (juce::TextEditor::textColourId, col::label);
            setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.45f));
            setColour (juce::TextEditor::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
            setColour (juce::CaretComponent::caretColourId, col::accent);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf8232326));
            setColour (juce::TooltipWindow::textColourId, col::label);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1e1e21));
            setColour (juce::AlertWindow::textColourId, col::label);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c2f));
            setColour (juce::TextButton::textColourOffId, col::label);
            setColour (juce::ResizableWindow::backgroundColourId, col::bg1);
        }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            return f.getTypefaceStyle().containsIgnoreCase ("bold") ? typeface (Weight::semibold) : typeface (Weight::medium);
        }

        juce::Font getPopupMenuFont() override { return font (13.0f, Weight::medium); }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xff1f1f22));
            g.setColour (col::strokeHi);
            g.drawRect (0, 0, w, h, 1);
        }

        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                                const juce::Drawable*, const juce::Colour*) override
        {
            auto r = area.toFloat();
            if (isSeparator)
            {
                g.setColour (col::stroke.withMultipliedAlpha (2.0f));
                g.fillRect (r.reduced (10.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 20.0f, 1.0f));
                return;
            }
            if (isHighlighted && isActive)
            {
                g.setColour (col::accent);
                g.fillRoundedRectangle (r.reduced (5.0f, 1.0f), 6.0f);
            }
            const auto tc = ! isActive ? col::label3 : isHighlighted ? juce::Colours::white : col::label;
            if (isTicked)
            {
                juce::Path tick; tick.startNewSubPath (0, 5); tick.lineTo (3.5f, 8.5f); tick.lineTo (10, 1.5f);
                g.setColour (tc);
                g.strokePath (tick, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                              juce::AffineTransform::translation (r.getX() + 13.0f, r.getCentreY() - 5.0f));
            }
            g.setColour (tc);
            g.setFont (getPopupMenuFont());
            g.drawText (text, r.withTrimmedLeft (32.0f).withTrimmedRight (24.0f), juce::Justification::centredLeft, true);
            if (shortcut.isNotEmpty())
            {
                g.setColour (isHighlighted ? juce::Colours::white.withAlpha (0.7f) : col::label3);
                g.drawText (shortcut, r.withTrimmedRight (14.0f), juce::Justification::centredRight, false);
            }
            if (hasSubMenu)
                icon::draw (g, icon::chevronRight(), juce::Rectangle<float> (r.getRight() - 24.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f), tc, 1.6f);
        }

        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
        {
            juce::ignoreUnused (standardHeight);
            if (isSeparator) { w = 50; h = 9; return; }
            h = 26;
            w = (int) textWidth (getPopupMenuFont(), text) + 64;
        }

        int getPopupMenuBorderSize() override { return 5; }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) override
        {
            const auto f = font (12.0f, Weight::medium);
            const int w = juce::jmin (320, (int) textWidth (f, tip) + 22), h = 26;
            return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                         pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h).constrainedWithin (parent);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
            g.setColour (juce::Colour (0xf8232326));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (col::label);
            g.setFont (font (12.0f, Weight::medium));
            g.drawFittedText (text, r.reduced (11.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        }
    };

    //==============================================================================
    // Base for animated widgets: one 60 Hz timer that runs only while something is moving.
    class Animated : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    protected:
        void animate() { if (! isTimerRunning()) startTimerHz (60); }
        virtual bool stepAnimation() = 0;     // return true while still moving
        void mouseEnter (const juce::MouseEvent&) override { hover = true;  animate(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; animate(); }
        bool hover = false;
    private:
        void timerCallback() override { if (! stepAnimation()) stopTimer(); repaint(); }
    };

    //==============================================================================
    class Segmented : public Animated
    {
    public:
        explicit Segmented (juce::StringArray itemsIn) : items (std::move (itemsIn)) {}

        std::function<void (int)> onChange;
        juce::Colour accentColour = col::thumb;

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setIndex (juce::roundToInt (v), false); }, um);
            attachment->sendInitialUpdate();
        }

        int getIndex() const { return index; }

        void setIndex (int i, bool notify)
        {
            i = juce::jlimit (0, items.size() - 1, i);
            if (i == index && thumbX >= 0) return;
            index = i;
            if (thumbX < 0) thumbX = (float) i;
            animate();
            if (notify)
            {
                if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i);
                if (onChange) onChange (i);
            }
            else if (onChange) onChange (i);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
            g.setColour (col::fill);
            g.fillRoundedRectangle (r, rad);

            const float seg = r.getWidth() / (float) items.size();
            auto t = juce::Rectangle<float> (r.getX() + juce::jmax (0.0f, thumbX) * seg, r.getY(), seg, r.getHeight()).reduced (2.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.3f), 6, { 0, 2 }).drawForRectangle (g, t.toNearestInt());
            g.setColour (accentColour);
            g.fillRoundedRectangle (t, rad - 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (t.reduced (0.5f), rad - 2.0f, 1.0f);

            g.setFont (font (12.0f, Weight::semibold));
            for (int i = 0; i < items.size(); ++i)
            {
                if (i > 0 && i != index && i != index + 1)
                {
                    g.setColour (col::stroke.withMultipliedAlpha (1.6f));
                    g.fillRect (juce::Rectangle<float> (r.getX() + seg * i - 0.5f, r.getY() + r.getHeight() * 0.28f, 1.0f, r.getHeight() * 0.44f));
                }
                const bool hot = hoverIndex == i && i != index;
                g.setColour (i == index ? col::label : hot ? col::label.withAlpha (0.8f) : col::label2);
                g.drawText (items[i], juce::Rectangle<float> (r.getX() + seg * i, r.getY(), seg, r.getHeight()), juce::Justification::centred, false);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = juce::jlimit (0, items.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) items.size())));
            if (h != hoverIndex) { hoverIndex = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent& e) override { hoverIndex = -1; Animated::mouseExit (e); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            setIndex ((int) (e.position.x / ((float) getWidth() / (float) items.size())), true);
        }

        // width that fits every label
        int idealWidth() const
        {
            float w = 0; for (auto& s : items) w = juce::jmax (w, textWidth (font (12.0f, Weight::semibold), s));
            return (int) std::ceil ((w + 26.0f) * (float) items.size());
        }

    private:
        bool stepAnimation() override { return approach (thumbX, (float) index, 0.3f); }

        juce::StringArray items;
        int index = 0, hoverIndex = -1;
        float thumbX = -1.0f;
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==============================================================================
    class Switch : public Animated
    {
    public:
        std::function<void (bool)> onChange;
        juce::Colour onColour = col::green;

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setOn (v > 0.5f, false); }, um);
            attachment->sendInitialUpdate();
        }

        bool isOn() const { return on; }
        void setOn (bool b, bool notify)
        {
            if (b == on && pos >= 0) return;
            on = b;
            if (pos < 0) pos = on ? 1.0f : 0.0f;
            animate();
            if (notify && attachment != nullptr) attachment->setValueAsCompleteGesture (on ? 1.0f : 0.0f);
            if (onChange) onChange (on);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().withSizeKeepingCentre (38.0f, 22.0f);
            const float t = juce::jmax (0.0f, pos);
            g.setColour (col::fill.interpolatedWith (onColour, t));
            g.fillRoundedRectangle (r, 11.0f);
            auto k = juce::Rectangle<float> (r.getX() + 2.0f + t * 16.0f, r.getY() + 2.0f, 18.0f, 18.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 }).drawForRectangle (g, k.toNearestInt());
            g.setColour (juce::Colours::white.withAlpha (hover ? 1.0f : 0.95f));
            g.fillEllipse (k);
        }

        void mouseDown (const juce::MouseEvent&) override { setOn (! on, true); }

    private:
        bool stepAnimation() override { return approach (pos, on ? 1.0f : 0.0f, 0.3f); }
        bool on = false;
        float pos = -1.0f;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==============================================================================
    class IconButton : public Animated
    {
    public:
        IconButton() = default;
        explicit IconButton (juce::Path p) : path (std::move (p)) {}

        std::function<void()> onClick;
        juce::Colour tint = col::accent;

        void setIcon (juce::Path p)         { path = std::move (p); repaint(); }
        void setText (juce::String s)       { text = std::move (s); repaint(); }
        void setToggled (bool t)            { if (t != toggled) { toggled = t; animate(); } }
        bool isToggled() const              { return toggled; }
        void setPrimary (bool p)            { primary = p; repaint(); }
        void setPill (bool p)               { pill = p; repaint(); }   // preset-style pill: text left, chevron right

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
            if (pill)
            {
                drawPill (g, r, juce::Colour (0xf0232326).brighter (0.1f * hoverAmt));
                g.setColour (col::label);
                g.setFont (font (12.5f, Weight::semibold));
                g.drawFittedText (text, r.withTrimmedLeft (14.0f).withTrimmedRight (30.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
                icon::draw (g, icon::chevronDown(), juce::Rectangle<float> (r.getRight() - 26.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f), col::label2, 1.6f);
                return;
            }
            if (primary)
            {
                g.setColour (tint.withMultipliedBrightness (1.0f + 0.12f * hoverAmt));
                g.fillRoundedRectangle (r, rad);
            }
            else
            {
                g.setColour (col::fill.withMultipliedAlpha (0.6f + 0.9f * hoverAmt + 0.6f * toggleAmt));
                g.fillRoundedRectangle (r, rad);
                if (toggleAmt > 0.01f)
                {
                    g.setColour (tint.withAlpha (0.22f * toggleAmt));
                    g.fillRoundedRectangle (r, rad);
                }
            }
            const auto c = primary ? juce::Colours::white
                                   : col::label2.interpolatedWith (col::label, hoverAmt).interpolatedWith (tint.brighter (0.25f), toggleAmt);
            if (text.isNotEmpty())
            {
                g.setColour (c);
                g.setFont (font (12.5f, Weight::semibold));
                g.drawText (text, r, juce::Justification::centred, false);
            }
            else if (! path.isEmpty())
                icon::draw (g, path, r.withSizeKeepingCentre (18.0f, 18.0f), c, 1.6f);
        }

        void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            down = false; repaint();
            if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        bool stepAnimation() override
        {
            const bool a = approach (hoverAmt, hover ? 1.0f : 0.0f, 0.3f);
            const bool b = approach (toggleAmt, toggled ? 1.0f : 0.0f, 0.3f);
            return a || b;
        }
        juce::Path path;
        juce::String text;
        bool toggled = false, primary = false, pill = false, down = false;
        float hoverAmt = 0, toggleAmt = 0;
    };

    //==============================================================================
    // Ring dial: caption above, value below; drag (Shift = fine), wheel, double-click resets,
    // click the value to type it.
    class Dial : public Animated
    {
    public:
        explicit Dial (juce::String captionText) : captionStr (std::move (captionText)) { setWantsKeyboardFocus (false); }

        juce::Colour colour = col::accent;
        bool bipolar = false;
        std::function<void (float)> onValue;  // normalised

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v)
            {
                value = param->convertTo0to1 (v);
                if (onValue) onValue (value);
                repaint();
            }, um);
            attachment->sendInitialUpdate();
        }

        float getValue() const { return value; }
        juce::String valueText() const { return param != nullptr ? param->getText (value, 32) : juce::String (value, 2); }

        void setDimmed (bool d) { if (d != dimmed) { dimmed = d; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto capR = r.removeFromTop (14.0f);
            auto valR = r.removeFromBottom (18.0f);
            g.setFont (caption());
            g.setColour (col::label3.interpolatedWith (col::label2, hoverAmt));
            g.drawText (captionStr.toUpperCase(), capR, juce::Justification::centred, false);

            const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
            auto ring = r.withSizeKeepingCentre (d, d);
            const float cx = ring.getCentreX(), cy = ring.getCentreY(), rad = d * 0.5f - 2.0f;
            const float a0 = juce::MathConstants<float>::pi * -0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
            const float av = a0 + (a1 - a0) * value;
            const float from = bipolar ? 0.0f : a0;

            juce::Path track; track.addCentredArc (cx, cy, rad, rad, 0, a0, a1, true);
            g.setColour (col::fill.withMultipliedAlpha (1.4f));
            g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto vc = dimmed ? col::label3 : colour;
            if (std::abs (av - from) > 0.001f)
            {
                juce::Path arc; arc.addCentredArc (cx, cy, rad, rad, 0, juce::jmin (from, av), juce::jmax (from, av), true);
                if (! dimmed)
                {
                    g.setColour (vc.withAlpha (0.16f + 0.08f * hoverAmt));
                    g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (vc);
                g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            const float fr = rad - 7.0f;
            auto face = juce::Rectangle<float> (cx - fr, cy - fr, fr * 2, fr * 2);
            juce::Path fp; fp.addEllipse (face);
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 8, { 0, 3 }).drawForPath (g, fp);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff313135).brighter (0.05f * hoverAmt), cx, face.getY(),
                                                     juce::Colour (0xff1a1a1d), cx, face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.drawEllipse (face.reduced (0.5f), 1.0f);

            const float s = std::sin (av), c = -std::cos (av);
            g.setColour (dimmed ? col::label2 : juce::Colours::white);
            g.drawLine (cx + s * fr * 0.35f, cy + c * fr * 0.35f, cx + s * fr * 0.78f, cy + c * fr * 0.78f, 2.2f);

            if (editor == nullptr)
            {
                g.setFont (font (12.0f, Weight::semibold));
                g.setColour (dimmed ? col::label.withAlpha (0.45f) : col::label.withAlpha (0.92f));
                g.drawText (valueText(), valR, juce::Justification::centred, false);
            }
        }

        void resized() override { if (editor != nullptr) editor->setBounds (valueArea()); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragged = false;
            startValue = value;
            if (attachment != nullptr) attachment->beginGesture();
            e.source.enableUnboundedMouseMovement (! valueArea().contains (e.getPosition()));
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() < 2 && ! dragged) return;
            dragged = true;
            const float sens = e.mods.isShiftDown() ? 0.0012f : 0.0055f;
            setNormalised (startValue - (float) e.getDistanceFromDragStartY() * sens + (float) e.getDistanceFromDragStartX() * sens * 0.3f, true);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            e.source.enableUnboundedMouseMovement (false);
            if (attachment != nullptr) attachment->endGesture();
            if (! dragged && e.getNumberOfClicks() == 1 && valueArea().contains (e.getPosition())) showEditor();
        }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (param == nullptr) return;
            hideEditor();
            if (attachment) attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (attachment == nullptr) return;
            const float step = (e.mods.isShiftDown() ? 0.004f : 0.02f) * (w.deltaY > 0 ? 1.0f : w.deltaY < 0 ? -1.0f : 0.0f) * (w.isReversed ? -1.0f : 1.0f);
            if (juce::exactlyEqual (step, 0.0f)) return;
            attachment->beginGesture();
            setNormalised (value + step, true);
            attachment->endGesture();
        }

    private:
        juce::Rectangle<int> valueArea() const { return getLocalBounds().removeFromBottom (18).reduced (2, 0); }

        void setNormalised (float v, bool inGesture)
        {
            v = juce::jlimit (0.0f, 1.0f, v);
            if (param != nullptr && attachment != nullptr)
            {
                const float plain = param->convertFrom0to1 (v);
                if (inGesture) attachment->setValueAsPartOfGesture (plain);
                else attachment->setValueAsCompleteGesture (plain);
            }
        }

        void showEditor()
        {
            if (param == nullptr) return;
            editor = std::make_unique<juce::TextEditor>();
            editor->setFont (font (12.0f, Weight::semibold));
            editor->setJustification (juce::Justification::centred);
            editor->setIndents (2, 1);
            editor->setText (valueText(), false);
            editor->selectAll();
            editor->onReturnKey = [this] { commitEditor(); };
            editor->onEscapeKey = [this] { hideEditor(); };
            editor->onFocusLost = [this] { commitEditor(); };
            addAndMakeVisible (*editor);
            editor->setBounds (valueArea());
            editor->grabKeyboardFocus();
            repaint();
        }
        void commitEditor()
        {
            if (editor == nullptr) return;
            const auto t = editor->getText();
            if (param != nullptr && attachment != nullptr && t.trim().isNotEmpty())
                attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, param->getValueForText (t))));
            hideEditor();
        }
        void hideEditor()
        {
            if (editor == nullptr) return;
            auto* ed = editor.release();
            juce::MessageManager::callAsync ([ed] { delete ed; });
            ed->setVisible (false);
            repaint();
        }

        bool stepAnimation() override { return approach (hoverAmt, hover ? 1.0f : 0.0f, 0.3f); }

        juce::String captionStr;
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        std::unique_ptr<juce::TextEditor> editor;
        float value = 0.0f, startValue = 0.0f, hoverAmt = 0.0f;
        bool dragged = false, dimmed = false;
    };
}
