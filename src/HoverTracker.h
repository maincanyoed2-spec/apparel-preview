#pragma once
#include "ItemPreviewSuppression.h"
#include "PCH.h"

namespace AP {

    // Tracks the highlighted InventoryEntryData in ItemCard-family menus via a
    // trampoline hook inside ItemCard::ShowItemData (SE REL::ID 51019 + 0x114).
    class HoverTracker {
    public:
        static void Install();  // call from SKSEPluginLoad after SKSE::Init

        // Latest hovered entry while a supported menu is open; nullptr otherwise.
        // Only valid while the menu is open (entries die with the menu).
        [[nodiscard]] static RE::InventoryEntryData* GetHovered();
        // Refresh from the live menu's selectedIndex first. This is the
        // resilient path used by input handling when another UI mod bypasses
        // or replaces the item-card call hook.
        [[nodiscard]] static RE::InventoryEntryData* GetSelectedOrHovered();
        static void ClearHovered();

        // Re-evaluate this plugin's owner-scoped claim on Menu Studio's
        // floating item preview. Returns true when Menu Studio accepted the
        // state, false when the legacy Inventory3DManager fallback is needed.
        static bool SyncItemPreviewSuppression();

    private:
        static std::int64_t Thunk(RE::InventoryEntryData* a_entry);
        static void SyncSelected();
        static void ObserveSelected(RE::InventoryEntryData* a_entry);
        // Inventory3DManager handles the 'Item Zoom' button natively (it is a
        // MenuEventHandler) and entering that mode hides the HUD. This vfunc
        // gate declines the zoom for apparel while armed, so the preview key
        // toggles the preview without the HUD-hiding inspect mode.
        static bool CanProcessGate(RE::Inventory3DManager* a_this, RE::InputEvent* a_event);
        // Second delivery path: menus receive the user event as a UIMessage and
        // forward it to Flash, which can also start the zoom. Eat it there too.
        static RE::UI_MESSAGE_RESULTS ContainerProcessMessage(RE::ContainerMenu* a_this,
                                                              RE::UIMessage& a_message);
        static RE::UI_MESSAGE_RESULTS BarterProcessMessage(RE::BarterMenu* a_this,
                                                           RE::UIMessage& a_message);
        static RE::UI_MESSAGE_RESULTS InventoryProcessMessage(RE::InventoryMenu* a_this,
                                                              RE::UIMessage& a_message);
        // Poll selectedIndex after each menu movie advance. Unlike the
        // item-card hook, this path is part of every supported menu and works
        // with UI overhauls that build item cards differently.
        static void ContainerAdvanceMovie(RE::ContainerMenu* a_this, float a_interval,
                                          std::uint32_t a_currentTime);
        static void BarterAdvanceMovie(RE::BarterMenu* a_this, float a_interval,
                                       std::uint32_t a_currentTime);
        static void InventoryAdvanceMovie(RE::InventoryMenu* a_this, float a_interval,
                                          std::uint32_t a_currentTime);
        static bool ShouldEatZoomMessage(RE::UIMessage& a_message);
        // Third delivery path: the RAW gamepad key also reaches Flash as a
        // kScaleformEvent key message, where UI mods bind their own actions
        // (Edge UI's explorer/filter panel sits on R3 this way). When the
        // press is ours - armed, apparel hovered, key == the resolved preview
        // button - starve Flash of it so previewing does not also trigger
        // whatever the UI bound to the same physical button.
        static bool ShouldEatScaleformKey(RE::UIMessage& a_message);

        static inline REL::Relocation<decltype(Thunk)>                   func_;
        static inline REL::Relocation<decltype(CanProcessGate)>          origCanProcess_;
        static inline REL::Relocation<decltype(ContainerProcessMessage)> origContainerPM_;
        static inline REL::Relocation<decltype(BarterProcessMessage)>    origBarterPM_;
        static inline REL::Relocation<decltype(InventoryProcessMessage)> origInventoryPM_;
        static inline REL::Relocation<decltype(ContainerAdvanceMovie)>   origContainerAdvance_;
        static inline REL::Relocation<decltype(BarterAdvanceMovie)>      origBarterAdvance_;
        static inline REL::Relocation<decltype(InventoryAdvanceMovie)>   origInventoryAdvance_;
        static inline std::atomic<RE::InventoryEntryData*>               hovered_{ nullptr };
        static inline bool                                                lastAutoMode_{ false };
        static inline bool                                                lastSuppressHoverModel_{ false };
        static inline bool                                                lastClearModelOnPreview_{
            kDefaultClearModelOnPreview };
    };

}  // namespace AP
