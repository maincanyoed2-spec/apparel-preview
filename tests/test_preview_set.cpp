// Pure-logic tests. No engine, no RE:: types.
#include "FittingRoomSignal.h"
#include "ItemPreviewSuppression.h"
#include "MarkerRefresh.h"
#include "PreviewSet.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(expr)                                                     \
    do {                                                                \
        if (!(expr)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                               \
        }                                                               \
    } while (0)

int main() {
    using AP::PreviewSet;
    constexpr std::uint32_t kHead = 1u << 0;   // BipedObjectSlot masks are 1<<(slot-30)
    constexpr std::uint32_t kBody = 1u << 2;
    constexpr std::uint32_t kBodyAndHands = (1u << 2) | (1u << 3);
    // Variant keys as PreviewSession::VariantKey would produce them: 0 for a
    // plain row, distinct nonzero folds for enchanted / tempered rows.
    constexpr std::uint32_t kPlain    = 0;
    constexpr std::uint32_t kEnchanted = 0xA1B2C3D4;
    constexpr std::uint32_t kTempered  = 0x55667788;

    {  // toggle on, toggle off
        PreviewSet s;
        CHECK(s.Empty());
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kAdded);
        CHECK(!s.Empty());
        CHECK(s.Contains(0x100));
        CHECK(s.ContainsExact(0x100, kPlain));
        CHECK(s.UnionMask() == kHead);
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // non-overlapping pieces stack
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        CHECK(s.Contains(0x100) && s.Contains(0x200));
        CHECK(s.UnionMask() == (kHead | kBody));
        CHECK(s.Size() == 2);
    }
    {  // overlapping piece evicts what it overlaps (and only that)
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        s.Toggle(0x300, kPlain, kBodyAndHands);  // overlaps 0x200, not 0x100
        CHECK(!s.Contains(0x200));
        CHECK(s.Contains(0x100) && s.Contains(0x300));
        CHECK(s.UnionMask() == (kHead | kBodyAndHands));
    }
    {  // same identity toggled with a different mask still removes
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        CHECK(s.Toggle(0x100, kPlain, kBody) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // THE BUG THIS GUARDS: rows sharing a base form but differing in extra
       // data (plain vs enchanted vs tempered Steel Helmet) are distinct
       // identities. Toggling the second one SWAPS the preview (same slots ->
       // eviction); only the toggled row is row-exact "previewed".
        PreviewSet s;
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kAdded);
        CHECK(s.Toggle(0x100, kEnchanted, kHead) == PreviewSet::Change::kAdded);
        CHECK(s.Size() == 1);
        CHECK(!s.ContainsExact(0x100, kPlain));
        CHECK(s.ContainsExact(0x100, kEnchanted));
        CHECK(!s.ContainsExact(0x100, kTempered));
        CHECK(s.Contains(0x100));  // form-level stays true: same 3D is injected
        CHECK(s.Toggle(0x100, kEnchanted, kHead) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // a tempered row is likewise distinct from the plain row
        PreviewSet s;
        s.Toggle(0x100, kTempered, kHead);
        CHECK(s.ContainsExact(0x100, kTempered));
        CHECK(!s.ContainsExact(0x100, kPlain));
        CHECK(s.RemoveExact(0x100, kPlain) == false);  // wrong row: no-op
        CHECK(s.RemoveExact(0x100, kTempered) == true);
        CHECK(s.Empty());
    }
    {  // Remove() (container-changed path) drops any variant of the base form
       // and leaves other forms alone; removing an absent id is a no-op
        PreviewSet s;
        s.Toggle(0x100, kEnchanted, kHead);
        s.Toggle(0x200, kPlain, kBody);
        CHECK(s.Remove(0x999) == false);
        CHECK(s.Remove(0x100) == true);
        CHECK(s.Size() == 1);
        CHECK(s.Contains(0x200));
        CHECK(s.Remove(0x200) == true);
        CHECK(s.Empty());
    }
    {  // Clear
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        s.Clear();
        CHECK(s.Empty());
        CHECK(s.UnionMask() == 0);
    }
    {  // zero-mask items are rejected (no visible slots -> nothing to preview)
        PreviewSet s;
        CHECK(s.Toggle(0x100, kPlain, 0) == PreviewSet::Change::kRejected);
        CHECK(s.Empty());
    }
    {  // mouseover mode replaces the complete preview atomically
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        CHECK(s.Size() == 2);
        CHECK(s.ReplaceWith(0x300, kEnchanted, kBodyAndHands) ==
              PreviewSet::Change::kAdded);
        CHECK(s.Size() == 1);
        CHECK(s.ContainsExact(0x300, kEnchanted));
        CHECK(s.UnionMask() == kBodyAndHands);
        CHECK(s.ReplaceWith(0x300, kEnchanted, kBodyAndHands) ==
              PreviewSet::Change::kUnchanged);
        CHECK(s.ReplaceWith(0x400, kPlain, 0) == PreviewSet::Change::kRejected);
        CHECK(s.ContainsExact(0x300, kEnchanted));
    }
    {  // The user's blanket apparel-model setting owns the floating item
       // preview even when the selected row is not previewed on the player.
        CHECK(!AP::kDefaultClearModelOnPreview);
        CHECK(!AP::ItemPreviewSuppressionInput{}.clearModelOnPreview);
        CHECK(AP::ShouldSuppressItemPreview3D({
            .selectedIsApparel = true,
            .suppressHoverModel = true,
            .clearModelOnPreview = false,
            .selectedExactEntryIsPreviewed = false,
        }));
        CHECK(AP::ShouldSuppressItemPreview3D({
            .selectedIsApparel = true,
            .suppressHoverModel = false,
            .clearModelOnPreview = true,
            .selectedExactEntryIsPreviewed = true,
        }));
        CHECK(!AP::ShouldSuppressItemPreview3D({
            .selectedIsApparel = false,
            .suppressHoverModel = true,
            .clearModelOnPreview = true,
            .selectedExactEntryIsPreviewed = true,
        }));
        CHECK(!AP::ShouldSuppressItemPreview3D({
            .selectedIsApparel = true,
            .suppressHoverModel = false,
            .clearModelOnPreview = true,
            .selectedExactEntryIsPreviewed = false,
        }));
        CHECK(!AP::ShouldSuppressItemPreview3D({
            .selectedIsApparel = true,
            .suppressHoverModel = false,
            .clearModelOnPreview = false,
            .selectedExactEntryIsPreviewed = true,
        }));
    }
    {  // Publish only real owner edges: no release before an accepted acquire,
       // then one balanced release and no duplicate.
        AP::ItemPreviewSuppressionClaim claim;
        CHECK(!claim.NeedsPublish(false));
        CHECK(claim.NeedsPublish(true));
        claim.RecordAccepted(true);
        CHECK(!claim.NeedsPublish(true));
        CHECK(claim.NeedsPublish(false));
        claim.RecordAccepted(false);
        CHECK(!claim.NeedsPublish(false));
    }
    {  // Fitting Room's legacy message only clears. The stateful one also
       // suspends/resumes Apparel Preview while its overlay owns InventoryMenu.
        using AP::FittingRoomSignal;
        CHECK(AP::DecodeFittingRoomSignal(nullptr, 0) == FittingRoomSignal::kClearOnly);
        const std::uint8_t opened = 1;
        const std::uint8_t closed = 0;
        CHECK(AP::DecodeFittingRoomSignal(&opened, sizeof(opened)) ==
              FittingRoomSignal::kOpened);
        CHECK(AP::DecodeFittingRoomSignal(&closed, sizeof(closed)) ==
              FittingRoomSignal::kClosed);
        CHECK(AP::PreviewActivityAllowed(true, true, false));
        CHECK(!AP::PreviewActivityAllowed(true, true, true));
        CHECK(!AP::PreviewActivityAllowed(false, true, false));
    }
    {  // What we send Fitting Room: the frozen one-byte flag, and the slot mask
       // that travels beside it (OS-145).
        CHECK(AP::kPreviewStateMsg == 0x41505056u);  // 'APPV'
        CHECK(AP::kPreviewSlotsMsg == 0x4150534Du);  // 'APSM'
        CHECK(AP::kPreviewStateMsg != AP::kPreviewSlotsMsg);

        // ⚠ THE FLAG IS STILL EXACTLY ONE BYTE. Fitting Room 0.4.0 and earlier
        // read anything else as "no preview", which would switch their
        // worn-mask stand-down off and bring the headless player back (OS-141).
        // This pin is what stops a future session growing this payload.
        CHECK(sizeof(AP::EncodePreviewState(true)) == 1);
        CHECK(AP::EncodePreviewState(true) != 0);
        CHECK(AP::EncodePreviewState(false) == 0);

        // Little endian, four bytes, and the two ends agree byte for byte:
        // FR's ApparelPreviewWire.h reads these back in this order.
        CHECK(AP::kPreviewSlotsPayloadSize == 4);
        const auto body = AP::EncodePreviewSlots(kBody);  // 1u << 2
        CHECK(body.size() == 4);
        CHECK(body[0] == 0x04);
        CHECK(body[1] == 0x00);
        CHECK(body[2] == 0x00);
        CHECK(body[3] == 0x00);

        // Bit 31 is a real armour bit, so the top byte has to survive the trip
        // and nothing here may sign-extend.
        const auto high = AP::EncodePreviewSlots(0x80000004u);
        CHECK(high[0] == 0x04);
        CHECK(high[1] == 0x00);
        CHECK(high[2] == 0x00);
        CHECK(high[3] == 0x80);

        const auto none = AP::EncodePreviewSlots(0);
        CHECK(none[0] == 0 && none[1] == 0 && none[2] == 0 && none[3] == 0);

        // A multi-slot piece names every bit it covers, because FR skips its
        // dye per bit and a half-named garment would keep half its dye.
        PreviewSet s;
        CHECK(s.Toggle(0x100, kPlain, kHead | kBody) == PreviewSet::Change::kAdded);
        const auto both = AP::EncodePreviewSlots(s.UnionMask());
        CHECK(both[0] == (kHead | kBody));
    }
    {  // In-place eye refresh addresses visible renderers without invalidating
       // the whole list under the mouse.
        CHECK(AP::VisibleRendererIndex(5, 3, 6).value() == 2);
        CHECK(AP::VisibleRendererIndex(3, 3, 6).value() == 0);
        CHECK(!AP::VisibleRendererIndex(2, 3, 6).has_value());
        CHECK(!AP::VisibleRendererIndex(9, 3, 6).has_value());
        CHECK(!AP::VisibleRendererIndex(3, 3, 0).has_value());
    }
    {  // Manual preview must use the same row-local marker refresh whenever
       // the active UI exposes a visible renderer. Rebuilding the entire list
       // here drops mouse hover until the next click on some SkyUI overhauls.
        CHECK(!AP::ShouldInvalidateMarkerList(false, 2, 2));
        CHECK(!AP::ShouldInvalidateMarkerList(false, 1, 1));
        CHECK(AP::ShouldInvalidateMarkerList(false, 1, 0));
        CHECK(!AP::ShouldInvalidateMarkerList(true, 1, 0));
        CHECK(!AP::ShouldInvalidateMarkerList(false, 0, 0));
    }

    if (g_failures == 0) {
        std::printf("PreviewSetTests: all passed\n");
        return 0;
    }
    std::printf("PreviewSetTests: %d failure(s)\n", g_failures);
    return 1;
}
