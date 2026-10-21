#include "Panel.h"
#include "BinaryData.h"

void drawLabel (juce::Graphics& g, const char* text, float x, float y,
                  float size, float tracking, juce::Justification just, float width, bool bold)
{
    drawLabel (g, juce::String::fromUTF8 (text), x, y, size, tracking, just, width, bold);
}

void drawLabel (juce::Graphics& g, const juce::String& text, float x, float y,
                  float size, float tracking, juce::Justification just, float width, bool bold)
{
    const juce::Font f (juce::FontOptions (size).withStyle (bold ? "Bold" : "Plain"));
    g.setFont (f);

    if (tracking < 0.05f)
    {
        const float wide = juce::GlyphArrangement::getStringWidth (f, text);
        float tx = x;
        if (just == juce::Justification::centred)    tx = x + (width - wide) * 0.5f;
        else if (just == juce::Justification::right) tx = x + width - wide;

        juce::GlyphArrangement one;
        one.addLineOfText (f, text, tx, y);
        one.draw (g);
        return;
    }

    float total = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        total += juce::GlyphArrangement::getStringWidth (f, text.substring (i, i + 1)) + tracking;
    total -= tracking;

    float cx = x;
    if (just == juce::Justification::centred)    cx = x + (width - total) * 0.5f;
    else if (just == juce::Justification::right) cx = x + width - total;

    juce::GlyphArrangement ga;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto ch = text.substring (i, i + 1);
        ga.addLineOfText (f, ch, cx, y);
        cx += juce::GlyphArrangement::getStringWidth (f, ch) + tracking;
    }
    ga.draw (g);
}

void engraveLine (juce::Graphics& g, float x, float y, float w, float h)
{
    g.setColour (paint::engrave);
    g.fillRect (x, y, w, h);
    g.setColour (paint::relief);
    g.fillRect (x, y + h, w, h);
}

void drawKnob (juce::Graphics& g, float cx, float cy, float r, float v, bool withScale,
               juce::Colour indexColour)
{
    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float end   = juce::MathConstants<float>::pi * 2.75f;
    const float angle = start + juce::jlimit (0.0f, 1.0f, v) * (end - start);

    if (withScale)
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (i / 10.0f) * (end - start);
            const float sn = std::sin (a), cs = std::cos (a);
            g.setColour (paint::inkSoft);
            g.drawLine (cx + sn * (r + 2.5f), cy - cs * (r + 2.5f),
                        cx + sn * (r + 5.0f), cy - cs * (r + 5.0f), 0.8f);
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillEllipse (cx - r, cy - r + 2.0f, r * 2.0f, r * 2.0f);
    g.setColour (paint::knobRim);
    g.fillEllipse (cx - r, cy - r + 1.0f, r * 2.0f, r * 2.0f);
    g.setGradientFill (juce::ColourGradient (paint::knobTop, cx, cy - r,
                                             paint::knobBot, cx, cy + r, false));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    g.setColour (indexColour);
    g.drawLine (cx + std::sin (angle) * (r * 0.14f), cy - std::cos (angle) * (r * 0.14f),
                cx + std::sin (angle) * (r * 0.84f), cy - std::cos (angle) * (r * 0.84f),
                juce::jmax (1.0f, r * 0.14f));
}

PanelLNF::PanelLNF()
{
    face = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,
                                                    BinaryData::InterRegular_ttfSize);
    setColour (juce::PopupMenu::backgroundColourId, paint::plate);
    setColour (juce::PopupMenu::textColourId, paint::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, paint::ink);
    setColour (juce::PopupMenu::highlightedTextColourId, paint::plate);
}

juce::Typeface::Ptr PanelLNF::getTypefaceForFont (const juce::Font& f)
{
    return face != nullptr ? face : LookAndFeel_V4::getTypefaceForFont (f);
}

CapButton::CapButton (const juce::String& label, bool latching, bool isRed)
    : juce::Button (label), latch (latching), tinted (isRed), cap (paint::red)
{
    setClickingTogglesState (false);
}

void CapButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = capR + 3.0f;
    const bool pressed = down || (latch && getToggleState());
    const float drop = pressed ? 2.4f : 0.0f;

    g.setColour (juce::Colours::black.withAlpha (pressed ? 0.14f : 0.20f));
    g.fillEllipse (cx - capR, cy - capR + (pressed ? 2.0f : 3.5f), capR * 2.0f, capR * 2.0f);

    const auto top = tinted ? (pressed ? cap.brighter (0.06f) : cap)
                            : (pressed ? paint::capDown : paint::capTop);
    const auto bot = tinted ? cap.darker (0.35f) : paint::capBot;

    g.setColour (bot);
    g.fillEllipse (cx - capR, cy - capR + drop + 1.2f, capR * 2.0f, capR * 2.0f);
    g.setGradientFill (juce::ColourGradient (top, cx, cy - capR + drop,
                                             bot.brighter (0.10f), cx, cy + capR + drop, false));
    g.fillEllipse (cx - capR, cy - capR + drop, capR * 2.0f, capR * 2.0f);

    if (pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.13f));
        g.drawEllipse (cx - capR + 0.6f, cy - capR + drop + 0.6f,
                       capR * 2.0f - 1.2f, capR * 2.0f - 1.2f, 1.6f);
    }
    else if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (cx - capR + 0.5f, cy - capR + 0.5f, capR * 2.0f - 1.0f, capR * 2.0f - 1.0f, 1.0f);
    }

    if (getButtonText().isNotEmpty())
    {
        g.setColour (pressed ? paint::ink : paint::inkSoft);
        drawLabel (g, getButtonText(), 0.0f, cy + capR + 12.0f, 8.0f, 0.0f,
                     juce::Justification::centred, b.getWidth());
    }
}

// same radius as the shelf
static float fieldRadius (int h) { return (float) h * 0.21f; }

void PanelLNF::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& ed)
{
    g.setColour (ed.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, fieldRadius (h));
}

void PanelLNF::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& ed)
{
    const bool focused = ed.hasKeyboardFocus (true) && ! ed.isReadOnly();
    g.setColour (ed.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                        : juce::TextEditor::outlineColourId).withMultipliedAlpha (focused ? 1.0f : 0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, fieldRadius (h), 1.0f);
}
