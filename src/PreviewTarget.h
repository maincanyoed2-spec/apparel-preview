#pragma once
#include "PCH.h"

namespace AP {

    // Which actor the preview is drawn on.
    //
    // Default is the player. While a ContainerMenu is open on a FOLLOWER
    // (the "Trade" / "Let's trade" screen, or any teammate's inventory), the
    // target becomes that follower instead, so a preview made from either
    // inventory list shows up on the follower rather than on the player.
    //
    // The target is stored as a FormID (0 = player) so hooks running inside
    // the engine's skinning pass can read it with one atomic load.
    class PreviewTarget {
    public:
        // Never null while a game is loaded: falls back to the player.
        [[nodiscard]] static RE::Actor*  Get();
        [[nodiscard]] static RE::FormID  GetID();  // 0 = player
        [[nodiscard]] static bool        IsPlayer();

        // Pick the target for the menu that is open right now. Idempotent, and
        // a no-op while a preview is live (the target must not change under an
        // active preview, or the old actor would keep the injected armor).
        static void Resolve();

        // Back to the player. Callers must have already cleared the session
        // (and captured GetID() for the revert refresh) before calling this.
        static void Reset();

    private:
        static inline std::atomic<RE::FormID> id_{ 0 };
    };

}  // namespace AP
