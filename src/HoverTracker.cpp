#include "HoverTracker.h"

#include "InputListener.h"
#include "ItemPreviewSuppression.h"
#include "MenuWatcher.h"
#include "MenuStudioApi.h"
#include "NavButton.h"
#include "PreviewSession.h"
#include "Settings.h"
#include "VersionCheck.h"

#include <thread>

namespace AP {

    namespace {
        // GFx 3.x GFxKeyEvent layout (CommonLibSSE-NG does not declare it):
        // the key code directly follows GFxEvent::type - the same packing as
        // GFxMouseEvent, whose first member sits at 0x04.
        struct GFxKeyEventView : RE::GFxEvent {
            std::uint32_t keyCode;  // 04
        };
    }

    void HoverTracker::Install() {
        // ItemCard hover hook. On AE 1.6.1170 this call moved to +0x22C (the AE
        // function 51897 has extra early-body code); the hooked instruction
        // stream is byte-identical to SE 51019 (SE +0x114 / AE +0x22C) (verified vs the AE binary).
        const auto callOffset = VersionCheck::HoverTrackerCallOffset();
        if (callOffset == 0) {
            spdlog::warn("HoverTracker: item-card call was not locatable; using the live "
                         "menu-selection fallback.");
        } else {
            const REL::Relocation<std::uintptr_t> site{
                REL::RelocationID(51019, 51897), callOffset
            };
            if (*reinterpret_cast<std::uint8_t*>(site.address()) != 0xE8) {
                spdlog::warn("HoverTracker: item-card site is {:02X}, not E8; using the live "
                             "menu-selection fallback.",
                             *reinterpret_cast<std::uint8_t*>(site.address()));
            } else {
                auto& trampoline = SKSE::GetTrampoline();
                func_ = trampoline.write_call<5>(site.address(), Thunk);
                spdlog::info("HoverTracker: item-card hook installed at 51019 "
                             "(SE +0x114 / AE +0x22C).");
            }
        }

        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_Inventory3DManager[0] };
        origCanProcess_ = vtbl.write_vfunc(0x1, CanProcessGate);

