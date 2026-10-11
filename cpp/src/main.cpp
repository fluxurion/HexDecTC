// HexDec TC — SDL3 + OpenGL 3 + RmlUi + Skia.
// RmlUi lays out and styles the whole UI from ui/ui.rml + ui/ui.rcss
// (embedded as RCDATA; HEXDEC_UI_DIR=<dir> loads them from disk and F5
// hot-reloads). Skia (fx.cpp) paints the nebula backdrop and the window
// rim into a texture blitted behind the RmlUi layer every frame.
// All app state lives in App (app.cpp); this file only binds it.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>

#include <Windows.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>

#ifndef NIN_BALLOONUSERACTION
#define NIN_BALLOONUSERACTION (WM_USER + 5)
#endif
#ifndef NIN_POPUPMENU
#define NIN_POPUPMENU (WM_USER + 6)
#endif

#include "RmlUi_Include_GL3.h"
#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "app.hpp"
#include "core.hpp"
#include "fx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_set>

static const float DESIGN_W = 380.0f; // dp
static const float ROW_DP = 22.0f;    // opcode row height (matches rcss)

// ------------------------------------------------- embedded documents

/// Serves ui.rml/ui.rcss/opcodes.json from RCDATA (or HEXDEC_UI_DIR in
/// dev mode) and everything else (fonts) straight from disk.
class EmbeddedFiles : public Rml::FileInterface {
  public:
    explicit EmbeddedFiles(std::string dev_dir) : dev_(std::move(dev_dir)) {}

    Rml::FileHandle Open(const Rml::String& path) override {
        std::string name = path.substr(path.find_last_of("/\\") + 1);
        auto* f = new File;
        if (!dev_.empty() && read_disk(dev_ + "\\" + name, f->data))
            return (Rml::FileHandle)f;
        if (read_resource(name, f->data) || read_disk(path, f->data))
            return (Rml::FileHandle)f;
        delete f;
        return 0;
    }
    void Close(Rml::FileHandle h) override { delete (File*)h; }
    size_t Read(void* buf, size_t size, Rml::FileHandle h) override {
        auto* f = (File*)h;
        size_t n = std::min(size, f->data.size() - f->pos);
        memcpy(buf, f->data.data() + f->pos, n);
        f->pos += n;
        return n;
    }
    bool Seek(Rml::FileHandle h, long off, int origin) override {
        auto* f = (File*)h;
        long base = origin == SEEK_SET   ? 0
                    : origin == SEEK_CUR ? (long)f->pos
                                         : (long)f->data.size();
        if (base + off < 0 || base + off > (long)f->data.size())
            return false;
        f->pos = (size_t)(base + off);
        return true;
    }
    size_t Tell(Rml::FileHandle h) override { return ((File*)h)->pos; }

    /// Read an RCDATA resource by its rml-style name (upper, '.'->'_')
    /// or verbatim. Static so core.cpp can use it too.
    static bool read_resource(const std::string& name, std::string& out) {
        std::string rn;
        for (char c : name)
            rn += c == '.' ? '_' : (char)toupper((unsigned char)c);
        HRSRC r = FindResourceA(nullptr, rn.c_str(), MAKEINTRESOURCEA(10));
        if (!r)
            return false;
        HGLOBAL g = LoadResource(nullptr, r);
        out.assign((const char*)LockResource(g), SizeofResource(nullptr, r));
        return true;
    }

  private:
    struct File {
        std::string data;
        size_t pos = 0;
    };
    static bool read_disk(const std::string& p, std::string& out) {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        std::stringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }
    std::string dev_;
};

// -------------------------------------------------- data model mirror

struct UiVersion {
    std::string id, label;
    bool active = false;
    bool operator==(const UiVersion&) const = default;
};

struct UiRow {
    std::string name;
    bool sel = false;
    bool operator==(const UiRow&) const = default;
};

struct Ui {
    std::vector<UiVersion> versions;
    std::vector<UiRow> rows;
    bool aot = false, tray = true;
};

