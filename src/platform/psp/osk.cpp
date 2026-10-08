#include "platform/psp/osk.h"

#include "runtime/log.h"

#include <psputility.h>

#include <cstring>
#include <vector>

namespace rs::osk {

namespace {

/* UTF-8 -> UTF-16 (BMP; anything else becomes '?'), NUL-terminated. */
std::vector<unsigned short> toUtf16(const std::string& s) {
    std::vector<unsigned short> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = '?';
        size_t n = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu);
            n = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) |
                 (s[i + 2] & 0x3Fu);
            n = 3;
        } else if ((c & 0xF8) == 0xF0) {
            n = 4;
        }
        out.push_back(static_cast<unsigned short>(cp));
        i += n;
    }
    out.push_back(0);
    return out;
}

std::string toUtf8(const unsigned short* s) {
    std::string out;
    for (; s && *s; ++s) {
        const unsigned cp = *s;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

struct Session {
    bool active = false;   /* reported anything but NONE yet */
    bool done = false;
};

/* Between our frame and the swap: advance the keyboard one frame. */
void step(void* ctx) {
    Session& s = *static_cast<Session*>(ctx);
    const int status = sceUtilityOskGetStatus();
    if (status != PSP_UTILITY_DIALOG_NONE) s.active = true;
    switch (status) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityOskUpdate(1);
            break;
        case PSP_UTILITY_DIALOG_QUIT:
            sceUtilityOskShutdownStart();
            break;
        case PSP_UTILITY_DIALOG_NONE:
            s.done = s.active;   /* closed (not merely not started yet) */
            break;
        default:   /* initialising or shutting down */
            break;
    }
}

}  // namespace

Result run(gfx::Renderer& renderer, const char* title,
           const std::string& initial, int maxChars, std::string& out,
           void (*drawBackground)(void*), void* ctx) {
    std::vector<unsigned short> desc = toUtf16(title ? title : "");
    std::vector<unsigned short> intext = toUtf16(initial);
    std::vector<unsigned short> outtext(size_t(maxChars) + 1, 0);

    SceUtilityOskData data;
    std::memset(&data, 0, sizeof data);
    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.lines = 1;
    data.unk_24 = 1;
    data.desc = desc.data();
    data.intext = intext.data();
    data.outtextlength = maxChars + 1;
    data.outtextlimit = maxChars;
    data.outtext = outtext.data();

    SceUtilityOskParams params;
    std::memset(&params, 0, sizeof params);
    params.base.size = sizeof params;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                                &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,
                                &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    const int started = sceUtilityOskInitStart(&params);
    if (started < 0) {
        RS_LOGW("osk: could not start (%08x)", unsigned(started));
        return Result::Failed;
    }
    Session session;
    while (!session.done) {
        renderer.beginFrame(0xFF000000u);
        if (drawBackground) drawBackground(ctx);
        renderer.endFrame(&step, &session);
    }
    if (data.result == PSP_UTILITY_OSK_RESULT_CANCELLED ||
        params.base.result != 0)
        return Result::Cancelled;
    out = toUtf8(outtext.data());
    return Result::Accepted;
}

}  // namespace rs::osk
