// offscreen panel tests
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
#include "../Source/MidiText.h"
#include <cstdio>
#include <functional>

static int fails = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "  ok  " : "  FAIL", what);
    if (! ok) ++fails;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    // restore clipboard after
    const auto clipboardBefore = juce::SystemClipboard::getTextFromClipboard();


    // ── knobs as host params ──
    {
        QuadratProcessor lp;
        lp.prepareToPlay (48000.0, 512);
        int osc = -1;
        {
            const juce::ScopedLock sl (lp.patchLock);
            for (const auto& b : lp.patch.blocks()) if (b.kind == qd::Kind::osc) osc = b.id;
        }

        const int slot = lp.linkKnob (osc, 0);
        lp.linkSend (slot, 0.25f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto* param = lp.linkParam (slot);
        check (slot >= 0 && param != nullptr && param->getName (64) == "osc tune"
                 && std::abs (param->getValue() - 0.25f) < 1.0e-4f,
               "turning a knob borrows a host parameter named after it");
        check (lp.linkKnob (osc, 0) == slot && lp.linkKnob (osc, 1) != slot, "the same knob keeps its parameter");

        // depth ring
        const int ring = lp.linkKnob (osc, qd::kMaxKnobs + 2);
        lp.linkParam (ring)->setValueNotifyingHost (0.3f);
        {
            juce::AudioBuffer<float> rb (2, 512);
            juce::MidiBuffer rm;
            lp.processBlock (rb, rm);
            const juce::ScopedLock sl (lp.patchLock);
            check (lp.linkParam (ring)->getName (64) == "osc shape depth"
                     && std::abs (lp.patch.find (osc)->depth[2] - 0.3f) < 1.0e-4f,
                   "a depth ring is a host parameter too, and automating it moves the ring");
        }

        // host automates it
        param->setValueNotifyingHost (0.8f);
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        lp.processBlock (b, m);
        float now = 0.0f;
        {
            const juce::ScopedLock sl (lp.patchLock);
            now = lp.patch.find (osc)->knob[0];
        }
        check (std::abs (now - 0.8f) < 1.0e-4f, "automating the parameter turns the knob");

        // survives save + load
        juce::MemoryBlock saved;
        lp.getStateInformation (saved);
        QuadratProcessor back;
        back.setStateInformation (saved.getData(), (int) saved.getSize());
        check (back.linkParam (slot)->getName (64) == "osc tune", "and the link is kept with the session");

        lp.setCurrentProgram (1);
        check (lp.linkParam (slot)->getName (64).startsWith ("Knob"), "a new preset frees the parameters");
    }

    // ── as an effect: input on, odd blocks ──
    {
        auto run = [] (bool prepareFirst, juce::AudioChannelSet in, std::initializer_list<int> sizes)
        {
            QuadratProcessor fx;
            auto layout = fx.getBusesLayout();
            layout.inputBuses.getReference (0) = in;
            fx.setBusesLayout (layout);
            if (prepareFirst) fx.prepareToPlay (44100.0, 256);

            juce::MidiBuffer m;
            for (int n : sizes)
            {
                juce::AudioBuffer<float> b (2, juce::jmax (1, n));
                for (int i = 0; i < n; ++i) b.setSample (0, i, std::sin (i * 0.05f));
                juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, n);
                fx.processBlock (view, m);
            }

            std::unique_ptr<juce::AudioProcessorEditor> e (fx.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            fx.releaseResources();
            fx.prepareToPlay (96000.0, 64);
            juce::AudioBuffer<float> b (2, 4096);
            fx.processBlock (b, m);
            return true;
        };

        std::printf ("    effect: unprepared\n");
        run (false, juce::AudioChannelSet::stereo(), { 512, 64 });
        std::printf ("    effect: big, tiny, empty blocks\n");
        run (true, juce::AudioChannelSet::stereo(), { 4096, 1, 0, 257, 8192 });
        run (true, juce::AudioChannelSet::mono(), { 512, 2048 });
        check (true, "as an effect: input on, unprepared, odd block sizes and a rate change don't crash");
    }

    QuadratProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> block (2, 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;

    for (int i = 0; i < 90; ++i)
    {
        block.clear();
        proc.processBlock (block, midi);
        peak = juce::jmax (peak, block.getMagnitude (0, block.getNumSamples()));
    }

    std::printf ("starter patch peak: %.3f over %d blocks\n", peak, (int) proc.patch.blocks().size());
    check (std::isfinite (peak) && peak > 0.01f, "the starter patch makes a sound on its own");
    check (peak <= 1.0f, "and stays at or below full scale");

    // ── every preset sounds ──
    {
        int silent = 0, loud = 0;

        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);

            auto run = [&] (int blocks)
            {
                float p = 0.0f;
                for (int k = 0; k < blocks; ++k)
                {
                    block.clear();
                    proc.processBlock (block, midi);
                    p = juce::jmax (p, block.getMagnitude (0, block.getNumSamples()));
                }
                return p;
            };

            float p = run (200);                       // 2 s

            if (p < 0.005f)                            // then try playing it
            {
                proc.patch.noteOn (60, 1.0f);
                p = juce::jmax (p, run (100));
                proc.patch.noteOff (60);
                run (20);
            }

            // limiter may touch 1.0
            if (! std::isfinite (p) || p > 1.0001f)
            {
                ++loud;
                std::printf ("    hot: %s  peak=%.4f\n", proc.getProgramName (i).toRawUTF8(), p);
            }
            if (p < 0.005f) { ++silent; std::printf ("    silent: %s\n", proc.getProgramName (i).toRawUTF8()); }
        }

        std::printf ("%d presets checked\n", proc.getNumPrograms());
        check (silent == 0, "every preset sounds, on its own or when played");
        check (loud == 0, "and none of them leaves full scale");
    }

    // first run: welcome card, once
    {
        const auto settings = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("quadrat-test-settings.xml");
        settings.deleteFile();
        QuadratEditor::settingsOverride = settings;

        auto open = [&] { return std::unique_ptr<QuadratEditor> (dynamic_cast<QuadratEditor*> (proc.createEditor())); };

        auto first = open();
        const bool shown = first->welcomeShowing();
        first->pressTourButton ("skip");
        check (shown && ! first->welcomeShowing() && ! QuadratEditor::welcomeSeen(),
               "first launch shows the welcome card; skip closes it");
        first.reset();

        auto second = open();
        const bool back = second->welcomeShowing();
        second->pressTourButton ("Don't show this again");
        second->pressTourButton ("skip");
        check (back && QuadratEditor::welcomeSeen(), "unticked, it comes back; ticked, it is remembered");
        second.reset();

        check (! open()->welcomeShowing(), "and then it does not come back");

        settings.deleteFile();
        auto again = open();
        again->pressTourButton ("Don't show this again");
        again->pressTourButton ("start the tutorial (recommended)");
        check (again->tourRunning() && ! again->welcomeShowing(), "start the tutorial starts the tour");
        again.reset();
        // leaves the flag set, so the editors below open clean
    }

    proc.setCurrentProgram (1);               // Acid Line, for scopes

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    check (ed != nullptr, "the panel builds");
    if (ed == nullptr) return 1;

    ed->setVisible (true);

    for (int i = 0; i < 4; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);

    // model-side edits
    {
        const juce::ScopedLock sl (proc.patchLock);
        const int before = (int) proc.patch.blocks().size();
        const int id = proc.patch.add (qd::Kind::noise, 30, 30);   // clear of presets
        proc.patch.rebuild();
        check (id > 0 && (int) proc.patch.blocks().size() == before + 1, "a block can be added");

        check (proc.patch.add (qd::Kind::osc, 30, 30) < 0, "and cannot be dropped on an occupied cell");

        proc.patch.remove (id);
        proc.patch.rebuild();
        check ((int) proc.patch.blocks().size() == before, "and taken away again");
    }

    // stop silences anything
    {
        for (int k = 0; k < 40; ++k) { block.clear(); proc.processBlock (block, midi); }

        proc.setMuted (true);
        block.clear();
        proc.processBlock (block, midi);
        check (block.getMagnitude (0, block.getNumSamples()) == 0.0f,
               "stop holds a running patch silent");

        for (int k = 0; k < 20; ++k) { block.clear(); proc.processBlock (block, midi); }
        check (block.getMagnitude (0, block.getNumSamples()) == 0.0f, "and it stays silent");

        proc.setMuted (false);
        float back = 0.0f;
        for (int k = 0; k < 400; ++k)     // slow starter
        {
            block.clear();
            proc.processBlock (block, midi);
            back = juce::jmax (back, block.getMagnitude (0, block.getNumSamples()));
        }
        check (back > 0.01f, "pressing it again lets the patch carry on");
    }

    for (int k = 0; k < 120; ++k) { block.clear(); proc.processBlock (block, midi); }
    for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (30);

    // tooltips + hit test
    {
        int missing = 0;
        for (int k = 0; k < (int) qd::Kind::count; ++k)
            if (juce::String (qd::describe ((qd::Kind) k)).isEmpty()) ++missing;

        check (missing == 0, "every block has a description for its tooltip");

        // multi-delete
        {
            const juce::ScopedLock sl (proc.patchLock);
            const int before = (int) proc.patch.blocks().size();
            const int a = proc.patch.add (qd::Kind::noise, 34, 30);
            const int c = proc.patch.add (qd::Kind::level, 37, 30);
            proc.patch.rebuild();
            proc.patch.remove (a);
            proc.patch.remove (c);
            proc.patch.rebuild();
            check ((int) proc.patch.blocks().size() == before,
                   "two blocks can be added and removed together");
        }

        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        q->mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                        { 90.0f, 250.0f }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        q, q, juce::Time::getCurrentTime(), { 90.0f, 250.0f },
                                        juce::Time::getCurrentTime(), 0, false));
        check (true, "hovering the palette does not fall over");
    }

    // undo
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        int before = 0;
        {
            const juce::ScopedLock sl (proc.patchLock);
            before = (int) proc.patch.blocks().size();
        }

        q->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0));

        int after = 0;
        {
            const juce::ScopedLock sl (proc.patchLock);
            after = (int) proc.patch.blocks().size();
        }

        check (after == before, "undo with nothing to undo changes nothing");
    }

    // copy keeps cables
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        int before = 0, cablesBefore = 0;

        {
            const juce::ScopedLock sl (proc.patchLock);
            before = (int) proc.patch.blocks().size();
            cablesBefore = (int) proc.patch.cables().size();
        }

        q->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0));   // all
        q->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0));
        q->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0));

        int after = 0, cablesAfter = 0;
        {
            const juce::ScopedLock sl (proc.patchLock);
            after = (int) proc.patch.blocks().size();
            cablesAfter = (int) proc.patch.cables().size();
        }

        check (after == before * 2, "pasting a whole patch doubles it");
        check (cablesAfter >= cablesBefore * 2, "and the copy is wired like the original");

        q->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0));   // undo the paste

        const juce::ScopedLock sl (proc.patchLock);
        check ((int) proc.patch.blocks().size() == before, "undo takes the pasted copy away again");
    }

    // notes persist
    {
        {
            const juce::ScopedLock sl (proc.patchLock);
            qd::Note t; t.x = 3; t.y = 5; t.text = "this is the bass";
            proc.patch.notes().push_back (t);
        }

        const auto code = proc.patchCode();
        check (code.startsWith ("QDR1") && code.length() > 40, "a patch fits in a line of text");

        {
            const juce::ScopedLock sl (proc.patchLock);
            proc.patch.notes().clear();
        }

        check (proc.loadPatchCode (code), "and comes back from one");

        const juce::ScopedLock sl (proc.patchLock);
        check (proc.patch.notes().size() == 1
                 && proc.patch.notes().front().text == "this is the bass",
               "with what was written on it");
    }

    // palette ignores pan
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        const float sc = ed->getWidth() / 1120.0f;
        const auto palette = juce::Rectangle<int> ((int) (24 * sc), (int) (96 * sc),
                                                   (int) (200 * sc), (int) (540 * sc));

        auto grab = [&]
        {
            return ed->createComponentSnapshot (palette, false, 1.0f);
        };

        auto pan = [&] (float dx, float dy)
        {
            const juce::Point<float> at { 700.0f * sc, 360.0f * sc };
            juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(), at, {},
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q, juce::Time::getCurrentTime(),
                                 at, juce::Time::getCurrentTime(), 1, false);
            q->mouseWheelMove (me, { dx, dy, false, false });
        };

        // scroll to seq
        for (int i = 0; i < 12; ++i)
        {
            const juce::Point<float> at { 100.0f * sc, 400.0f * sc };
            juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(), at, {},
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q, juce::Time::getCurrentTime(),
                                 at, juce::Time::getCurrentTime(), 1, false);
            q->mouseWheelMove (me, { 0.0f, -0.25f, false, false });
        }

        // sweep the risky band
        const auto before = grab();
        int differing = 0;

        pan (0.0f, 2.0f);                              // top

        for (int step = 0; step < 16 && differing == 0; ++step)
        {
            pan (-0.125f, 0.0f);                       // forty pixels further right

            const auto now = grab();

            for (int y = 0; y < before.getHeight(); ++y)
                for (int x = 0; x < before.getWidth(); ++x)
                    if (before.getPixelAt (x, y) != now.getPixelAt (x, y)) ++differing;
        }

        pan (2.0f, -2.0f);

        if (differing > 0) std::printf ("    %d palette pixels changed with the pan\n", differing);
        check (differing == 0, "panning the field leaves the palette untouched");
    }

    // start resumes, no reload
    {
        auto button = [&] (const juce::String& name) -> juce::Button*
        {
            for (auto* c : ed->getChildren())
                if (auto* bt = dynamic_cast<juce::Button*> (c); bt != nullptr && bt->getName() == name)
                    return bt;
            return nullptr;
        };

        auto* panic = button ("stop");
        auto* start = button ("start");
        int blocksBefore = 0;

        {
            const juce::ScopedLock sl (proc.patchLock);
            proc.patch.blocks().front().knob[0] = 0.123f;       // reload would lose it
            blocksBefore = (int) proc.patch.blocks().size();
        }

        if (panic != nullptr) panic->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (proc.isMuted(), "stop stops the patch");

        if (start != nullptr) start->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        const juce::ScopedLock sl (proc.patchLock);
        check (! proc.isMuted() && panic != nullptr && ! panic->getToggleState(),
               "start lets it play again and releases stop");
        check ((int) proc.patch.blocks().size() == blocksBefore
                 && std::abs (proc.patch.blocks().front().knob[0] - 0.123f) < 1.0e-6f,
               "without reloading the patch");
    }

    // rolled names persist
    {
        juce::Button* roll = nullptr;

        for (auto* c : ed->getChildren())
            if (auto* bt = dynamic_cast<juce::Button*> (c); bt != nullptr && bt->getName() == "roll")
                roll = bt;

        const auto presetName = proc.patchTitle();
        if (roll != nullptr) roll->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        const auto rolled = proc.patchTitle();
        std::printf ("    rolled: %s\n", rolled.toRawUTF8());
        check (rolled.isNotEmpty() && rolled != presetName && ! rolled.containsIgnoreCase ("nobody")
                 && rolled.containsChar (' '),
               "a rolled patch is given a name of its own");

        juce::MemoryBlock state;
        proc.getStateInformation (state);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        check (proc.patchTitle() == rolled, "and keeps it when the session is saved and reopened");

        proc.setCurrentProgram (0);
        check (proc.patchTitle() == juce::String (presets::name (0)),
               "and gives it up when a preset is loaded");
    }

    // drawer state persists
    {
        int ox = -1, oy = -1, opened = -1;      // match by cell
        {
            const juce::ScopedLock sl (proc.patchLock);
            for (auto& b : proc.patch.blocks())
                if (qd::spec (b.kind).numKnobs > 0)
                {
                    b.drawer = true;
                    opened = b.id; ox = b.x; oy = b.y;
                    break;
                }
        }

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        {
            const juce::ScopedLock sl (proc.patchLock);
            if (auto* b = proc.patch.find (opened)) b->drawer = false;
        }

        proc.setStateInformation (state.getData(), (int) state.getSize());

        const juce::ScopedLock sl (proc.patchLock);
        bool open = false;

        for (const auto& b : proc.patch.blocks())
            if (b.x == ox && b.y == oy) open = b.drawer;

        check (opened >= 0 && open, "an open drawer comes back with the session");
    }

    // load report
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        check (proc.loadReport().isEmpty(), "a session saved now reloads complete");
    }

    // tour, end to end
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        auto button = [&] (const juce::String& name) -> juce::Button*
        {
            for (auto* c : ed->getChildren())
                if (auto* bt = dynamic_cast<juce::Button*> (c); bt != nullptr && bt->getName() == name)
                    return bt;
            return nullptr;
        };

        using K = qd::Kind;
        const int S = qd::kSub;

        proc.setCurrentProgram (3);
        pump (100);

        int mineBlocks = 0;
        float mineKnob = 0.0f;
        {
            const juce::ScopedLock sl (proc.patchLock);
            proc.patch.blocks().front().knob[0] = 0.321f;
            mineBlocks = (int) proc.patch.blocks().size();
            mineKnob = proc.patch.blocks().front().knob[0];
        }
        const auto mineTitle = proc.patchTitle();

        const auto tourBtn = [&] { q->openAbout(); const auto at = q->tourLink().getCentre();
                               q->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, {},
                                   1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q, juce::Time::getCurrentTime(), at,
                                   juce::Time::getCurrentTime(), 1, false)); };   // the "tour ▶" link
        tourBtn();
        pump (100);

        {
            const juce::ScopedLock sl (proc.patchLock);
            check (q->tourRunning() && q->tourAt() == 0 && proc.patch.blocks().empty(),
                   "the about page's Tutorial link starts the tour on an empty field");
        }

        auto idOf = [&] (K k)
        {
            for (const auto& b : proc.patch.blocks()) if (b.kind == k) return b.id;
            return -1;
        };

        auto edit = [&] (std::function<void (qd::Patch&)> fn)
        {
            const juce::ScopedLock sl (proc.patchLock);
            fn (proc.patch);
            proc.patch.rebuild();
        };

        const std::vector<std::function<void()>> doIt = {
            [&] { edit ([&] (qd::Patch& p) { p.add (K::osc, 4 * S, 0); }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::out, 7 * S, 0); }); },
            [&] { edit ([&] (qd::Patch& p) { p.find (idOf (K::osc))->knob[0] += 0.2f; }); },
            [&] { edit ([&] (qd::Patch& p) { p.find (idOf (K::osc))->option = 3; }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::clock, 0, 4 * S); }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::seq, 2 * S, 4 * S); }); },
            [&] { edit ([&] (qd::Patch& p) { p.connect (idOf (K::seq), 0, idOf (K::osc), 0); }); },
            [&] { edit ([&] (qd::Patch& p) { auto* b = p.find (idOf (K::seq)); b->steps[1] = 0.9f;
                                               b->stepMode[3] = qd::Block::Step::fall; }); },
            [&] { edit ([&] (qd::Patch& p) { p.move (idOf (K::out), 10 * S, 0); }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::level, 7 * S, 0); }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::env, 7 * S, 4 * S); }); },
            [&] { edit ([&] (qd::Patch& p) { p.connect (idOf (K::seq), 1, idOf (K::env), 0); }); },
            [&] { edit ([&] (qd::Patch& p) { p.connect (idOf (K::env), 0, idOf (K::level), 1); }); },
            [&] { edit ([&] (qd::Patch& p) { p.add (K::lfo, 11 * S, 4 * S); }); },
            [&] { edit ([&] (qd::Patch& p) { p.find (idOf (K::osc))->drawer = true; }); },
            [&] { edit ([&] (qd::Patch& p) { p.connect (idOf (K::lfo), 0, idOf (K::osc), 1, true); }); },
            [&] { edit ([&] (qd::Patch& p) { p.find (idOf (K::osc))->depth[1] = 0.45f; }); },
            [&] { edit ([&] (qd::Patch& p) { blockPresets::apply (K::seq, 9, *p.find (idOf (K::seq))); }); },
            [&] { button ("stop")->triggerClick(); pump (150); button ("start")->triggerClick(); },
            [&] { edit ([&] (qd::Patch& p) { p.notes().push_back ({ S, S, "first patch" }); }); },
            [&] { button ("roll")->triggerClick(); },
            [&] { q->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)); } };

        bool walked = true;
        int chapters = 0;

        for (int i = 0; i < (int) doIt.size() && walked; ++i)
        {
            if (q->tourAt() != i) { std::printf ("    tour is on step %d, expected %d\n", q->tourAt() + 1, i + 1); walked = false; break; }

            doIt[(std::size_t) i]();

            bool moved = false;
            for (int w = 0; w < 50 && ! moved; ++w)
            {
                pump (50);
                moved = q->tourCardUp() || q->tourAt() != i;
            }

            if (! moved) { std::printf ("    step %d was not noticed\n", i + 1); walked = false; break; }

            if (q->tourCardUp() && i + 1 < (int) doIt.size())
            {
                ++chapters;
                q->pressTourButton ("continue");
                pump (60);
            }
        }

        check (walked, "all 22 tour steps are noticed and passed, done on the field");
        check (chapters == 4 && q->tourCardUp(), "with a card at the end of each chapter and one at the end");

        {
            const juce::ScopedLock sl (proc.patchLock);
            bool clockQuick = false;
            for (const auto& b : proc.patch.blocks()) if (b.kind == K::clock) clockQuick = b.option == 3;
            check (clockQuick, "the tour sets its clock to eighths");
        }

        q->pressTourButton ("go back to the previous patch");
        pump (100);

        {
            const juce::ScopedLock sl (proc.patchLock);
            check (! q->tourRunning() && (int) proc.patch.blocks().size() == mineBlocks
                     && std::abs (proc.patch.blocks().front().knob[0] - mineKnob) < 1.0e-6f
                     && proc.patchTitle() == mineTitle,
                   "going back gives the player's own patch back, edits and name intact");
        }

        // skip, esc, keep
        tourBtn();
        pump (60);
        q->pressTourButton ("skip");
        q->pressTourButton ("skip");
        pump (60);
        q->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        const bool asked = q->tourCardUp();
        q->pressTourButton ("keep this patch");
        pump (100);

        {
            const juce::ScopedLock sl (proc.patchLock);
            check (asked && ! q->tourRunning() && proc.patch.blocks().size() == 2
                     && proc.patch.find (idOf (K::out)) != nullptr,
                   "skipping builds for you, and leaving can keep what was built");
        }

        q->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0));
        pump (60);

        {
            const juce::ScopedLock sl (proc.patchLock);
            check ((int) proc.patch.blocks().size() == mineBlocks, "and one undo brings the old patch back");
        }

        proc.setCurrentProgram (2);
        pump (100);
    }

    // cmd-V Logic notes
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        const auto before = juce::SystemClipboard::getTextFromClipboard();

        proc.setCurrentProgram (0);
        pump (100);

        int blocksBefore = 0;
        {
            const juce::ScopedLock sl (proc.patchLock);
            blocksBefore = (int) proc.patch.blocks().size();
        }

        juce::SystemClipboard::copyTextToClipboard (juce::String::fromUTF8 (" \t  \t 1 1 1 1 \t Note\t 1\t F3\t 80\t 0 1 1 160\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 1 2 1 \t Note\t 1\t D3\t 80\t 0 0 1 0\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 2 1 1 \t Note\t 1\t C\xe2\x99\xaf" "3\t 80\t 0 0 1 0\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 2 2 1 \t Note\t 1\t A\xe2\x99\xaf" "2\t 80\t 0 0 0 1\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"));
        q->mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                        { 700.0f, 500.0f }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                        juce::Time::getCurrentTime(), { 700.0f, 500.0f },
                                        juce::Time::getCurrentTime(), 1, false));
        q->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0));
        pump (60);

        auto midiNotes = [&]
        {
            const juce::ScopedLock sl (proc.patchLock);
            for (const auto& b : proc.patch.blocks())
                if (b.kind == qd::Kind::midi) return (int) b.phrase.size();
            return -1;
        };

        check (midiNotes() == 4, "command-V with Logic notes on the clipboard puts down a midi block holding them");

        juce::MemoryBlock state;
        proc.getStateInformation (state);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        pump (60);

        bool same = false;
        {
            const juce::ScopedLock sl (proc.patchLock);
            for (const auto& b : proc.patch.blocks())
                if (b.kind == qd::Kind::midi)
                    same = b.phrase.size() == 4 && b.phrase[2].pitch == 61 && b.phrase[0].length == 1360
                           && b.loopTicks == 3840;
        }
        check (same, "and the notes come back with the session");

        q->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0));
        pump (60);
        {
            const juce::ScopedLock sl (proc.patchLock);
            check ((int) proc.patch.blocks().size() == blocksBefore && midiNotes() == -1,
                   "and undo takes the pasted block away again");
        }

        juce::SystemClipboard::copyTextToClipboard (before);
        proc.setCurrentProgram (2);
        pump (100);
    }

    // sidechain input
    {
        auto layout = proc.getBusesLayout();
        const bool onToStart = layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo();

        auto withIn = [&] (const juce::AudioChannelSet& set)
        {
            auto l = proc.getBusesLayout();
            l.inputBuses.getReference (0) = set;
            return l;
        };

        check (onToStart && proc.checkBusesLayoutSupported (withIn (juce::AudioChannelSet::mono()))
                 && proc.checkBusesLayoutSupported (withIn (juce::AudioChannelSet::disabled())),
               "the input is on by default, so effect slots feed it; mono or off also work");

        proc.setBusesLayout (withIn (juce::AudioChannelSet::stereo()));
        proc.prepareToPlay (48000.0, 512);

        {
            const juce::ScopedLock sl (proc.patchLock);
            proc.patch.clear();
            proc.patch.add (qd::Kind::input, 0, 0);
            proc.patch.add (qd::Kind::out, 2 * qd::kSub, 0);
            for (auto& b : proc.patch.blocks()) if (b.kind == qd::Kind::input) b.knob[0] = 0.8f;
            proc.patch.rebuild();
        }

        juce::AudioBuffer<float> io (2, 512);
        juce::MidiBuffer none;
        float heard = 0.0f;
        double ph = 0.0;

        for (int k = 0; k < 20; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = 0.4f * (float) std::sin (ph);
                ph += 2.0 * 3.14159265358979 * 220.0 / 48000.0;
                io.setSample (0, i, v);
                io.setSample (1, i, v);
            }

            proc.processBlock (io, none);
            heard = juce::jmax (heard, io.getMagnitude (0, 0, 512));
        }

        check (heard > 0.05f, "audio on the side chain comes out through the in block");

        proc.setBusesLayout (layout);
        proc.prepareToPlay (48000.0, 512);
        proc.setCurrentProgram (2);
    }



    // ── MIDI files: load + drop ──
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        proc.setCurrentProgram (0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        // 480 tpq, 3/4, notes split over two tracks
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence meta, a, b;
        meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0);
        a.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        a.addEvent (juce::MidiMessage::noteOff (1, 60), 480);
        b.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 80), 960);
        b.addEvent (juce::MidiMessage::noteOff (1, 67), 1200);
        mf.addTrack (meta); mf.addTrack (a); mf.addTrack (b);

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("quadrat-test.mid");
        file.deleteFile();
        { juce::FileOutputStream out (file); mf.writeTo (out); }

        auto midiBlocks = [&]
        {
            std::vector<qd::Block> v;
            const juce::ScopedLock sl (proc.patchLock);
            for (const auto& blk : proc.patch.blocks()) if (blk.kind == qd::Kind::midi) v.push_back (blk);
            return v;
        };

        const bool loaded = q->loadMidiFile (file, -1, { 700.0f, 500.0f });
        auto m = midiBlocks();
        check (loaded && m.size() == 1 && m[0].phrase.size() == 2 && m[0].phrase[0].length == 960
                 && m[0].phrase[1].start == 1920 && m[0].phrase[1].pitch == 67 && m[0].loopTicks == 2880,
               "a MIDI file loads into a new midi block: both tracks, 480 tpq scaled, a 3/4 bar");

        // drop onto it: replaced, not a second block
        juce::MidiFile one;
        one.setTicksPerQuarterNote (96);
        juce::MidiMessageSequence t;
        t.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100), 0);
        t.addEvent (juce::MidiMessage::noteOff (1, 72), 96);
        one.addTrack (t);
        file.deleteFile();
        { juce::FileOutputStream out (file); one.writeTo (out); }

        juce::Point<int> at;
        {
            const juce::ScopedLock sl (proc.patchLock);
            for (const auto& blk : proc.patch.blocks()) if (blk.kind == qd::Kind::midi) at = { blk.x, blk.y };
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);   // view catches up

        // block centre on screen
        const auto s = ed->getWidth() / 1120.0f;
        juce::ignoreUnused (s);
        bool dropped = false;
        for (int py = 80; py < ed->getHeight() && ! dropped; py += 6)
            for (int px = 260; px < ed->getWidth() && ! dropped; px += 6)
            {
                q->filesDropped ({ file.getFullPathName() }, px, py);
                const auto now = midiBlocks();
                dropped = now.size() == 1 && now[0].phrase.size() == 1 && now[0].phrase[0].pitch == 72;
                if (now.size() > 1) break;                 // missed: made a new one
            }
        check (dropped, "dropping a .mid onto a midi block replaces its notes");
        check (q->isInterestedInFileDrag ({ "/x/song.MID" }) && ! q->isInterestedInFileDrag ({ "/x/a.wav" }),
               "only MIDI files are accepted for dropping");
        file.deleteFile();

        proc.setCurrentProgram (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    }

    // ── MIDI the way hosts send it ──
    {
        // key -> osc (sine) -> level <- key gate; prepared for 512, fed bigger blocks
        auto build = [&]
        {
            const juce::ScopedLock sl (proc.patchLock);
            auto& p = proc.patch;
            p.clear();
            const int S = qd::kSub;
            const int key = p.add (qd::Kind::key, 0, 0);
            const int osc = p.add (qd::Kind::osc, 3 * S, 0);
            const int lvl = p.add (qd::Kind::level, 6 * S, 0);
            p.add (qd::Kind::out, 9 * S, 0);
            p.connect (key, 1, lvl, 1);
            if (auto* b = p.find (osc)) b->option = 3;
            p.rebuild();
        };

        proc.setCurrentProgram (0);
        proc.prepareToPlay (48000.0, 512);
        build();

        auto run = [&] (int size, std::initializer_list<std::pair<int, juce::MidiMessage>> events)
        {
            juce::AudioBuffer<float> b (2, size);
            juce::MidiBuffer m;
            for (const auto& e : events) m.addEvent (e.second, e.first);
            b.clear();
            proc.processBlock (b, m);
            return b;
        };

        auto firstSound = [] (const juce::AudioBuffer<float>& b)
        {
            for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 1.0e-4f) return i;
            return -1;
        };

        // short note inside one block
        auto shortNote = run (2048, { { 100, juce::MidiMessage::noteOn (1, 60, 0.9f) },
                                      { 400, juce::MidiMessage::noteOff (1, 60) } });
        const float shortPeak = shortNote.getMagnitude (0, 0, 2048);

        // note-on late in a big block
        run (2048, {});
        auto late = run (2048, { { 1500, juce::MidiMessage::noteOn (1, 60, 0.9f) } });
        const int starts = firstSound (late);
        run (2048, { { 0, juce::MidiMessage::noteOff (1, 60) } });
        std::printf ("    midi: short-note peak %.3f, late note starts at sample %d of 2048\n", shortPeak, starts);

        check (shortPeak > 0.01f, "a note shorter than the buffer still sounds");
        check (starts >= 1500 && starts < 1520, "a note starts at its own sample, not the buffer's start");

        // bend: pitch out moves 2 semitones
        float bentPitch = 0.0f;
        run (512, { { 0, juce::MidiMessage::noteOn (1, 60, 0.9f) }, { 0, juce::MidiMessage::pitchWheel (1, 16383) } });
        run (512, {});
        {
            const juce::ScopedLock sl (proc.patchLock);
            for (const auto& b : proc.patch.blocks())
                if (b.kind == qd::Kind::key) bentPitch = proc.patch.scope (b.id)[qd::Patch::kScope - 1];
        }
        check (std::abs (bentPitch - 2.0f) < 0.05f, "pitch bend moves the key block two semitones");

        // sustain: release under pedal keeps sounding, pedal up stops it
        run (512, { { 0, juce::MidiMessage::pitchWheel (1, 8192) }, { 0, juce::MidiMessage::controllerEvent (1, 64, 127) },
                    { 10, juce::MidiMessage::noteOff (1, 60) } });
        const float held = run (2048, {}).getMagnitude (0, 1024, 1024);
        run (512, { { 0, juce::MidiMessage::controllerEvent (1, 64, 0) } });
        run (4096, {});                                  // also: 8x the prepared block
        const float released = run (2048, {}).getMagnitude (0, 1024, 1024);
        check (held > 0.01f && released < 1.0e-4f, "the sustain pedal holds a released note, and lets it go");
        // 8x the prepared block, with a note in it
        auto big = run (4096, { { 3000, juce::MidiMessage::noteOn (1, 60, 0.9f) } });
        check (firstSound (big) >= 3000 && big.getMagnitude (0, 3500, 596) > 0.01f,
               "a block eight times the prepared size plays, note on time");
        run (512, { { 0, juce::MidiMessage::noteOff (1, 60) } });

        proc.setCurrentProgram (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    }

    // argv: preset to render
    // tut mockups
    // quiet sidechain render
    if (argc > 2 && juce::String (argv[2]) == "welcome")
    {
        QuadratEditor::settingsOverride.deleteFile();      // first run again
        ed.reset (proc.createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        ed->repaint();
    }
    else     if (argc > 2 && juce::String (argv[2]) == "input")
    {
        proc.setCurrentProgram (0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        auto l = proc.getBusesLayout();
        l.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        proc.setBusesLayout (l);
        proc.prepareToPlay (48000.0, 512);
        proc.setCurrentProgram (1);

        {
            const juce::ScopedLock sl (proc.patchLock);
            auto& p = proc.patch;
            p.clear();
            const int S = qd::kSub;
            p.add (qd::Kind::input, 0, 0);
            p.add (qd::Kind::filter, 2 * S, 0);
            p.add (qd::Kind::out, 5 * S, 0);
            p.rebuild();
        }

        juce::AudioBuffer<float> io (2, 512);
        juce::MidiBuffer none;
        double ph = 0.0;
        const float amp = juce::Decibels::decibelsToGain (argc > 3 ? juce::String (argv[3]).getFloatValue() : -40.0f);

        for (int k = 0; k < 60; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = amp * (float) (std::sin (ph) * 0.7 + std::sin (ph * 2.02) * 0.3);
                ph += 2.0 * 3.14159265358979 * 180.0 / 48000.0;
                io.setSample (0, i, v);
                io.setSample (1, i, v);
            }

            proc.processBlock (io, none);
            if (k % 4 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }

        ed->resized();
        ed->repaint();
    }
    else // midi block render
    if (argc > 2 && juce::String (argv[2]) == "midi")
    {
        proc.setCurrentProgram (0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        proc.setCurrentProgram (1);             // program change -> reframe

        {
            const juce::ScopedLock sl (proc.patchLock);
            auto& p = proc.patch;
            p.clear();
            const int S = qd::kSub;

            const int clk = p.add (qd::Kind::clock, 0, 0);
            const int mid = p.add (qd::Kind::midi, 2 * S, 0);
            const int osc = p.add (qd::Kind::osc, 7 * S, 0);
            const int lvl = p.add (qd::Kind::level, 10 * S, 0);
            p.add (qd::Kind::out, 13 * S, 0);
            p.connect (mid, 0, osc, 0);
            p.connect (mid, 1, lvl, 1);

            if (auto* b = p.find (clk)) b->option = 5;
            if (auto* b = p.find (osc)) b->option = 3;

            const std::string riff =
                "\t 1 1 1 1 \t Note\t 1\t A2\t 100\t 0 0 2 0\t\n"
                "\t 1 1 3 1 \t Note\t 1\t C3\t 90\t 0 0 1 0\t\n"
                "\t 1 1 4 1 \t Note\t 1\t E3\t 90\t 0 0 1 0\t\n"
                "\t 1 2 1 1 \t Note\t 1\t A3\t 110\t 0 0 2 0\t\n"
                "\t 1 2 3 1 \t Note\t 1\t G3\t 80\t 0 0 2 0\t\n"
                "\t 1 3 1 1 \t Note\t 1\t E3\t 100\t 0 1 0 0\t\n"
                "\t 1 4 1 1 \t Note\t 1\t D3\t 80\t 0 0 1 0\t\n"
                "\t 1 4 3 1 \t Note\t 1\t C3\t 80\t 0 0 2 0\t\n"
                "\t 2 1 1 1 \t Note\t 1\t F2\t 100\t 0 0 2 0\t\n"
                "\t 2 1 3 1 \t Note\t 1\t A2\t 90\t 0 0 1 0\t\n"
                "\t 2 1 4 1 \t Note\t 1\t C3\t 90\t 0 0 1 0\t\n"
                "\t 2 2 1 1 \t Note\t 1\t F3\t 110\t 0 0 2 0\t\n"
                "\t 2 2 3 1 \t Note\t 1\t E3\t 80\t 0 0 2 0\t\n"
                "\t 2 3 1 1 \t Note\t 1\t C3\t 100\t 0 1 0 0\t\n"
                "\t 2 4 1 1 \t Note\t 1\t G2\t 90\t 0 0 2 0\t\n"
                "\t 2 4 3 1 \t Note\t 1\t B2\t 90\t 0 0 2 0\t\n";

            if (auto* b = p.find (mid)) midiText::fromLogic (riff, b->phrase, b->loopTicks);
            p.rebuild();
        }

        // run audio a bit
        {
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer none;
            for (int i = 0; i < (argc > 4 ? juce::String (argv[4]).getIntValue() : 150); ++i)
            {
                audio.clear();
                proc.processBlock (audio, none);
            }
        }

        juce::MessageManager::getInstance()->runDispatchLoopUntil (argc > 3 ? juce::String (argv[3]).getIntValue() : 1300);
        ed->resized();
        ed->repaint();
    }
    else // tour frame renders
    if (argc > 2 && juce::String (argv[2]).startsWith ("tour"))
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        const juce::String arg (argv[2]);
        const int total = (int) tour::steps().size();
        const int want = arg == "tourchapter" ? 8 : arg == "tourend" ? total + 1
                           : arg == "tourleave" ? 5 : juce::jlimit (1, total, arg.getTrailingIntValue());

        proc.setCurrentProgram (0);
        pump (200);

        ([&] { q->openAbout(); const auto at = q->tourLink().getCentre();
                               q->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, {},
                                   1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q, juce::Time::getCurrentTime(), at,
                                   juce::Time::getCurrentTime(), 1, false)); }) ();

        pump (100);

        for (int guard = 0; guard < 60 && q->tourRunning(); ++guard)
        {
            if (q->tourCardUp())
            {
                if (arg == "tourchapter" || ! q->pressTourButton ("continue")) break;   // end card: no continue
            }
            else if (q->tourAt() + 1 >= want) break;
            else q->pressTourButton ("skip");

            pump (60);
        }

        if (arg == "tourleave") q->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

        // argv[4]: pan left
        if (argc > 4)
        {
            const float dx = juce::String (argv[4]).getFloatValue();
            const juce::Point<float> at { 800.0f, 400.0f };
            q->mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                                 at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                                 juce::Time::getCurrentTime(), at,
                                                 juce::Time::getCurrentTime(), 1, false),
                               { -dx / 320.0f, 0.0f, false, false });
        }

        // settle the frame
        q->mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                        { 600.0f, 20.0f }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                        juce::Time::getCurrentTime(), { 600.0f, 20.0f },
                                        juce::Time::getCurrentTime(), 1, false));
        pump (argc > 3 ? juce::String (argv[3]).getIntValue() : 1100);
        ed->repaint();
    }
    else if (argc > 2 && juce::String (argv[2]).startsWith ("tut"))
    {
        const int stage = juce::String (argv[2]).getTrailingIntValue();
        proc.setCurrentProgram (0);

        {
            const juce::ScopedLock sl (proc.patchLock);
            auto& p = proc.patch;
            p.clear();
            const int S = qd::kSub;

            if (stage >= 1)                 // 1: stage 2 framing
            {
                const int osc = p.add (qd::Kind::osc, 4 * S, 0);
                const int clk = p.add (qd::Kind::clock, 0, 4 * S);
                const int seq = p.add (qd::Kind::seq, 2 * S, 4 * S);

                if (stage <= 2)
                    p.add (qd::Kind::out, 7 * S, 0);              // touching the osc

                if (stage >= 3)
                {
                    p.add (qd::Kind::level, 7 * S, 0);
                    p.add (qd::Kind::out, 10 * S, 0);
                    const int env = p.add (qd::Kind::env, 7 * S, 4 * S);
                    const int amp = p.blocks()[p.blocks().size() - 3].id;
                    const int lfo = p.add (qd::Kind::lfo, 11 * S, 4 * S);
                    p.connect (seq, 0, osc, 0);
                    p.connect (seq, 1, env, 0);
                    p.connect (env, 0, amp, 1);
                    p.connect (lfo, 0, osc, 2, true);
                    if (auto* b = p.find (osc)) { b->drawer = true; b->depth[2] = 0.55f; }
                }

                (void) clk;
            }

            p.rebuild();
        }

        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);

        if (stage == 1)                     // empty, same framing
        {
            {
                const juce::ScopedLock sl (proc.patchLock);
                proc.patch.clear();
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        }

        // reset palette + tooltip
        {
            auto* q = dynamic_cast<QuadratEditor*> (ed.get());
            const float sc = ed->getWidth() / 1120.0f;
            auto at = [&] (float x, float y) { return juce::Point<float> (x * sc, y * sc); };
            auto ev = [&] (juce::Point<float> p)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, {},
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q, juce::Time::getCurrentTime(),
                                         p, juce::Time::getCurrentTime(), 1, false);
            };

            for (int i = 0; i < 40; ++i) q->mouseWheelMove (ev (at (100.0f, 400.0f)), { 0.0f, 1.0f, false, false });
            q->mouseMove (ev (at (600.0f, 20.0f)));
        }

        ed->resized();
        ed->repaint();
    }
    else if (argc > 2)
    {
        proc.setCurrentProgram (juce::String (argv[2]).getIntValue());

        if (argc > 3)
        {
            const juce::ScopedLock sl (proc.patchLock);
            const auto kind = (qd::Kind) juce::String (argv[3]).getIntValue();

            for (int y = 0; y < 8 && proc.patch.add (kind, 0, y * qd::kSub * 2) < 0; ++y) {}
        }

        // argv[5]: a note
        if (argc > 5)
        {
            const juce::ScopedLock sl (proc.patchLock);
            qd::Note t;
            t.x = 2 * qd::kSub; t.y = (int) (2.5 * qd::kSub);
            t.text = juce::String (argv[5]).toStdString();
            proc.patch.notes().push_back (t);
        }

        // let timer catch up
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

        // argv[4]: zoom
        if (argc > 4)
        {
            auto* q = dynamic_cast<QuadratEditor*> (ed.get());
            const float want = juce::String (argv[4]).getFloatValue();

            // anchor at top-left
            const juce::Point<float> at { 260.0f, 120.0f };
            const int steps = want > 0.0f
                                ? (int) std::ceil (std::log (want / 0.45f) / std::log (1.0813f))
                                : 0;

            for (int i = 0; i < steps; ++i)
            {
                juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(),
                                     at, juce::ModifierKeys::commandModifier,
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                     juce::Time::getCurrentTime(), at,
                                     juce::Time::getCurrentTime(), 1, false);
                q->mouseWheelMove (me, { 0.0f, 0.08f, false, false });
            }
        }

        ed->resized();
        ed->repaint();
    }

    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 2.0f);
    check (img.isValid() && img.getWidth() > 100, "the panel renders");

    // credits
    {
        auto* q = dynamic_cast<QuadratEditor*> (ed.get());
        const juce::Point<float> mark { 30.0f, 32.0f };
        q->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                        mark, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                        juce::Time::getCurrentTime(), mark,
                                        juce::Time::getCurrentTime(), 1, false));

        const auto about = ed->createComponentSnapshot (ed->getLocalBounds(), false, 2.0f);
        check (about.isValid(), "the credits panel renders");

        const juce::File cf (juce::File::getCurrentWorkingDirectory().getChildFile ("about.png"));
        cf.deleteFile();
        if (auto* os = cf.createOutputStream().release())
        {
            std::unique_ptr<juce::OutputStream> owned (os);
            juce::PNGImageFormat().writeImageToStream (about, *owned);
        }

        q->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                        mark, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, q, q,
                                        juce::Time::getCurrentTime(), mark,
                                        juce::Time::getCurrentTime(), 1, false));
    }

    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("panel.png"));
    out.deleteFile();

    juce::PNGImageFormat png;
    if (auto* os = out.createOutputStream().release())
    {
        std::unique_ptr<juce::OutputStream> owned (os);
        png.writeImageToStream (img, *owned);
    }

    juce::SystemClipboard::copyTextToClipboard (clipboardBefore);

    std::printf ("panel written to %s (%d x %d)\n%s\n",
                 out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(),
                 fails == 0 ? "OK" : "FAILURES");
    return fails == 0 ? 0 : 1;
}
