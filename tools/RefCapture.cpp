// RefCapture (from Wavy's MeterCapture): films or screenshots a plug-in editor while audio plays (Windows), to measure
// the reference's look and meter ballistics the host cannot read.
//   RefCapture <plugin.vst3> "<sub-plugin name>[@uid]" <out.raw> [paramName=value ...] [in=<mono float32 48 kHz file>]
//   ... [design=<width>] drag=x,y,dx,dy[,ctrl|shift|alt] ...   (drag script instead of the capture, see DragScript)
// Loads the plug-in (a shell's sub-plugin by name), opens its editor, feeds a fixed schedule of 50 Hz tone bursts from
// a real-time paced thread, and grabs the editor's screen area every ~25 ms. out.raw: per frame a double timestamp
// (seconds since the schedule started), then width * height * 3 bytes (RGB); out.raw.txt holds width and height,
// out.raw.pcm the plug-in's output (float32, interleaved stereo, 48 kHz).
//   ... state=<file>      loads a raw plug-in state (getStateInformation bytes) before the editor opens
//   ... shot=<ms>         writes one PNG (out path) after <ms> of audio instead of filming
//   ... rows=<from>,<to>  films only that band of rows (fractions of the editor's height)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <windows.h>
#include <cstdio>
#include <vector>

using namespace juce;

static const double fs = 48000.0;
static const int block = 480;
struct Seg { double secs; double db; bool on; };
static const Seg schedule[] = { { 1.0, 0, false }, { 2.0, -6, true }, { 3.0, 0, false }, { 2.0, -20, true }, { 3.0, 0, false },
                                { 0.3, -6, true }, { 3.0, 0, false } };

class Feeder : public Thread
{
public:
    Feeder (AudioPluginInstance& p, std::vector<float> in) : Thread ("feeder"), plugin (p), input (std::move (in)) {}
    std::atomic<double> startTime { 0.0 };
    std::atomic<bool> done { false };
    std::vector<float> recorded;   // the plug-in's output, interleaved stereo; written next to the frames
    void run() override
    {
        AudioBuffer<float> buf (2, block); MidiBuffer midi;
        sleep (2500);
        const double t0 = Time::getMillisecondCounterHiRes() * 0.001;
        startTime = t0;
        for (size_t i = 0; i + (size_t) block <= input.size() && ! threadShouldExit(); i += (size_t) block)
        {
            const double wait = t0 + (double) i / fs - Time::getMillisecondCounterHiRes() * 0.001;
            if (wait > 0) Thread::sleep ((int) (wait * 1000.0));
            for (int k = 0; k < block; ++k) { buf.setSample (0, k, input[i + (size_t) k]); buf.setSample (1, k, input[i + (size_t) k]); }
            plugin.processBlock (buf, midi);
            for (int k = 0; k < block; ++k) { recorded.push_back (buf.getSample (0, k)); recorded.push_back (buf.getSample (1, k)); }
        }
        done = true;
        std::printf ("feeder done: %d of %d samples\n", (int) recorded.size() / 2, (int) input.size());
        std::fflush (stdout);
    }
    // the default input: 50 Hz tone bursts (see schedule)
    static std::vector<float> defaultInput()
    {
        std::vector<float> v;
        for (auto& s : schedule)
        {
            const float a = s.on ? (float) std::pow (10.0, s.db / 20.0) : 0.0f;
            for (int64 i = 0; i < (int64) (s.secs * fs); ++i)
                v.push_back (a * (float) std::sin (2.0 * MathConstants<double>::pi * 50.0 * (double) v.size() / fs));
        }
        return v;
    }
    AudioPluginInstance& plugin;
    std::vector<float> input;
};

// the editor's pixels, taken from its own window (PrintWindow with full content: right even while other windows
// cover it, which a grab of the screen is not)
static void grabEditor (Component& c, HDC dst, int w, int h)
{
    HWND hwnd = nullptr; juce::Rectangle<int> r; double scale = 1.0;
    {
        MessageManagerLock mml;
        hwnd = (HWND) c.getTopLevelComponent()->getWindowHandle();
        r = c.getScreenBounds();
        scale = Desktop::getInstance().getDisplays().getDisplayForRect (r)->scale;
    }
    RECT wr; GetWindowRect (hwnd, &wr);
    const int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    HDC screen = GetDC (nullptr), mem = CreateCompatibleDC (screen);
    HBITMAP bmp = CreateCompatibleBitmap (screen, ww, wh);
    auto old = SelectObject (mem, bmp);
    PrintWindow (hwnd, mem, 2 /* PW_RENDERFULLCONTENT */);
    BitBlt (dst, 0, 0, w, h, mem, roundToInt (r.getX() * scale) - wr.left, roundToInt (r.getY() * scale) - wr.top, SRCCOPY);
    SelectObject (mem, old); DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
}