/// Dirty the bound variable only when the value actually changed —
/// keeps data-for DOM churn at zero while idle.
template <class T>
static void dirty_set(Rml::DataModelHandle& m, const char* name, T& cur,
                      const T& v) {
    if (!(cur == v)) {
        cur = v;
        m.DirtyVariable(name);
    }
}

// ------------------------------------------------- virtualized list

/// The opcode list may hold ~2400 rows; only the visible slice (+1)
/// is bound to data-for, with sp_top/sp_bot spacers preserving the
/// native scrollbar's total height.
struct ListSync {
    int offset = -1, count = -1;
    uint32_t rev = ~0u;
    std::string sel;
    bool flash_on = false;

    void sync(App& app, Ui& ui, Rml::ElementDocument* doc,
              Rml::DataModelHandle& model, float dpr) {
        Rml::Element* el = doc ? doc->GetElementById("oplist") : nullptr;
        if (!el)
            return;
        int n = (int)app.filtered.size();
        float vh_dp = el->GetClientHeight() / std::max(dpr, 0.01f);
        int cnt = std::max(1, (int)std::ceil(vh_dp / ROW_DP) + 1);
        int max_off = std::max(0, n - cnt);
        int off = std::clamp(
            (int)(el->GetScrollTop() / (ROW_DP * dpr)), 0, max_off);

        bool dirty = off != offset || cnt != count ||
                     rev != app.filter_rev || sel != app.selected;
        offset = off;
        count = cnt;
        rev = app.filter_rev;
        sel = app.selected;
        if (dirty) {
            std::vector<UiRow> rows;
            for (int i = off; i < std::min(n, off + cnt); ++i)
                rows.push_back(
                    {app.filtered[i], app.filtered[i] == app.selected});
            ui.rows = std::move(rows);
            model.DirtyVariable("rows");
        }

        set_h(doc->GetElementById("sp_top"), off * ROW_DP);
        set_h(doc->GetElementById("sp_bot"),
              std::max(0, n - off - cnt) * ROW_DP);

        // match flash: two border pulses over ~0.5s
        float now = (float)SDL_GetTicks() / 1000.0f;
        float rem = app.flash_until - now;
        if (rem > 0.0f) {
            float f = std::fabs(
                std::sin((0.5f - rem) * 4.0f * 3.14159f));
            char buf[80];
            snprintf(buf, sizeof buf, "rgba(61,157,245,%.2f)",
                     0.15f + 0.75f * f);
            el->SetProperty("border-color", buf);
            snprintf(buf, sizeof buf,
                     "rgba(58,155,220,%.2f) 0 0 12dp 0dp", 0.4f * f);
            el->SetProperty("box-shadow", buf);
            flash_on = true;
        } else if (flash_on) {
            el->RemoveProperty("border-color");
            el->RemoveProperty("box-shadow");
            flash_on = false;
        }
    }

    /// After ctx->Update (layout is fresh): honor a pending
    /// scroll-to-match request from App::check.
    void apply_scroll(App& app, Rml::ElementDocument* doc, float dpr) {
        if (app.scroll_idx < 0)
            return;
        Rml::Element* el = doc ? doc->GetElementById("oplist") : nullptr;
        if (!el) {
            app.scroll_idx = -1;
            return;
        }
        float vh_dp = el->GetClientHeight() / std::max(dpr, 0.01f);
        float target = std::max(
            0.0f, app.scroll_idx * ROW_DP - (vh_dp - ROW_DP) * 0.5f);
        el->SetScrollTop(target * dpr);
        app.scroll_idx = -1;
    }

    void reset() { offset = count = -1; rev = ~0u; flash_on = false; }

  private:
    static void set_h(Rml::Element* el, float dp) {
        if (el)
            el->SetProperty(Rml::PropertyId::Height,
                            Rml::Property(dp, Rml::Unit::DP));
    }
};

