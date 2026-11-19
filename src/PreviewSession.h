#pragma once
#include "PCH.h"

#include "PreviewSet.h"
#include "REAugments.h"

namespace AP {

    // All preview state + the refresh that makes it visible. Thread rules:
    // Toggle/Clear marshal to the game thread via the SKSE task interface;
    // the Should*/Visit* readers are called from inside the engine's skinning
    // pass (game thread) and take the same lock.
    class PreviewSession {
    public:
        static PreviewSession& GetSingleton();

        void Toggle(RE::TESObjectARMO* a_armor, std::uint32_t a_variantKey);  // add/remove/swap + refresh
        // Mouseover mode: show exactly this row, replacing the previous
        // automatic preview in one rebuild. Passing a rejected candidate
        // (unfitted, worn, hidden headgear, disabled shield) restores reality.
        void PreviewOnly(RE::TESObjectARMO* a_armor, std::uint32_t a_variantKey);
        void Clear();                                // revert to real worn look + refresh
        void OnPlayerAcquired(RE::FormID a_formID);  // container-changed path
        void OnPlayerEquipped(RE::FormID a_formID);  // equip-event path (inventory menu)

        // Row discriminator for an inventory entry. Several list rows can
        // share one base ARMO - they differ only in extra data (enchantment,
        // temper, ownership, custom name). Folds those into one key; 0 means
        // a plain row. Rows that are genuinely identical share a key, which
        // is correct: they are the same item.
        [[nodiscard]] static std::uint32_t VariantKey(RE::InventoryEntryData* a_entry);

        // ---- read side, used by BipedHooks (game thread, during rebuild) ----
        [[nodiscard]] bool IsActive() const;
        // Row-exact (form + variant) - list markers, the DIII condition and
        // the hover-model clear use this so only the toggled row counts.
        [[nodiscard]] bool IsPreviewedEntry(RE::FormID a_formID, std::uint32_t a_variantKey) const;
        [[nodiscard]] std::uint32_t PreviewMask() const;
        void VisitPreviewArmors(const std::function<void(RE::TESObjectARMO*)>& a_fn) const;

        // What PublishAndRefresh tells Fitting Room, read under ONE lock.
        // ⚠ NOT IsActive() FOLLOWED BY PreviewMask(). Those take the lock
        // twice, so a set that changed in between would be published as a live
        // preview carrying the previous hover's slots - and FR skips its dye on
        // exactly the bits this names, so the mismatch would exempt the wrong
        // garment. The pair is only useful if it is a pair.
        struct PublishState {
            bool          active{ false };
            std::uint32_t slotMask{ 0 };
        };
        [[nodiscard]] PublishState StateToPublish() const;

    private:
        void ToggleImpl(RE::FormID a_formID, std::uint32_t a_variantKey, std::uint32_t a_mask,
                        const std::string& a_name);
        void PreviewOnlyImpl(RE::FormID a_formID, std::uint32_t a_variantKey,
                             std::uint32_t a_mask, const std::string& a_name);
        void RemoveFormAndRefresh(RE::FormID a_formID, const char* a_reason);

        // Tell Fitting Room whether a preview is live, then refresh.
        // ⚠ EVERY refresh that can change what sits on the biped goes through
        // here, and the order inside is load-bearing - the refresh is what
        // makes the engine ask for the worn mask, and FR answers it from this
        // flag. Routing all of them through one function is the point: a new
        // refresh site that called REAug::RefreshPlayer directly would leave
        // FR answering for the previous state. See FittingRoomSignal.h.
        // Static so the task-interface lambdas can reach it without capturing.
        // a_actorID: the actor to rebuild (0 = player). Callers that run after
        // the menu closed capture PreviewTarget::GetID() BEFORE the target is
        // reset, so the revert refresh still reaches the follower.
        static void PublishAndRefresh(bool a_sceneKick, RE::FormID a_actorID);

        mutable std::mutex lock_;
        PreviewSet         set_;
        // Coalesces rapid mouseover changes. Every new automatic request gets
        // a generation; manual toggles, clears and menu closes invalidate any
        // queued request that has not reached the game thread yet.
        std::atomic<std::uint64_t> automaticGeneration_{ 0 };
    };

}  // namespace AP
