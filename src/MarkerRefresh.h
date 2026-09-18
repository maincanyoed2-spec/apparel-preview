#pragma once

#include <cstddef>
#include <optional>

namespace AP {

    // Converts an absolute item/data-provider index to the corresponding
    // visible renderer index. Invisible rows need no immediate refresh: their
    // renderer receives the updated data naturally when they scroll on-screen.
    [[nodiscard]] inline std::optional<std::size_t> VisibleRendererIndex(
        std::size_t a_itemIndex, std::size_t a_scrollPosition,
        std::size_t a_rendererCount) {
        if (a_rendererCount == 0 || a_itemIndex < a_scrollPosition) {
            return std::nullopt;
        }
        const auto rendererIndex = a_itemIndex - a_scrollPosition;
        if (rendererIndex >= a_rendererCount) {
            return std::nullopt;
        }
        return rendererIndex;
    }

    // Preserve mouse hover whenever the active UI exposes at least one changed
    // visible row. Manual mode retains a whole-list compatibility fallback
    // only when no row-local refresh path exists; automatic mode must never
    // invalidate under a stationary cursor.
    [[nodiscard]] inline bool ShouldInvalidateMarkerList(
        bool a_automatic, std::size_t a_changedRows,
        std::size_t a_refreshedRows) {
        return a_changedRows > 0 && a_refreshedRows == 0 && !a_automatic;
    }

}  // namespace AP