// ----------------------------------------------------------- toasts

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default: o += c;
        }
    }
    return o;
}

/// Mirrors App::toasts into DOM cards: slide/fade in, height-collapse
/// out, hover pauses the timer, click dismisses.
class Toasts : public Rml::EventListener {
  public:
    void sync(App& app, Rml::ElementDocument* doc, float dt) {
        Rml::Element* box = doc ? doc->GetElementById("toasts") : nullptr;
        if (!box)
            return;
        float now = (float)SDL_GetTicks() / 1000.0f;
        for (auto& t : app.toasts) {
            if (std::any_of(els_.begin(), els_.end(),
                            [&](auto& e) { return e.id == t.id; }))
                continue;
            auto wrap = doc->CreateElement("div");
            wrap->SetClass("twrap", true);
            static const char* icons[3] = {"\xEE\xA5\x86",  // E946 info
                                           "\xEE\x9C\xBE",  // E73E check
                                           "\xEE\x9C\x91"}; // E711 cancel
            int k = std::clamp(t.kind, 0, 2);
            wrap->SetInnerRML(
                "<div class=\"toast k" + std::to_string(k) +
                "\"><div class=\"accent\"/><div class=\"ticon\"><span "
                "class=\"ico\">" +
                icons[k] + "</span></div><div class=\"ttitle\">" +
                esc(t.title) + "</div><div class=\"ttext\">" + esc(t.body) +
                "</div><div class=\"tbar\"><div class=\"tfill\"/></div></div>");
            Rml::Element* w = box->InsertBefore(std::move(wrap),
                                                box->GetFirstChild());
            Rml::Element* card = w->GetFirstChild();
            card->AddEventListener(Rml::EventId::Click, this);
            els_.push_back({t.id, w, card, card->QuerySelector(".tfill")});
        }
        for (auto it = els_.begin(); it != els_.end();) {
            auto src = std::find_if(app.toasts.begin(), app.toasts.end(),
                                    [&](auto& t) { return t.id == it->id; });
            float age = src != app.toasts.end() ? now - src->at : TOAST_TTL;
            if (dismissed_.erase(it->card) && src != app.toasts.end()) {
                src->at = std::min(src->at, now - (TOAST_TTL - 0.35f));
                age = now - src->at;
            }
            bool closing = age >= TOAST_TTL - 0.35f;
            if (!closing && src != app.toasts.end() &&
                it->card->IsPseudoClassSet("hover") && age > 0.3f)
                src->at += dt; // pause while hovered
            if (it->frames++ == 1)
                it->card->SetClass("show", true);
            it->card->SetClass("hide", closing);
            if (it->fill)
                it->fill->SetProperty(
                    Rml::PropertyId::Width,
                    Rml::Property(std::clamp(100.0f * (1.0f - age / TOAST_TTL),
                                             0.0f, 100.0f),
                                  Rml::Unit::PERCENT));
            drive(it->wrap, it->col, !closing, dt, closing ? 0.3f : 0.22f);
            if (closing && it->col <= 0.0f) {
                it->card->RemoveEventListener(Rml::EventId::Click, this);
                box->RemoveChild(it->wrap);
                it = els_.erase(it);
            } else {
                ++it;
            }
        }
    }
    void reset() {
        els_.clear();
        dismissed_.clear();
    }

  private:
    void ProcessEvent(Rml::Event& ev) override {
        dismissed_.insert(ev.GetCurrentElement());
    }
    /// Height-collapse driver for .twrap wrappers.
    static void drive(Rml::Element* el, float& t, bool open, float dt,
                      float secs) {
        float d = dt / secs;
        t = open ? std::min(1.0f, t + d) : std::max(0.0f, t - d);
        if (t >= 1.0f) {
            el->SetProperty("height", "auto");
            return;
        }
        Rml::Element* inner = el->GetFirstChild();
        float full =
            inner ? inner->GetBox().GetSize(Rml::BoxArea::Margin).y : 0.0f;
        float e = t < 0.5f
                      ? 4 * t * t * t
                      : 1 - std::pow(-2 * t + 2, 3.0f) * 0.5f;
        el->SetProperty(Rml::PropertyId::Height,
                        Rml::Property(std::floor(full * e), Rml::Unit::PX));
    }
    struct El {
        uint32_t id;
        Rml::Element* wrap;
        Rml::Element* card;
        Rml::Element* fill;
        float col = 0;
        int frames = 0;
    };
    std::vector<El> els_;
    std::unordered_set<Rml::Element*> dismissed_;
};

