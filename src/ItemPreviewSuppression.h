#pragma once

namespace AP {

    inline constexpr bool kDefaultClearModelOnPreview = false;

    struct ItemPreviewSuppressionInput {
        bool selectedIsApparel{ false };
        bool suppressHoverModel{ false };
        bool clearModelOnPreview{ kDefaultClearModelOnPreview };
        bool selectedExactEntryIsPreviewed{ false };
    };

    [[nodiscard]] constexpr bool ShouldSuppressItemPreview3D(
        const ItemPreviewSuppressionInput& a_input) noexcept {
        return a_input.selectedIsApparel &&
               (a_input.suppressHoverModel ||
                (a_input.clearModelOnPreview && a_input.selectedExactEntryIsPreviewed));
    }

    class ItemPreviewSuppressionClaim {
    public:
        [[nodiscard]] constexpr bool NeedsPublish(bool a_desired) const noexcept {
            return claimed_ != a_desired;
        }

        constexpr void RecordAccepted(bool a_claimed) noexcept { claimed_ = a_claimed; }

        [[nodiscard]] constexpr bool Claimed() const noexcept { return claimed_; }

    private:
        bool claimed_{ false };
    };

}  // namespace AP
