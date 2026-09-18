#pragma once

namespace AP::MenuStudioApi {

    // Optional, additive Menu Studio export resolved at runtime. Every call is
    // a no-op when Menu Studio is absent or predates the export.
    [[nodiscard]] bool Available();

    // Publish Apparel Preview's one owner-scoped suppression claim. Repeated
    // edges are idempotent; false releases only a claim accepted from this DLL.
    // Returns true when Menu Studio owns the request, false when the caller must
    // retain its legacy Inventory3DManager behavior.
    bool SetItemPreviewSuppressed(bool a_suppressed);

}  // namespace AP::MenuStudioApi
