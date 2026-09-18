#include "PCH.h"

#include <Windows.h>

#include "ItemPreviewSuppression.h"
#include "MenuStudioApi.h"

namespace AP::MenuStudioApi {

    namespace {
        using SetItemPreviewSuppressed_t = bool (*)(const char*, bool);

        constexpr auto kOwnerId = "ApparelPreview";
        ItemPreviewSuppressionClaim g_claim;

        SetItemPreviewSuppressed_t Resolve() {
            static const auto api = []() -> SetItemPreviewSuppressed_t {
                const auto module = GetModuleHandleW(L"MenuStudio.dll");
                if (!module) {
                    spdlog::info("MenuStudioApi: MenuStudio.dll not loaded; using local item "
                                 "preview handling.");
                    return nullptr;
                }
                const auto fn = reinterpret_cast<SetItemPreviewSuppressed_t>(
                    GetProcAddress(module, "MenuStudio_SetItemPreviewSuppressed"));
                if (!fn) {
                    spdlog::info("MenuStudioApi: item preview suppression export absent; using "
                                 "local handling.");
                    return nullptr;
                }
                spdlog::info("MenuStudioApi: owner-scoped item preview suppression available.");
                return fn;
            }();
            return api;
        }
    }

    bool Available() { return Resolve() != nullptr; }

    bool SetItemPreviewSuppressed(bool a_suppressed) {
        const auto setSuppressed = Resolve();
        if (!setSuppressed) {
            return false;
        }
        if (!g_claim.NeedsPublish(a_suppressed)) {
            return true;
        }
        if (!setSuppressed(kOwnerId, a_suppressed)) {
            spdlog::error("MenuStudioApi: item preview suppression {} was refused for owner '{}'.",
                          a_suppressed ? "acquire" : "release", kOwnerId);
            return false;
        }
        g_claim.RecordAccepted(a_suppressed);
        return true;
    }

}  // namespace AP::MenuStudioApi
