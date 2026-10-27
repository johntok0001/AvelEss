#include "PluginEditor.h"
#include "Presets.h"

using namespace avel::ui;

namespace
{
    constexpr int topBarH = 58, rightW = 88, cardH = 116;
    const juce::Colour accentA { 0xff66d4cf }, accentB { 0xff0a84ff };
}

AvelEssEditor::AvelEssEditor (AvelEssProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    proc.editorOpen = true;

    auto& s = proc.apvts;
    auto* um = &proc.undo;
    auto prm = [&s] (const char* id) -> juce::RangedAudioParameter& { return *s.getParameter (id); };

    // top bar
    for (auto* b : { &presetButton, &prevPreset, &nextPreset, &undoButton, &redoButton, &slotA, &slotB, &copySlot })
        addAndMakeVisible (*b);
    presetButton.setTooltip ("Presets");
    presetButton.setPill (true);
    presetButton.onClick = [this] { showPresetMenu(); };
    prevPreset.setTooltip ("Previous preset");  prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.setTooltip ("Next preset");      nextPreset.onClick = [this] { stepPreset (1); };
    undoButton.setTooltip ("Undo");             undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.setTooltip ("Redo");             redoButton.onClick = [this] { proc.undo.redo(); };
    slotA.setText ("A"); slotA.setTooltip ("Setting A"); slotA.onClick = [this] { proc.selectSlot (0); };
    slotB.setText ("B"); slotB.setTooltip ("Setting B"); slotB.onClick = [this] { proc.selectSlot (1); };
    copySlot.setTooltip ("Copy the active setting to the other one");
    copySlot.onClick = [this] { proc.copyActiveToOther(); };

    addAndMakeVisible (mode);
    mode.bind (prm ("mode"), um);
    mode.setTooltip ("Single Vocal ignores bright vowels and only reacts to sibilance. Allround is for mixes, drums and instruments.");

    // display
    addAndMakeVisible (history);
    history.bindThreshold (prm ("threshold"), um);
    addAndMakeVisible (band);
    band.bind (prm ("low"), prm ("high"), um);
    band.setSampleRate (proc.currentSampleRate.load());

    // card
    struct D { Dial* d; const char* id; juce::Colour c; const char* tip; };
    const D dials[] = {
        { &threshold, "threshold", accentA,       "Level above which sibilance is reduced. You can also drag the line in the display." },
        { &range,     "range",     ess::gr,       "Maximum amount of reduction." },
        { &release,   "release",   accentA,       "How fast the reduction lets go after each \"s\"." },
        { &lookahead, "lookahead", accentA,       "Looks ahead so the reduction is in place when the \"s\" starts. Adds latency." },
        { &link,      "link",      accentA,       "How much the two channels share their reduction." },
        { &low,       "low",       accentA,       "Lower edge of the detection band." },
        { &high,      "high",      accentA,       "Upper edge of the detection band. Open means no upper limit." },
        { &input,     "input",     col::accent,   "Input gain" },
        { &output,    "output",    col::accent,   "Output gain" },
    };
    for (auto& d : dials)
    {
        addAndMakeVisible (*d.d);
        d.d->bind (prm (d.id), um);
        d.d->colour = d.c;
        d.d->setTooltip (d.tip);
    }
    input.bipolar = output.bipolar = true;

    addAndMakeVisible (processing);
    processing.bind (prm ("processing"), um);
    processing.setTooltip ("Wide band turns the whole signal down. Split band only turns down the band, phase-linear.");
    processing.onChange = [this] (int) { refreshDependents(); };
    addAndMakeVisible (channels);
    channels.bind (prm ("channels"), um);
    channels.setTooltip ("Process both channels, only the mid or only the side.");
    channels.onChange = [this] (int) { refreshDependents(); };
    addAndMakeVisible (audition);
    audition.onColour = accentA;
    audition.bind (prm ("audition"), um);

    addAndMakeVisible (meters);

    refreshDependents();
    setResizable (true, true);
    setResizeLimits (1000, 620, 1900, 1200);
    setSize (proc.uiWidth, proc.uiHeight);
    startTimerHz (60);
}