class Capture : public Timer
{
public:
    Capture (Component& ed, Feeder& f, const String& path) : editor (ed), feeder (f), out (path) { out.deleteFile(); stream = out.createOutputStream(); }
    void timerCallback() override
    {
        if (feeder.done)
        {
            stopTimer(); stream.reset();
            File (out.getFullPathName() + ".pcm").replaceWithData (feeder.recorded.data(), feeder.recorded.size() * sizeof (float));
            JUCEApplicationBase::quit(); return;
        }
        if (feeder.startTime == 0.0) return;
        auto r = editor.getScreenBounds();
        const double scale = Desktop::getInstance().getDisplays().getDisplayForRect (r)->scale;
        const int x = roundToInt (r.getX() * scale), y = roundToInt (r.getY() * scale), w = roundToInt (r.getWidth() * scale), h = roundToInt (r.getHeight() * scale);
        HDC screen = GetDC (nullptr), mem = CreateCompatibleDC (screen);
        HBITMAP bmp = CreateCompatibleBitmap (screen, w, h);
        auto old = SelectObject (mem, bmp);
        grabEditor (editor, mem, w, h); ignoreUnused (x, y);
        BITMAPINFO bi {}; bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
        const int stride = ((w * 3 + 3) / 4) * 4;
        HeapBlock<uint8> px ((size_t) stride * (size_t) h);
        GetDIBits (mem, bmp, 0, (UINT) h, px.get(), &bi, DIB_RGB_COLORS);
        SelectObject (mem, old); DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
        const double t = Time::getMillisecondCounterHiRes() * 0.001 - feeder.startTime;
        stream->write (&t, sizeof (double));
        HeapBlock<uint8> rgb ((size_t) w * 3);
        const int r0 = jlimit (0, h, roundToInt (rowFrom * (float) h)), r1 = jlimit (r0, h, roundToInt (rowTo * (float) h));
        for (int row = r0; row < r1; ++row)
        {
            const uint8* src = px.get() + (size_t) row * (size_t) stride;
            for (int c = 0; c < w; ++c) { rgb[c * 3] = src[c * 3 + 2]; rgb[c * 3 + 1] = src[c * 3 + 1]; rgb[c * 3 + 2] = src[c * 3]; }
            stream->write (rgb.get(), (size_t) w * 3);
        }
        if (! sizeWritten) { File (out.getFullPathName() + ".txt").replaceWithText (String (w) + " " + String (r1 - r0)); sizeWritten = true; }
    }
    Component& editor; Feeder& feeder; File out; std::unique_ptr<FileOutputStream> stream; bool sizeWritten = false;
    float rowFrom = 0.0f, rowTo = 1.0f;      // rows=<from>,<to>: only that band of the editor (fractions of its height)
};

// grabs the component's screen area (also native child windows, e.g. a Waves editor) into a PNG
static Image grabImage (Component& c);
static void grabPng (Component& c, const File& f)
{
    auto img = grabImage (c);
    f.deleteFile();
    if (auto st = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *st);
}

static Image grabImage (Component& c)
{
    juce::Rectangle<int> r; double scale = 1.0;
    {
        MessageManagerLock mml;
        r = c.getScreenBounds();
        scale = Desktop::getInstance().getDisplays().getDisplayForRect (r)->scale;
    }
    const int x = roundToInt (r.getX() * scale), y = roundToInt (r.getY() * scale), w = roundToInt (r.getWidth() * scale), h = roundToInt (r.getHeight() * scale);
    HDC screen = GetDC (nullptr), mem = CreateCompatibleDC (screen);
    HBITMAP bmp = CreateCompatibleBitmap (screen, w, h);
    auto old = SelectObject (mem, bmp);
    grabEditor (c, mem, w, h); ignoreUnused (x, y);
    BITMAPINFO bi {}; bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    // (a software image: its pixels can be written straight, which keeps bursts of grabs fast)
    Image img (Image::ARGB, w, h, true, SoftwareImageType());
    {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        HeapBlock<uint8> px ((size_t) w * (size_t) h * 4);
        GetDIBits (mem, bmp, 0, (UINT) h, px.get(), &bi, DIB_RGB_COLORS);
        for (int row = 0; row < h; ++row)
        {
            auto* d = bd.getLinePointer (row);
            const uint8* q = px.get() + (size_t) row * (size_t) w * 4;
            for (int col = 0; col < w; ++col, d += bd.pixelStride, q += 4)
            {
                d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = 255;     // BGRA in both
            }
        }
    }
    SelectObject (mem, old); DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
    return img;
}

