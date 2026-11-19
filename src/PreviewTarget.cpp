#include "PreviewTarget.h"

#include "PreviewSession.h"
#include "Settings.h"

namespace AP {

    namespace {
        // ContainerMenu::GetTargetRefHandle has been declared with either a raw
        // RefHandle or an ObjectRefHandle across CommonLibSSE-NG revisions;
        // these overloads accept both.
        RE::RefHandle ToRaw(RE::RefHandle a_handle) { return a_handle; }
        RE::RefHandle ToRaw(const RE::ObjectRefHandle& a_handle) { return a_handle.native_handle(); }
    }

    RE::Actor* PreviewTarget::Get() {
        if (const auto id = id_.load(std::memory_order_relaxed); id != 0) {
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                return actor;
            }
        }
        return RE::PlayerCharacter::GetSingleton();
    }

    RE::FormID PreviewTarget::GetID() { return id_.load(std::memory_order_relaxed); }

    bool PreviewTarget::IsPlayer() { return GetID() == 0; }

    void PreviewTarget::Resolve() {
        // Never retarget under a live preview.
        if (PreviewSession::GetSingleton().IsActive()) {
            return;
        }

        RE::FormID wanted = 0;
        const auto& cfg   = Settings::GetSingleton();
        auto*       ui    = RE::UI::GetSingleton();

        if (cfg.previewOnFollower && ui && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            const auto raw = ToRaw(RE::ContainerMenu::GetTargetRefHandle());
            auto       ref = RE::TESObjectREFR::LookupByHandle(raw);
            auto*      actor = ref ? ref->As<RE::Actor>() : nullptr;
            // Only a living follower whose body is actually loaded in the
            // world; corpses, chests and the player keep the default.
            if (actor && !actor->IsPlayerRef() && actor->IsPlayerTeammate() &&
                !actor->IsDead() && actor->Get3D(false)) {
                wanted = actor->GetFormID();
            }
        }

        const auto previous = id_.exchange(wanted);
        if (previous != wanted) {
            spdlog::info("PreviewTarget: now {:08X} ({}).", wanted,
                         wanted ? "follower" : "player");
        }
    }

    void PreviewTarget::Reset() {
        if (id_.exchange(0) != 0) {
            spdlog::info("PreviewTarget: reset to player.");
        }
    }

}  // namespace AP