AvelEssEditor::~AvelEssEditor()
{
    proc.editorOpen = false;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AvelEssEditor::refreshDependents()
{
    link.setDimmed (channels.getIndex() != 0);
    band.setSplit (processing.getIndex() == 1);
}

//==============================================================================
void AvelEssEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141416), 0, 0, juce::Colour (0xff09090a), 0, b.getBottom(), false));
    g.fillAll();

    // logo
    {
        auto lr = juce::Rectangle<float> (16.0f, (topBarH - 26) * 0.5f, 26.0f, 26.0f);
        g.setGradientFill (juce::ColourGradient (accentA, lr.getX(), lr.getY(), accentB, lr.getRight(), lr.getBottom(), false));
        g.fillRoundedRectangle (lr, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (lr.reduced (0.5f), 7.0f, 1.0f);
        icon::draw (g, icon::ess(), lr.reduced (4.0f), juce::Colours::white, 2.2f);

        g.setFont (font (15.0f, Weight::semibold));
        g.setColour (col::label);
        const float x = lr.getRight() + 10.0f;
        g.drawText ("Avel", juce::Rectangle<float> (x, 0, 60, (float) topBarH), juce::Justification::centredLeft, false);
        const float w = textWidth (font (15.0f, Weight::semibold), "Avel") + 5.0f;
        g.setFont (font (15.0f, Weight::regular));
        g.setColour (col::label2);
        g.drawText ("Ess", juce::Rectangle<float> (x + w, 0, 60, (float) topBarH), juce::Justification::centredLeft, false);
    }

    // display panel
    {
        auto d = displayArea;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), 0, d.getY(), juce::Colour (0xff0c0c0e), 0, d.getBottom(), false));
        g.fillRoundedRectangle (d, 14.0f);
        g.setGradientFill (juce::ColourGradient (accentA.withAlpha (0.035f), d.getCentreX(), d.getY(), juce::Colours::transparentBlack,
                                                 d.getCentreX(), d.getY() + d.getHeight() * 0.6f, true));
        g.fillRoundedRectangle (d, 14.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (d.reduced (0.5f), 14.0f, 1.0f);
    }

    // right column panel
    {
        auto r = rightArea;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), 0, r.getY(), juce::Colour (0xff0c0c0e), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    }

    // card
    drawGlass (g, cardArea, 18.0f);
    {
        g.setFont (caption());
        g.setColour (col::label3);
        g.drawText ("PROCESSING", processingCaption, juce::Justification::centredLeft, false);
        g.drawText ("CHANNELS", channelsCaption, juce::Justification::centredLeft, false);
        g.drawText ("AUDITION", auditionCaption, juce::Justification::centred, false);
        icon::draw (g, icon::headphones(), audition.getBounds().toFloat().withY ((float) audition.getBottom() + 4.0f).withHeight (20.0f).withSizeKeepingCentre (18, 18),
                    audition.isOn() ? accentA : col::label3, 1.5f);

        // dividers
        g.setColour (col::stroke.withMultipliedAlpha (1.5f));
        auto div = [&] (float x) { g.fillRect (x, cardArea.getY() + 18.0f, 1.0f, cardArea.getHeight() - 36.0f); };
        div ((float) (link.getRight() + low.getX()) * 0.5f);
        div ((float) (high.getRight() + processing.getX()) * 0.5f);
        div ((float) (processing.getRight() + audition.getX()) * 0.5f);
    }
}

void AvelEssEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    proc.uiWidth = w; proc.uiHeight = h;

    // top bar
    const int cy = topBarH / 2;
    const int mw = mode.idealWidth() + 20;
    mode.setBounds (w / 2 - mw / 2, cy - 14, mw, 28);
    const int pillW = juce::jlimit (130, 220, mode.getX() - 24 - 32 - 160);
    prevPreset.setBounds (128, cy - 14, 28, 28);
    presetButton.setBounds (160, cy - 14, pillW, 28);
    nextPreset.setBounds (presetButton.getRight() + 4, cy - 14, 28, 28);
    copySlot.setBounds (w - 16 - 30, cy - 14, 30, 28);
    slotB.setBounds (copySlot.getX() - 6 - 30, cy - 14, 30, 28);
    slotA.setBounds (slotB.getX() - 4 - 30, cy - 14, 30, 28);
    redoButton.setBounds (slotA.getX() - 18 - 30, cy - 14, 30, 28);
    undoButton.setBounds (redoButton.getX() - 4 - 30, cy - 14, 30, 28);

    // areas
    rightArea   = juce::Rectangle<float> ((float) (w - 12 - rightW), (float) topBarH, (float) rightW, (float) (h - topBarH - 12));
    displayArea = juce::Rectangle<float> (12.0f, (float) topBarH, rightArea.getX() - 10.0f - 12.0f, (float) (h - topBarH - 12));

    auto inner = displayArea.reduced (14.0f, 6.0f).toNearestInt();
    auto cardZone = inner.removeFromBottom (cardH + 14);
    auto bandArea = inner.removeFromBottom (juce::jlimit (110, 170, (int) (inner.getHeight() * 0.32f)));
    inner.removeFromBottom (6);
    history.setBounds (inner);
    band.setBounds (bandArea);

    // card content
    const bool compact = cardZone.getWidth() < 900;
    const int dialW = compact ? 60 : 66, dialH = 86, gap = compact ? 3 : 6, sep = compact ? 21 : 27;
    const int segW = juce::jmax (processing.idealWidth(), channels.idealWidth()) + (compact ? 0 : 10);
    const int auditionW = compact ? 60 : 70;
    const int contentW = 5 * dialW + 4 * gap + sep + 2 * dialW + gap + sep + segW + sep + 4 + auditionW;
    const int cw = contentW + (compact ? 28 : 40);
    cardArea = juce::Rectangle<float> ((float) (cardZone.getCentreX() - cw / 2), (float) (cardZone.getY() + 4), (float) cw, (float) cardH);
    int x = (int) cardArea.getX() + (compact ? 14 : 20);
    const int dy = (int) cardArea.getY() + (cardH - dialH) / 2;
    for (auto* d : { &threshold, &range, &release, &lookahead, &link }) { d->setBounds (x, dy, dialW, dialH); x += dialW + gap; }
    x += sep - gap;
    for (auto* d : { &low, &high }) { d->setBounds (x, dy, dialW, dialH); x += dialW + gap; }
    x += sep - gap;
    const int sy = (int) cardArea.getY() + 22;
    processingCaption = { x + 2, sy, segW, 14 };
    processing.setBounds (x, sy + 17, segW, 26);
    channelsCaption = { x + 2, sy + 48, segW, 14 };
    channels.setBounds (x, sy + 65, segW, 26);
    x += segW + sep + 4;
    auditionCaption = { x, sy, auditionW, 14 };
    audition.setBounds (x, sy + 22, auditionW, 30);

    // right column
    auto rc = rightArea.reduced (8.0f, 12.0f).toNearestInt();
    output.setBounds (rc.removeFromBottom (dialH).withSizeKeepingCentre (dialW, dialH));
    rc.removeFromBottom (6);
    input.setBounds (rc.removeFromBottom (dialH).withSizeKeepingCentre (dialW, dialH));
    rc.removeFromBottom (14);
    meters.setBounds (rc);
}