// ------------------------------------------------- Skia backdrop pass

/// Uploads fx's BGRA frame into a GL texture and blits it to the default
/// framebuffer (y-flipped) before RmlUi composites its layer on top.
struct Backdrop {
    GLuint tex = 0, fbo = 0;
    int tw = 0, th = 0;

    void draw(int w, int h) {
        int bh = fx::buffer_h();
        if (!tex || tw != w || th < bh) {
            if (!tex) {
                glGenTextures(1, &tex);
                glGenFramebuffers(1, &fbo);
            }
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, bh, 0, GL_BGRA,
                         GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, tex, 0);
            tw = w;
            th = bh;
        }
        int rows = std::min(h, th);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, fx::stride() / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, rows, GL_BGRA,
                        GL_UNSIGNED_BYTE, fx::pixels());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, rows, 0, h, w, h - rows,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// ------------------------------------------------------ tray (Win32)

static const UINT WM_TRAYICON = WM_APP + 1;
static const UINT WM_RESTORE = WM_APP + 2; // single-instance restore

static HWND g_hwnd = nullptr;
static SDL_Window* g_win = nullptr;
static bool g_tray_added = false;
static bool g_quit = false;
static int g_intro = 0; // entrance animation phase

static void tray_add() {
    if (g_tray_added || !g_hwnd)
        return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = g_hwnd;
    d.uID = 1;
    d.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    d.uCallbackMessage = WM_TRAYICON;
    d.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    wcscpy_s(d.szTip, L"HexDec TC");
    if (Shell_NotifyIconW(NIM_ADD, &d)) {
        d.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &d);
        g_tray_added = true;
    }
}

static void tray_remove() {
    if (!g_tray_added)
        return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = g_hwnd;
    d.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &d);
    g_tray_added = false;
}

