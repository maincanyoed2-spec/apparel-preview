#pragma once

// ⚠ NO PCH.h, NO SKSE, NO RE. PreviewSetTests compiles this header in a
// pure-logic target with no engine dependencies (see CMakeLists), so anything
// here that reached for SKSE:: would break that build rather than this one.
// The outbound Dispatch lives in PreviewSession.cpp for exactly that reason;
// what stays here is the part worth testing - the wire format itself.
#include <array>
#include <cstddef>
#include <cstdint>

// The whole cross-mod contract with Fitting Room (same studio), both
// directions, mirrored by FR's src/ApparelPreviewSignal.h. Change BOTH or
// neither.
namespace AP {

    // Fitting Room historically sent CLRP without data, which means "clear
    // once". Newer builds attach one byte so Apparel Preview can also remain
    // suspended for the lifetime of the editor overlay.
    enum class FittingRoomSignal {
        kClearOnly,
        kOpened,
        kClosed
    };

    [[nodiscard]] inline FittingRoomSignal DecodeFittingRoomSignal(
        const void* a_data, std::size_t a_length) {
        if (!a_data || a_length != sizeof(std::uint8_t)) {
            return FittingRoomSignal::kClearOnly;
        }
        return *static_cast<const std::uint8_t*>(a_data) != 0 ?
                   FittingRoomSignal::kOpened :
                   FittingRoomSignal::kClosed;
    }

    [[nodiscard]] inline bool PreviewActivityAllowed(bool a_armed,
                                                     bool a_supportedMenuOpen,
                                                     bool a_fittingRoomOpen) {
        return a_armed && a_supportedMenuOpen && !a_fittingRoomOpen;
    }

    // ---- outbound: Apparel Preview -> Fitting Room ------------------------
    //
    //   message type 'APPV', receiver plugin "FittingRoom"
    //   one byte, nonzero = a preview is on the player right now
    //   one byte, zero    = it is not
    //
    // ⚠ WE ARE THE OUTER SHIM ON GetWornMask. Fitting Room hooks it too and we
    // hook after it, so the engine calls us, we call FR, and we union our
    // preview coverage onto FR's already-styled answer. Both answers are
    // honest and the union is still wrong: the consumer reads slot 30 to cull
    // the FaceGen head node, FR sets that bit for a style whose geometry we
    // then replace with the previewed piece, and the player renders headless
    // (field 2026-08-04, a slot-30 style plus a previewed helmet).
    //
    // No mask records whose geometry is actually on the biped, so FR cannot
    // subtract its way out of it. Telling FR when a preview is live lets it
    // hand us the vanilla mask instead, and we union onto reality. FR's
    // src/ApparelPreviewSignal.h carries the same note and the log lines.
    //
    // ⚠ SEND THIS BEFORE THE REFRESH, never after: the refresh is what makes
    // the engine ask for the mask, and FR decides its answer from this flag.
    // PreviewSession::PublishAndRefresh is the only sender and enforces that
    // order; the Dispatch itself is there because this header stays engine-free.
    inline constexpr std::uint32_t kPreviewStateMsg = 'APPV';

    // One byte on the wire, mirroring the shape FR already sends us.
    //
    // ⚠ THIS SHAPE IS FROZEN. Fitting Room reads anything that is not EXACTLY
    // one byte as "no preview", and that rule is baked into every FR already in
    // the wild, 0.4.0 included. Widening this payload therefore does not
    // "require both mods to ship together" - it BREAKS the older half, silently
    // and in the field: a newer Apparel Preview would switch FR's worn-mask
    // stand-down back off, and the player would render headless again under a
    // previewed helmet with a slot-30 style (OS-141, fixed and shipped). The
    // slot mask travels in its own message below for exactly that reason.
    [[nodiscard]] inline std::uint8_t EncodePreviewState(bool a_previewActive) {
        return a_previewActive ? std::uint8_t{ 1 } : std::uint8_t{ 0 };
    }

    // ---- outbound: which slots the preview occupies ------------------------
    //
    //   message type 'APSM', receiver plugin "FittingRoom"
    //   four bytes, little-endian uint32 = the biped object bits this preview
    //   is standing on, in BGSBipedObjectForm::GetSlotMask space (bit N = biped
    //   object N = editor slot N + 30), which is the space PreviewSet already
    //   stores and unions.
    //
    // ⚠ WHY FITTING ROOM NEEDS THE SLOTS AND NOT JUST THE FLAG (OS-145). The
    // worn mask was half the problem. FR's dye walk paints by CHANNEL INDEX
    // within a slot, and a preview replaces the geometry on that slot without
    // changing anything the walk can observe, so the channel that coloured the
    // player's own cuirass colours whatever we just hung there. The user reads
    // that as Fitting Room dyeing an item they never dyed.
    //
    // FR cannot fix it from the flag alone: standing its whole dye walk down
    // for the duration of a hover would strip the dye off everything else the
    // character is wearing, which is a worse frame than the bug. It needs to
    // skip exactly the slots we are standing on, so we have to name them.
    //
    // ⚠ SEND THIS IMMEDIATELY BEFORE THE MATCHING 'APPV', never after. FR packs
    // the two into one word and the flag is what arms the skip, so the mask has
    // to be in place before the flag says the preview is live. Both then land
    // before the refresh, for the reason 'APPV' already documents.
    //
    // ⚠ AN OLDER FITTING ROOM IGNORES THIS ENTIRELY and keeps working. Its
    // listener tests the message type and drops what it does not know, so it
    // sees the unchanged 'APPV' and behaves exactly as it does today. A newer
    // FR paired with an Apparel Preview too old to send this leaves the mask
    // unknown and also behaves exactly as it does today. Neither direction
    // regresses; only the pair gets the fix.
    inline constexpr std::uint32_t kPreviewSlotsMsg = 'APSM';

    inline constexpr std::size_t kPreviewSlotsPayloadSize = 4;

    // Little endian is STATED, not inherited from the host: the bytes are laid
    // out here and read back byte by byte on FR's side, so neither end depends
    // on how the other was compiled or on the payload's alignment.
    [[nodiscard]] inline std::array<std::uint8_t, kPreviewSlotsPayloadSize> EncodePreviewSlots(
        std::uint32_t a_slotMask) {
        return { static_cast<std::uint8_t>(a_slotMask & 0xFF),
                 static_cast<std::uint8_t>((a_slotMask >> 8) & 0xFF),
                 static_cast<std::uint8_t>((a_slotMask >> 16) & 0xFF),
                 static_cast<std::uint8_t>((a_slotMask >> 24) & 0xFF) };
    }

}  // namespace AP