//==============================================================================
void AvelEssEditor::timerCallback()
{
    // frames → history + meters
    {
        int s1, n1, s2, n2;
        proc.frameFifo.prepareToRead (proc.frameFifo.getNumReady(), s1, n1, s2, n2);
        float in = -120, gr = 0, out = -120;
        bool any = n1 + n2 > 0;
        auto use = [&] (const avel::dsp::Frame& f) { history.push (f); in = juce::jmax (in, f.inDb); gr = juce::jmax (gr, f.grDb); out = juce::jmax (out, f.outDb); };
        for (int i = 0; i < n1; ++i) use (proc.frameBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) use (proc.frameBuf[(size_t) (s2 + i)]);
        proc.frameFifo.finishedRead (n1 + n2);
        if (any) { lastIn = in; lastGr = gr; lastOut = out; }
        else     { lastIn = juce::jmax (-120.0f, lastIn - 1.5f); lastOut = juce::jmax (-120.0f, lastOut - 1.5f); lastGr = juce::jmax (0.0f, lastGr - 0.5f); }
        meters.setLevels (lastIn, lastGr, lastOut);
        band.setReduction (lastGr);
    }
    // samples → spectrum
    {
        int s1, n1, s2, n2;
        proc.sampleFifo.prepareToRead (proc.sampleFifo.getNumReady(), s1, n1, s2, n2);
        if (n1 > 0) band.pushSamples (proc.sampleBuf.data() + s1, n1);
        if (n2 > 0) band.pushSamples (proc.sampleBuf.data() + s2, n2);
        proc.sampleFifo.finishedRead (n1 + n2);
        band.setSampleRate (proc.currentSampleRate.load());
    }

    history.tick();
    band.tick();
    meters.tick();

    presetButton.setText (proc.presetName);
    undoButton.setEnabled (proc.undo.canUndo());   undoButton.setAlpha (proc.undo.canUndo() ? 1.0f : 0.45f);
    redoButton.setEnabled (proc.undo.canRedo());   redoButton.setAlpha (proc.undo.canRedo() ? 1.0f : 0.45f);
    slotA.setToggled (proc.activeSlot == 0);
    slotB.setToggled (proc.activeSlot == 1);
    repaint (cardArea.toNearestInt().expanded (2));
}

//==============================================================================
void AvelEssEditor::stepPreset (int delta)
{
    const int n = (int) std::size (avel::ess::factoryPresets);
    proc.loadFactoryPreset ((proc.getCurrentProgram() + delta + n) % n);
}

void AvelEssEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const int n = (int) std::size (avel::ess::factoryPresets);
    for (int i = 0; i < n; ++i)
        m.addItem (1 + i, juce::String::fromUTF8 (avel::ess::factoryPresets[i].name), true, proc.presetName == juce::String::fromUTF8 (avel::ess::factoryPresets[i].name));

    auto folder = AvelEssProcessor::userPresetFolder();
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.avelpreset");
    files.sort();
    juce::PopupMenu user;
    for (int i = 0; i < files.size(); ++i)
        user.addItem (1000 + i, files[i].getFileNameWithoutExtension(), true, proc.presetName == files[i].getFileNameWithoutExtension());
    m.addSeparator();
    if (files.isEmpty()) m.addItem (999, "No user presets yet", false, false);
    else m.addSubMenu ("User Presets", user);
    m.addSeparator();
    m.addItem (900, "Save Preset...");
    m.addItem (901, "Open Presets Folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                     [this, files, folder] (int r)
    {
        if (r >= 1 && r < 900) proc.loadFactoryPreset (r - 1);
        else if (r >= 1000 && r - 1000 < files.size()) proc.loadUserPreset (files[r - 1000]);
        else if (r == 901) { folder.createDirectory(); folder.startAsProcess(); }
        else if (r == 900)
        {
            folder.createDirectory();
            chooser = std::make_unique<juce::FileChooser> ("Save Avel Ess preset", folder.getChildFile (proc.presetName + ".avelpreset"), "*.avelpreset");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File()) proc.saveUserPreset (f.withFileExtension ("avelpreset"));
            });
        }
    });
}

juce::AudioProcessorEditor* AvelEssProcessor::createEditor() { return new AvelEssEditor (*this); }