// Drag script (drag=x,y,dx,dy[,mod[,steps]] in the editor's design pixels, design=<design width>): moves the real
// mouse over the editor, prints every parameter before and after each drag, and whether the cursor stayed visible
// in the middle of the drag. Used to measure how far a control moves per pixel of mouse movement.
// mod: ctrl | shift | alt, optionally joined with '+' to: rel (relative mouse moves through SendInput, as a real mouse
// makes them: a control that hides and re-centres the cursor sees them too), mid (the key goes down after the first
// step, i.e. during the drag), back (then half the way back in the same press). mod enter / leave: the cursor moves onto / off the control, and a burst of screen grabs
// follows (written afterwards as <out>_<n>_<ms>ms.png, the time since the move).
class DragScript : public Thread
{
public:
    DragScript (AudioPluginInstance& p, Component& ed, StringArray d, float designW, File shotBase)
        : Thread ("drag"), plugin (p), editor (ed), drags (std::move (d)), design (designW), shots (shotBase) {}
    std::atomic<bool> done { false };
    void run() override
    {
        sleep (2000);
        int n = 0;
        auto shot = [this, &n] (const char* when)
        {
            if (shots != File()) grabPng (editor, shots.getSiblingFile (shots.getFileNameWithoutExtension() + "_" + String (n) + "_" + when + ".png"));
        };
        for (auto& d : drags)
        {
            ++n;
            auto t = StringArray::fromTokens (d, ",", "");
            const float x = t[0].getFloatValue(), y = t[1].getFloatValue(), dx = t[2].getFloatValue(), dy = t[3].getFloatValue();
            const String mod = t.size() > 4 ? t[4] : String();
            const int steps = t.size() > 5 ? jmax (1, t[5].getIntValue()) : 50;     // 12 ms apart
            std::printf ("== drag %s\n", d.toRawUTF8());
            dump ("before");
            Point<int> start;
            {
                MessageManagerLock mml;
                const auto pos = editor.getScreenPosition();
                const float s = (float) editor.getWidth() / design;
                const double scale = Desktop::getInstance().getDisplays().getDisplayForRect (editor.getScreenBounds())->scale;
                start = { roundToInt ((pos.x + x * s) * scale), roundToInt ((pos.y + y * s) * scale) };
                stepScale = s * (float) scale;
            }
            const WORD vk = mod.contains ("ctrl") ? VK_CONTROL : mod.contains ("shift") ? VK_SHIFT : mod.contains ("alt") ? VK_MENU : 0;
            const bool relative = mod.contains ("rel"), keyMid = mod.contains ("mid");
            SetCursorPos (start.x, start.y); sleep (150);
            if (mod == "enter" || mod == "leave")                 // timing of what hovering shows
            {
                const Point<int> away { start.x + 400, start.y + 300 };
                SetCursorPos (mod == "enter" ? away.x : start.x, mod == "enter" ? away.y : start.y); sleep (800);
                const double t0 = Time::getMillisecondCounterHiRes();
                SetCursorPos (mod == "enter" ? start.x : away.x, mod == "enter" ? start.y : away.y);
                std::vector<std::pair<double, Image>> burst;
                for (double at : { 0.0, 10.0, 20.0, 35.0, 50.0, 75.0, 100.0, 150.0, 200.0, 300.0, 500.0, 800.0, 1200.0 })
                {
                    while (Time::getMillisecondCounterHiRes() - t0 < at) Thread::yield();
                    const double t = Time::getMillisecondCounterHiRes() - t0;
                    burst.emplace_back (t, grabImage (editor));
                }
                if (shots != File())
                    for (auto& [t, img] : burst)
                    {
                        auto f = shots.getSiblingFile (shots.getFileNameWithoutExtension() + "_" + String (n) + "_" + String (roundToInt (t)) + "ms.png");
                        f.deleteFile();
                        if (auto st = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *st);
                    }
                std::printf ("%s burst\n", mod.toRawUTF8()); continue;
            }
            if (mod == "burst" || mod == "hoverburst")            // click (or only move there), then film what follows
            {
                const int ms = t.size() > 5 ? t[5].getIntValue() : 1500;
                if (mod == "burst")
                {
                    sleep (400);
                    mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (30);
                    mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }
                const double t0 = Time::getMillisecondCounterHiRes();
                std::vector<std::pair<double, Image>> burst;
                while (Time::getMillisecondCounterHiRes() - t0 < ms)
                {
                    const double at = Time::getMillisecondCounterHiRes() - t0;
                    burst.emplace_back (at, grabImage (editor));
                }
                if (shots != File())
                    for (size_t k = 0; k < burst.size(); ++k)
                    {
                        auto f = shots.getSiblingFile (shots.getFileNameWithoutExtension() + "_" + String (n) + "_f" + String (k).paddedLeft ('0', 3)
                                                       + "_" + String (roundToInt (burst[k].first)) + "ms.png");
                        f.deleteFile();
                        if (auto st = f.createOutputStream()) PNGImageFormat().writeImageToStream (burst[k].second, *st);
                    }
                std::printf ("burst: %d frames in %d ms\n", (int) burst.size(), ms);
                dump ("after"); continue;
            }
            if (mod == "hover")                                   // no click: what does hovering show?
            {
                sleep (400); shot ("hover");
                std::printf ("hover\n"); continue;
            }
            if (mod == "click" || mod == "dbl" || mod.endsWith ("click"))   // click / double-click (+ modifier), then type t[6] + Enter
            {
                const WORD kv = mod.startsWith ("ctrl") ? VK_CONTROL : mod.startsWith ("alt") ? VK_MENU : mod.startsWith ("shift") ? VK_SHIFT : 0;
                if (kv) { keybd_event ((BYTE) kv, 0, 0, 0); sleep (50); }
                for (int c = 0; c < (mod == "dbl" ? 2 : 1); ++c)
                {
                    mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (30);
                    mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); sleep (60);
                }
                if (kv) keybd_event ((BYTE) kv, 0, KEYEVENTF_KEYUP, 0);
                sleep (500); shot ("edit");
                dump ("clicked");
                if (t.size() > 6)
                {
                    for (auto ch : t[6])
                    {
                        const SHORT k = VkKeyScanA ((char) ch);
                        keybd_event ((BYTE) (k & 0xff), 0, 0, 0); sleep (20); keybd_event ((BYTE) (k & 0xff), 0, KEYEVENTF_KEYUP, 0); sleep (40);
                    }
                    sleep (200); shot ("typed");
                    keybd_event (VK_RETURN, 0, 0, 0); sleep (20); keybd_event (VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                }
                sleep (500); shot ("after");
                dump ("after"); continue;
            }
            if (vk && ! keyMid) { keybd_event ((BYTE) vk, 0, 0, 0); sleep (50); }
            mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (80);
            shot ("down");
            bool hidden = false;
            Point<int> sent;
            for (int i = 1; i <= steps; ++i)
            {
                const Point<int> to { roundToInt (dx * stepScale * (float) i / steps), roundToInt (dy * stepScale * (float) i / steps) };
                if (relative)
                {
                    INPUT in {}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE;
                    in.mi.dx = to.x - sent.x; in.mi.dy = to.y - sent.y; sent = to;
                    SendInput (1, &in, sizeof (INPUT));
                }
                else
                    SetCursorPos (start.x + to.x, start.y + to.y);
                sleep (12);
                if (i == 1 && vk && keyMid) { keybd_event ((BYTE) vk, 0, 0, 0); sleep (50); }
                if (i == steps / 2)
                {
                    shot ("mid");
                    CURSORINFO ci { sizeof (CURSORINFO) };
                    GetCursorInfo (&ci);
                    hidden = (ci.flags & CURSOR_SHOWING) == 0 || ci.hCursor == nullptr;
                }
            }
            if (mod.contains ("back"))
                for (int i = 1; i <= steps / 2; ++i)
                {
                    const float f = 1.0f - 0.5f * (float) i / (float) (steps / 2);
                    SetCursorPos (start.x + roundToInt (dx * stepScale * f), start.y + roundToInt (dy * stepScale * f));
                    sleep (12);
                }
            sleep (80);
            mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); sleep (50);
            if (vk) keybd_event ((BYTE) vk, 0, KEYEVENTF_KEYUP, 0);
            sleep (300);
            shot ("after");
            SetCursorPos (start.x + 400, start.y + 300); sleep (600);   // mouse away: does the highlight stay?
            shot ("away");
            std::printf ("cursor during drag: %s\n", hidden ? "HIDDEN" : "visible");
            dump ("after");
        }
        done = true;
    }
    void dump (const char* when)
    {
        std::printf ("%s:", when);
        for (auto* p : plugin.getParameters())
            if (p->getName (64).isNotEmpty() && ! p->getName (64).startsWith ("MIDI") && p->getName (64).length() < 24)
                std::printf (" [%s=%s]", p->getName (64).toRawUTF8(), p->getText (p->getValue(), 32).trim().toRawUTF8());
        std::printf ("\n");
        std::fflush (stdout);
    }
    AudioPluginInstance& plugin;
    Component& editor;
    StringArray drags;
    float design, stepScale = 1.0f;
    File shots;
};