static void tray_balloon() {
    if (!g_tray_added)
        return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = g_hwnd;
    d.uID = 1;
    d.uFlags = NIF_INFO;
    wcscpy_s(d.szInfoTitle, L"HexDec TC");
    wcscpy_s(d.szInfo,
             L"The application is still running in the system tray.\n"
             L"Click here to restore.");
    d.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

static void show_window() {
    if (!g_win)
        return;
    SDL_ShowWindow(g_win);
    SDL_RestoreWindow(g_win);
    SDL_RaiseWindow(g_win);
    g_intro = 1; // replay the entrance animation
}

static void hide_to_tray() {
    tray_add();
    tray_balloon();
    SDL_HideWindow(g_win);
}

static void tray_menu() {
    HMENU m = CreatePopupMenu();
    if (!m)
        return;
    AppendMenuW(m, MF_STRING, 1, L"Show");
    AppendMenuW(m, MF_STRING, 2, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x,
                              pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
    if (cmd == 1)
        show_window();
    else if (cmd == 2)
        g_quit = true;
}

static bool SDLCALL win_hook(void*, MSG* msg) {
    if (msg->hwnd != g_hwnd)
        return true;
    if (msg->message == WM_RESTORE) {
        show_window();
        return false;
    }
    if (msg->message != WM_TRAYICON)
        return true;
    switch (LOWORD(msg->lParam)) {
    case WM_LBUTTONUP:
    case NIN_SELECT:
    case NIN_BALLOONUSERACTION:
        show_window();
        break;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
    case NIN_POPUPMENU:
        tray_menu();
        break;
    }
    return false;
}

// ---------------------------------------------------- window chrome

static float g_title_logical = 36.0f;

static SDL_HitTestResult SDLCALL hit_test(SDL_Window* win,
                                          const SDL_Point* p, void*) {
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    const int B = 6;
    bool l = p->x < B, r = p->x >= w - B;
    bool t = p->y < B, b = p->y >= h - B;
    if (t && l) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (t && r) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (b && l) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (b && r) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (t) return SDL_HITTEST_RESIZE_TOP;
    if (b) return SDL_HITTEST_RESIZE_BOTTOM;
    if (l) return SDL_HITTEST_RESIZE_LEFT;
    if (r) return SDL_HITTEST_RESIZE_RIGHT;
    if (p->y < g_title_logical && p->x < w - g_title_logical * 4.3f)
        return SDL_HITTEST_DRAGGABLE;
    return SDL_HITTEST_NORMAL;
}

// --------------------------------------------------------------- main

int main(int, char**) {
    // single instance: signal the running copy and bail out
    HANDLE mtx =
        CreateMutexW(nullptr, TRUE, L"Local\\HexDecTC.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND w = FindWindowW(nullptr, L"HexDec TC")) {
            PostMessageW(w, WM_RESTORE, 0, 0);
            SetForegroundWindow(w);
        }
        if (mtx)
            CloseHandle(mtx);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* win = SDL_CreateWindow(
        "HexDec TC", (int)DESIGN_W, 560,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS |
            SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!win)
        return 1;
    SDL_SetWindowMinimumSize(win, 300, 400);
    SDL_SetWindowHitTest(win, hit_test, nullptr);
    if (SDL_PropertiesID props = SDL_GetWindowProperties(win)) {
        g_hwnd = (HWND)SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (g_hwnd) {
            int pref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(g_hwnd, 33, &pref, sizeof pref);
            if (HICON ic = LoadIconW(GetModuleHandleW(nullptr),
                                     MAKEINTRESOURCEW(1))) {
                SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)ic);
                SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)ic);
            }
        }
    }
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);
    if (!RmlGL3::Initialize())
        return 1;

    // UI scale: dp ratio = display scale; window coords = pixels/density.
    float dpr = SDL_GetWindowDisplayScale(win);
    if (dpr <= 0)
        dpr = 1;
    float density = SDL_GetWindowPixelDensity(win);
    if (density <= 0)
        density = 1;
    SDL_SetWindowSize(win, (int)std::lround(DESIGN_W * dpr / density),
                    (int)std::lround(560 * dpr / density));
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    const char* dev = getenv("HEXDEC_UI_DIR");
    EmbeddedFiles files(dev ? dev : "");
    SystemInterface_SDL sys(win);
    RenderInterface_GL3 render;
    TextInputMethodEditor_SDL ime;
    if (!render)
        return 1;
    Rml::SetFileInterface(&files);
    Rml::SetSystemInterface(&sys);
    Rml::SetRenderInterface(&render);
    Rml::SetTextInputHandler(&ime);
    Rml::Initialise();
    for (const char* f : {"segoeui.ttf", "segoeuib.ttf", "consola.ttf",
                          "segmdl2.ttf"})
        Rml::LoadFontFace(std::string("C:\\Windows\\Fonts\\") + f);

    int pw, ph;
    SDL_GetWindowSizeInPixels(win, &pw, &ph);
    render.SetViewport(pw, ph);
    Rml::Context* ctx = Rml::CreateContext("main", {pw, ph});
    ctx->SetDensityIndependentPixelRatio(dpr);

    g_win = win;
    tray_add();
    SDL_SetWindowsMessageHook(win_hook, nullptr);

    App app;
    app.init();
    if (app.settings.always_on_top)
        SDL_SetWindowAlwaysOnTop(win, true);
    Ui ui;

    // ---- data model
    Rml::DataModelConstructor dm = ctx->CreateDataModel("app");
    if (auto vs = dm.RegisterStruct<UiVersion>()) {
        vs.RegisterMember("id", &UiVersion::id);
        vs.RegisterMember("label", &UiVersion::label);
        vs.RegisterMember("active", &UiVersion::active);
    }
    if (auto rs = dm.RegisterStruct<UiRow>()) {
        rs.RegisterMember("name", &UiRow::name);
        rs.RegisterMember("sel", &UiRow::sel);
    }
    dm.RegisterArray<std::vector<UiVersion>>();
    dm.RegisterArray<std::vector<UiRow>>();
    dm.Bind("versions", &ui.versions);
    dm.Bind("rows", &ui.rows);
    dm.Bind("aot", &ui.aot);
    dm.Bind("tray", &ui.tray);
    // two-way text fields write straight into App
    dm.Bind("search", &app.search);
    dm.Bind("hex", &app.hex);
    dm.Bind("dec", &app.dec);

    bool run = true;
    std::unique_ptr<Rml::DataModelHandle> model;
    auto dirty = [&](const char* n) { model->DirtyVariable(n); };
    using Args = const Rml::VariantList&;
    auto arg = [](Args a, size_t i) {
        return a.size() > i ? a[i].Get<Rml::String>() : Rml::String();
    };
    auto field_val = [](Rml::Event& ev) -> std::string {
        if (auto* c = dynamic_cast<Rml::ElementFormControl*>(
                ev.GetCurrentElement()))
            return c->GetValue();
        return {};
    };
    auto on = [&](const char* name, auto fn) {
        dm.BindEventCallback(name, [fn](Rml::DataModelHandle, Rml::Event& ev,
                                        Args a) { fn(ev, a); });
    };
    auto close_request = [&] {
        if (app.settings.tray_on_close)
            hide_to_tray();
        else
            run = false;
    };
    on("win", [&](Rml::Event&, Args a) {
        auto w = arg(a, 0);
        if (w == "min")
            SDL_MinimizeWindow(win);
        else if (w == "max")
            (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)
                ? SDL_RestoreWindow(win)
                : SDL_MaximizeWindow(win);
        else
            close_request();
    });
    on("select_version", [&](Rml::Event&, Args a) {
        app.switch_version(arg(a, 0));
    });
    on("toggle", [&](Rml::Event&, Args a) {
        if (arg(a, 0) == "aot") {
            app.settings.always_on_top = !app.settings.always_on_top;
            SDL_SetWindowAlwaysOnTop(win, app.settings.always_on_top);
        } else {
            app.settings.tray_on_close = !app.settings.tray_on_close;
        }
        core::save_settings(app.settings);
    });
    on("search_changed", [&](Rml::Event& ev, Args) {
        app.search = field_val(ev);
        app.refilter();
    });
    on("hex_changed", [&](Rml::Event& ev, Args) {
        app.hex = field_val(ev);
        app.on_hex_input();
        dirty("dec");
    });
    on("dec_changed", [&](Rml::Event& ev, Args) {
        app.dec = field_val(ev);
        app.on_dec_input();
        dirty("hex");
    });
    on("pick", [&](Rml::Event&, Args a) {
        app.select_name(arg(a, 0));
        dirty("hex");
        dirty("dec");
    });
    on("paste", [&](Rml::Event&, Args a) {
        char* t = SDL_GetClipboardText();
        if (!t)
            return;
        std::string s = t;
        SDL_free(t);
        if (arg(a, 0) == "hex") {
            app.hex = s;
            app.on_hex_input();
        } else {
            app.dec = s;
            app.on_dec_input();
        }
        dirty("hex");
        dirty("dec");
    });
    on("copy", [&](Rml::Event&, Args a) {
        const std::string& v = arg(a, 0) == "hex" ? app.hex : app.dec;
        SDL_SetClipboardText(v.c_str());
        app.toast(0, "Clipboard", "Value copied.");
    });
    on("copy_op", [&](Rml::Event&, Args) {
        if (app.selected.empty()) {
            app.toast(0, "Clipboard", "No opcode selected.");
        } else {
            SDL_SetClipboardText(app.selected.c_str());
            app.toast(1, "Clipboard", app.selected);
        }
    });
    on("clear_all", [&](Rml::Event&, Args) {
        app.clear_all();
        dirty("hex");
        dirty("dec");
    });
    model = std::make_unique<Rml::DataModelHandle>(dm.GetModelHandle());

    // ---- document
    Rml::ElementDocument* doc = nullptr;
    Toasts toasts;
    ListSync list;
    auto load_doc = [&] {
        if (doc)
            doc->Close();
        toasts.reset();
        list.reset();
        Rml::Factory::ClearStyleSheetCache();
        doc = ctx->LoadDocument("ui.rml");
        if (doc)
            doc->Show();
    };
    load_doc();
    g_intro = 2; // #main starts with class="pre"; removing it fades in

    Backdrop backdrop;
    std::vector<fx::Decal> decals;
    Uint64 prev = SDL_GetTicksNS();
    bool shown = false;

    while (run && !g_quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                close_request();
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                render.SetViewport(ev.window.data1, ev.window.data2);
                ctx->SetDimensions({ev.window.data1, ev.window.data2});
                break;
            case SDL_EVENT_TEXT_EDITING: ime.HandleEdit(ev.edit); continue;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_F5 && dev) {
                    load_doc();
                    continue;
                }
                break;
            default: break;
            }
            RmlSDL::InputEventHandler(ctx, win, ev);
        }
        if (shown && (SDL_GetWindowFlags(win) & SDL_WINDOW_HIDDEN)) {
            SDL_Delay(60); // parked in the tray: no rendering needed
            continue;
        }
        Uint64 nowns = SDL_GetTicksNS();
        float dt = std::clamp((nowns - prev) / 1e9f, 0.0f, 0.05f);
        prev = nowns;

        app.pump();
        // mirror app -> ui (dirty only what changed)
        dirty_set(*model, "aot", ui.aot, app.settings.always_on_top);
        dirty_set(*model, "tray", ui.tray, app.settings.tray_on_close);
        {
            std::vector<UiVersion> vs;
            for (auto& v : app.versions)
                vs.push_back({v, v, v == app.settings.version});
            dirty_set(*model, "versions", ui.versions, vs);
        }
        list.sync(app, ui, doc, *model, dpr);

        dpr = ctx->GetDensityIndependentPixelRatio();
        density = SDL_GetWindowPixelDensity(win);
        if (density <= 0)
            density = 1;
        g_title_logical = 36.0f * dpr / density;

        // entrance: add .pre for a frame, then remove -> CSS transition
        if (doc && g_intro == 1) {
            if (auto* m = doc->GetElementById("main"))
                m->SetClass("pre", true);
            g_intro = 2;
        } else if (doc && g_intro == 2) {
            if (auto* m = doc->GetElementById("main"))
                m->SetClass("pre", false);
            g_intro = 0;
        }

        toasts.sync(app, doc, dt);
        ctx->Update();
        list.apply_scroll(app, doc, dpr);

        // Skia decor: nebula backdrop + glowing window rim
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
        decals.clear();
        int lw, lh;
        SDL_GetWindowSize(win, &lw, &lh);
        decals.push_back({fx::Kind::Frame, 0, 0, (float)lw, (float)lh, 1,
                          1, 1, 1, 8, 1});
        fx::render(pw, ph, density, (float)(nowns / 1e9), decals);

        glViewport(0, 0, pw, ph);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        backdrop.draw(pw, ph);
        render.BeginFrame();
        ctx->Render();
        render.EndFrame();
        SDL_GL_SwapWindow(win);
        if (!shown) {
            SDL_ShowWindow(win);
            shown = true;
        }
    }

    tray_remove();
    if (mtx)
        CloseHandle(mtx);
    Rml::Shutdown();
    RmlGL3::Shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