        REL::Relocation<std::uintptr_t> contVtbl{ RE::VTABLE_ContainerMenu[0] };
        origContainerPM_ = contVtbl.write_vfunc(0x4, ContainerProcessMessage);
        origContainerAdvance_ = contVtbl.write_vfunc(0x5, ContainerAdvanceMovie);
        REL::Relocation<std::uintptr_t> bartVtbl{ RE::VTABLE_BarterMenu[0] };
        origBarterPM_ = bartVtbl.write_vfunc(0x4, BarterProcessMessage);
        origBarterAdvance_ = bartVtbl.write_vfunc(0x5, BarterAdvanceMovie);
        REL::Relocation<std::uintptr_t> invVtbl{ RE::VTABLE_InventoryMenu[0] };
        origInventoryPM_ = invVtbl.write_vfunc(0x4, InventoryProcessMessage);
        origInventoryAdvance_ = invVtbl.write_vfunc(0x5, InventoryAdvanceMovie);
        spdlog::info("HoverTracker: inspect gates and live menu-selection fallback installed.");
    }

    bool HoverTracker::CanProcessGate(RE::Inventory3DManager* a_this, RE::InputEvent* a_event) {
        if (a_event && MenuWatcher::IsArmed() && Settings::GetSingleton().blockApparelInspect) {
            const auto* ue = RE::UserEvents::GetSingleton();
            if (ue && a_event->QUserEvent() == ue->itemZoom) {
                auto* entry = hovered_.load();
                auto* obj   = entry ? entry->GetObject() : nullptr;
                const bool eat = obj && obj->As<RE::TESObjectARMO>() != nullptr;
                spdlog::debug("I3DM CanProcess(itemZoom): apparel={} -> {}", eat,
                              eat ? "declined" : "passed");
                if (eat) {
                    return false;  // no zoom/inspect for apparel - key toggles the preview
                }
            }
        }
        return origCanProcess_(a_this, a_event);
    }

    bool HoverTracker::ShouldEatZoomMessage(RE::UIMessage& a_message) {
        if (!MenuWatcher::IsArmed() || !Settings::GetSingleton().blockApparelInspect) {
            return false;
        }
        if (a_message.type != RE::UI_MESSAGE_TYPE::kUserEvent || !a_message.data) {
            return false;
        }
        const auto* ue   = RE::UserEvents::GetSingleton();
        auto*       data = static_cast<RE::BSUIMessageData*>(a_message.data);
        if (!ue || data->fixedStr != ue->itemZoom) {
            return false;
        }
        auto* entry = hovered_.load();
        auto* obj   = entry ? entry->GetObject() : nullptr;
        const bool eat = obj && obj->As<RE::TESObjectARMO>() != nullptr;
        spdlog::debug("menu ProcessMessage(itemZoom userEvent): apparel={} -> {}", eat,
                      eat ? "eaten" : "passed");
        return eat;
    }

    bool HoverTracker::ShouldEatScaleformKey(RE::UIMessage& a_message) {
        if (!MenuWatcher::IsArmed() || !Settings::GetSingleton().blockApparelInspect) {
            return false;
        }
        if (a_message.type != RE::UI_MESSAGE_TYPE::kScaleformEvent || !a_message.data) {
            return false;
        }
        const auto* data = static_cast<RE::BSUIScaleformData*>(a_message.data);
        const auto* ev   = data->scaleformEvent;
        using ET = RE::GFxEvent::EventType;
        if (!ev || (ev->type != ET::kKeyDown && ev->type != ET::kKeyUp)) {
            return false;
        }
        const auto code   = static_cast<const GFxKeyEventView*>(ev)->keyCode;
        const auto wanted = InputListener::ResolvedPadScaleformKey();
        // Two ways this key event belongs to the preview button:
        //  (a) it IS the 266+bit code we computed from the binding, or
        //  (b) it arrived in the correlation window of a physical pad-button
        //      edge - the engine delivers gamepad buttons to Flash as
        //      TRANSLATED KEYBOARD codes (field log 2026-07-12: R3 came in
        //      as 112/F1 and drove SkyUI's column sort AND its filter box;
        //      273 never arrived at all).
        const bool isPreviewCode = wanted != 0 && code == wanted;
        const bool correlated    = InputListener::PadPressInFlight();
        if (!isPreviewCode && !correlated) {
            if (ev->type == ET::kKeyDown) {
                spdlog::debug("scaleform keyDown {} passed (preview key {}, no pad edge)",
                              code, wanted);
            }
            return false;
        }
        // Eat for apparel AND for empty hovers. The empty-hover press has no
        // preview target and no zoom target - the only consumer left is the
        // UI's own binding on the same physical button (SkyUI/Vel'dun filter
        // box), which is exactly the mode flip the field reported: scrolling
        // on controller clears the hover for a beat, and a press in that
        // window fell through to Flash. Non-apparel hovers keep vanilla
        // item zoom.
        auto* entry = hovered_.load();
        auto* obj   = entry ? entry->GetObject() : nullptr;
        const bool eat = !entry || (obj && obj->As<RE::TESObjectARMO>() != nullptr);
        if (eat && ev->type == ET::kKeyDown) {
            spdlog::debug("scaleform key {} eaten ({}; {}; preview button)", code,
                          isPreviewCode ? "bound code" : "pad-edge correlated",
                          entry ? "apparel hovered" : "no hover");
        }
        return eat;  // both down and up, so Flash never sees half a press
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::ContainerProcessMessage(RE::ContainerMenu* a_this,
                                                                 RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origContainerPM_(a_this, a_message);
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::BarterProcessMessage(RE::BarterMenu* a_this,
                                                              RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origBarterPM_(a_this, a_message);
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::InventoryProcessMessage(RE::InventoryMenu* a_this,
                                                                  RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origInventoryPM_(a_this, a_message);
    }

    void HoverTracker::ContainerAdvanceMovie(RE::ContainerMenu* a_this, float a_interval,
                                              std::uint32_t a_currentTime) {
        origContainerAdvance_(a_this, a_interval, a_currentTime);
        SyncSelected();
    }

    void HoverTracker::BarterAdvanceMovie(RE::BarterMenu* a_this, float a_interval,
                                           std::uint32_t a_currentTime) {
        origBarterAdvance_(a_this, a_interval, a_currentTime);
        SyncSelected();
    }

    void HoverTracker::InventoryAdvanceMovie(RE::InventoryMenu* a_this, float a_interval,
                                              std::uint32_t a_currentTime) {
        origInventoryAdvance_(a_this, a_interval, a_currentTime);
        SyncSelected();
    }

    RE::InventoryEntryData* HoverTracker::GetHovered() { return hovered_.load(); }

    RE::InventoryEntryData* HoverTracker::GetSelectedOrHovered() {
        SyncSelected();
        return hovered_.load();
    }

    void HoverTracker::ClearHovered() {
        const auto& cfg             = Settings::GetSingleton();
        hovered_                    = nullptr;
        lastAutoMode_               = cfg.previewOnHover;
        lastSuppressHoverModel_     = cfg.suppressHoverModel;
        lastClearModelOnPreview_    = cfg.clearModelOnPreview;
        SyncItemPreviewSuppression();
    }

    bool HoverTracker::SyncItemPreviewSuppression() {
        auto* entry = hovered_.load();
        auto* obj   = entry ? entry->GetObject() : nullptr;
        auto* armo  = obj ? obj->As<RE::TESObjectARMO>() : nullptr;
        const auto& cfg = Settings::GetSingleton();
        const bool selectedExactEntryIsPreviewed =
            armo && PreviewSession::GetSingleton().IsPreviewedEntry(
                        obj->GetFormID(), PreviewSession::VariantKey(entry));
        const bool suppress = ShouldSuppressItemPreview3D({
            .selectedIsApparel = armo != nullptr,
            .suppressHoverModel = cfg.suppressHoverModel,
            .clearModelOnPreview = cfg.clearModelOnPreview,
            .selectedExactEntryIsPreviewed = selectedExactEntryIsPreviewed,
        });
        return MenuStudioApi::SetItemPreviewSuppressed(suppress);
    }

    void HoverTracker::SyncSelected() {
        if (!MenuWatcher::IsArmedAndMenuOpen()) {
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return;
        }

        RE::ItemList* list = nullptr;
        if (const auto container = ui->GetMenu<RE::ContainerMenu>()) {
            list = container->GetRuntimeData().itemList;
        } else if (const auto barter = ui->GetMenu<RE::BarterMenu>()) {
            list = barter->GetRuntimeData().itemList;
        } else if (const auto inventory = ui->GetMenu<RE::InventoryMenu>()) {
            list = inventory->GetRuntimeData().itemList;
        }
        if (!list || list->updatePending) {
            return;  // list rebuild in progress; keep the last stable selection
        }

        auto* selected = list->GetSelectedItem();
        ObserveSelected(selected ? selected->data.objDesc : nullptr);
    }

    void HoverTracker::ObserveSelected(RE::InventoryEntryData* a_entry) {
        const auto& cfg         = Settings::GetSingleton();
        const bool  modeChanged = lastAutoMode_ != cfg.previewOnHover;
        const bool itemModelPolicyChanged =
            lastSuppressHoverModel_ != cfg.suppressHoverModel ||
            lastClearModelOnPreview_ != cfg.clearModelOnPreview;
        lastAutoMode_            = cfg.previewOnHover;
        lastSuppressHoverModel_  = cfg.suppressHoverModel;
        lastClearModelOnPreview_ = cfg.clearModelOnPreview;

        auto* previous = hovered_.exchange(a_entry);
        if (previous == a_entry && !modeChanged && !itemModelPolicyChanged) {
            return;
        }

        auto* obj  = a_entry ? a_entry->GetObject() : nullptr;
        auto* armo = obj ? obj->As<RE::TESObjectARMO>() : nullptr;
        const bool selectedExactEntryIsPreviewed =
            armo && PreviewSession::GetSingleton().IsPreviewedEntry(
                        obj->GetFormID(), PreviewSession::VariantKey(a_entry));
        const bool delegated = SyncItemPreviewSuppression();
        if (armo) {
            // Clear only for the actual selected row. The old item-card hook is
            // also invoked while some UI overhauls build every row, so treating
            // its argument as "hovered" hid models and selected the final row
            // in the rebuild rather than the row under the cursor.
            const bool hidePreviewed =
                cfg.clearModelOnPreview && selectedExactEntryIsPreviewed;
            const bool suppressItemPreview = cfg.suppressHoverModel || hidePreviewed;
            if (suppressItemPreview && !delegated) {
                if (auto* i3d = RE::Inventory3DManager::GetSingleton()) {
                    i3d->UnloadInventoryItem();
                    spdlog::debug("item3d cleared for selected '{}'{}",
                                  a_entry->GetDisplayName(),
                                  hidePreviewed ? " (previewed)" : "");
                }
            }
        }

        auto*       ui   = RE::UI::GetSingleton();
        const char* menu = "Container";
        if (ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            menu = "Barter";
        } else if (ui && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            menu = "Inventory";
        }
        spdlog::debug("selected[{}] {:08X} '{}' armo={} variant={:08X} entry={} thread={}",
                      menu, obj ? obj->GetFormID() : 0,
                      a_entry ? a_entry->GetDisplayName() : "<none>", armo != nullptr,
                      a_entry ? PreviewSession::VariantKey(a_entry) : 0,
                      static_cast<const void*>(a_entry),
                      std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF);

        if (cfg.showPreviewButton) {
            // Defer so this runs after the menu finishes rebuilding its bottom
            // bar for the newly selected row.
            SKSE::GetTaskInterface()->AddUITask([]() { NavButton::Refresh(); });
        }

        if (cfg.previewOnHover) {
            if (armo) {
                PreviewSession::GetSingleton().PreviewOnly(
                    armo, PreviewSession::VariantKey(a_entry));
            } else {
                PreviewSession::GetSingleton().Clear();
            }
        }
    }

    std::int64_t HoverTracker::Thunk(RE::InventoryEntryData* a_entry) {
        // Preserve the displaced call first, then read selectedIndex from the
        // live list. The argument is not reliably the highlighted row: several
        // popular UI stacks invoke this call once for every row during a list
        // rebuild.
        const auto result = func_(a_entry);
        SyncSelected();
        return result;
    }

}  // namespace AP