class DragWatch : public Timer
{
public:
    explicit DragWatch (DragScript& d) : script (d) {}
    void timerCallback() override { if (script.done) { stopTimer(); JUCEApplicationBase::quit(); } }
    DragScript& script;
};

class App : public JUCEApplication
{
public:
    const String getApplicationName() override { return "RefCapture"; }
    const String getApplicationVersion() override { return "1.0"; }
    void systemRequestedQuit() override { std::printf ("system requested quit\n"); std::fflush (stdout); quit(); }
    void initialise (const String& cmd) override
    {
        auto args = StringArray::fromTokens (cmd, true);
        for (auto& a : args) a = a.unquoted();
        if (args.size() < 3) { std::printf ("usage: RefCapture plugin.vst3 \"name\" out.raw [param=value ...]\n"); quit(); return; }
        fm.addFormat (new VST3PluginFormat());
        // "name@uid": build the description directly; enumerating a shell first leaves JUCE's module cache pointing
        // at the shell's first class, so the wrong sub-plugin would load
        if (args[1].contains ("@"))
        {
            static PluginDescription direct;
            direct.pluginFormatName = "VST3"; direct.fileOrIdentifier = args[0];
            direct.name = args[1].upToFirstOccurrenceOf ("@", false, false);
            direct.uniqueId = args[1].fromFirstOccurrenceOf ("@", false, false).getIntValue();
            direct.numInputChannels = direct.numOutputChannels = 2;
            String e2;
            // the shell changes its class list once it has a host context, so the index JUCE looked up before that
            // points at another sub-plugin; keeping the first instance loaded, the second creation looks it up again
            // in the final list
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                plugin = fm.createPluginInstance (direct, fs, block, e2);
                if (plugin == nullptr || plugin->getName() == direct.name) break;
                std::printf ("got %s, retrying\n", plugin->getName().toRawUTF8());
                keepLoaded.push_back (std::move (plugin));
            }
            if (plugin == nullptr) { std::printf ("load failed: %s\n", e2.toRawUTF8()); quit(); return; }
            std::printf ("loaded %s\n", plugin->getName().toRawUTF8());
            setUp (args);
            return;
        }
        OwnedArray<PluginDescription> types;
        for (auto* f : fm.getFormats()) if (f->getName() == "VST3") f->findAllTypesForFile (types, args[0]);
        PluginDescription* d = nullptr;
        for (auto* t : types)
        {
            if (args[1] == "--list") std::printf ("%s | uid %d | id %s\n", t->name.toRawUTF8(), t->uniqueId, t->createIdentifierString().toRawUTF8());
            if (t->name == args[1]) d = t;
        }
        if (d != nullptr) std::printf ("loading %s (uid %d)\n", d->name.toRawUTF8(), d->uniqueId);
        if (d == nullptr) { std::printf ("plug-in not found\n"); quit(); return; }
        String err;
        plugin = fm.createPluginInstance (*d, fs, block, err);
        if (plugin == nullptr) { std::printf ("load failed: %s\n", err.toRawUTF8()); quit(); return; }
        std::printf ("loaded %s\n", plugin->getName().toRawUTF8());
        setUp (args);
    }
    void setUp (const StringArray& args)
    {
        for (int io = 0; io < 2; ++io)                       // bus layout (main / side-chain)
            for (int b = 0; b < plugin->getBusCount (io == 0); ++b)
                if (auto* bus = plugin->getBus (io == 0, b))
                    std::printf ("  %s bus %d '%s': %d ch, enabled %d\n", io == 0 ? "input" : "output", b, bus->getName().toRawUTF8(),
                                 bus->getNumberOfChannels(), (int) bus->isEnabled());
        plugin->setPlayConfigDetails (2, 2, fs, block);
        plugin->prepareToPlay (fs, block);
        for (int i = 3; i < args.size(); ++i)
            if (args[i].startsWith ("state="))
            {
                MemoryBlock mb; File (args[i].substring (6)).loadFileAsData (mb);
                plugin->setStateInformation (mb.getData(), (int) mb.getSize());
            }
        for (int i = 3; i < args.size(); ++i)
        {
            if (args[i].startsWith ("state=") || args[i].startsWith ("shot=") || args[i].startsWith ("in=")
                || args[i].startsWith ("drag=") || args[i].startsWith ("design=") || args[i].startsWith ("rows=")) continue;
            const auto name = args[i].upToFirstOccurrenceOf ("=", false, false);
            const float v = args[i].fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            for (auto* p : plugin->getParameters())
                if (p->getName (64) == name) p->setValueNotifyingHost (v);
        }
        window = std::make_unique<DocumentWindow> ("RefCapture", Colours::black, DocumentWindow::closeButton);
        window->setUsingNativeTitleBar (true);
        window->setContentNonOwned (plugin->createEditorIfNeeded(), true);
        window->setTopLeftPosition (100, 100);
        window->setVisible (true);
        window->setAlwaysOnTop (true);
        StringArray drags; float design = 800.0f;
        for (int i = 3; i < args.size(); ++i)
        {
            if (args[i].startsWith ("drag=")) drags.add (args[i].substring (5));
            if (args[i].startsWith ("design=")) design = args[i].substring (7).getFloatValue();
        }
        if (! drags.isEmpty())
        {
            dragScript = std::make_unique<DragScript> (*plugin, *window->getContentComponent(), drags, design, File (args[2]));
            dragWatch = std::make_unique<DragWatch> (*dragScript);
            dragScript->startThread();
            dragWatch->startTimer (100);
            return;
        }
        std::vector<float> in = Feeder::defaultInput();
        for (int i = 3; i < args.size(); ++i)
            if (args[i].startsWith ("in="))      // a mono float32 48 kHz file instead of the default schedule
            {
                MemoryBlock mb; File (args[i].substring (3)).loadFileAsData (mb);
                in.assign ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / sizeof (float));
            }
        feeder = std::make_unique<Feeder> (*plugin, std::move (in));
        int shotMs = 0;
        for (int i = 3; i < args.size(); ++i)
            if (args[i].startsWith ("shot=")) shotMs = args[i].substring (5).getIntValue();
        if (shotMs > 0)
        {
            feeder->startThread (Thread::Priority::high);
            const File out (args[2]);
            Timer::callAfterDelay (2500 + shotMs, [this, out]
            {
                grabPng (*window->getContentComponent(), out);
                std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(),
                             window->getContentComponent()->getWidth(), window->getContentComponent()->getHeight());
                quit();
            });
            return;
        }
        capture = std::make_unique<Capture> (*window->getContentComponent(), *feeder, args[2]);
        for (int i = 3; i < args.size(); ++i)
            if (args[i].startsWith ("rows="))
            {
                capture->rowFrom = args[i].substring (5).upToFirstOccurrenceOf (",", false, false).getFloatValue();
                capture->rowTo = args[i].fromFirstOccurrenceOf (",", false, false).getFloatValue();
            }
        feeder->startThread (Thread::Priority::high);
        capture->startTimer (25);
    }
    void shutdown() override
    {
        if (dragScript) dragScript->stopThread (3000);
        if (feeder) feeder->stopThread (3000);
        capture.reset();
        if (window) window->setContentNonOwned (nullptr, false);
        window.reset();
        plugin.reset();
    }
private:
    AudioPluginFormatManager fm;
    std::vector<std::unique_ptr<AudioPluginInstance>> keepLoaded;
    std::unique_ptr<AudioPluginInstance> plugin;
    std::unique_ptr<DocumentWindow> window;
    std::unique_ptr<Feeder> feeder;
    std::unique_ptr<DragScript> dragScript;
    std::unique_ptr<DragWatch> dragWatch;
    std::unique_ptr<Capture> capture;
};

START_JUCE_APPLICATION (App)
