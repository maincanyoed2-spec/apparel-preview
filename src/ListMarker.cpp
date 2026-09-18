#include "ListMarker.h"

#include "DIIIIntegration.h"
#include "MarkerRefresh.h"
#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"

namespace AP {

    namespace {
        constexpr const char* kIconLabel = "PreviewIcon";

        // Add/remove our icon in the entry's _DIIIIcons array - the exact
        // member DIII's formatName hook reads (verified in its source). This
        // avoids an engine list rebuild entirely: no stutter, no hover reset,
        // no re-stamping of every entry by every inventory mod.
        bool SyncDIIIIcon(RE::GFxMovieView* a_view, RE::GFxValue& a_entry, bool a_marked) {
            RE::GFxValue arr;
            const bool hasArr = a_entry.GetMember("_DIIIIcons", &arr) && arr.IsArray();

            std::int32_t found = -1;
            if (hasArr) {
                const auto n = arr.GetArraySize();
                for (std::uint32_t i = 0; i < n; ++i) {
                    RE::GFxValue elem, label;
                    if (arr.GetElement(i, &elem) && elem.IsObject() &&
                        elem.GetMember("label", &label) && label.IsString() &&
                        std::string_view{ label.GetString() } == kIconLabel) {
                        found = static_cast<std::int32_t>(i);
                        break;
                    }
                }
            }

            if (a_marked && found < 0) {
                if (!hasArr) {
                    a_view->CreateArray(&arr);
                }
                RE::GFxValue icon;
                a_view->CreateObject(&icon);
                icon.SetMember("label", RE::GFxValue{ kIconLabel });
                arr.PushBack(icon);
                a_entry.SetMember("_DIIIIcons", arr);
                return true;
            }
            if (!a_marked && found >= 0) {
                arr.RemoveElements(static_cast<std::uint32_t>(found), 1);
                a_entry.SetMember("_DIIIIcons", arr);
                return true;
            }
            return false;
        }

        enum class RefreshRoute {
            kNone,
            kSkyUI,
            kCLIK
        };

        RefreshRoute RefreshRenderer(RE::ItemList* a_list, std::size_t a_itemIndex,
                                     RE::GFxValue& a_entry) {
            RE::GFxValue renderer;
            RE::GFxValue clipIndex;

            // SkyUI's custom ScrollingList stores the currently assigned
            // visible clip index on each entry. Re-run the same row-local
            // update used by SkyUI itself when selection changes:
            //   getClipByIndex(entry.clipIndex).setEntry(entry, listState)
            // This is also the list implementation used by Edge UI. It
            // updates DIII's formatted icon without invalidating the list or
            // disturbing mouse selection.
            if (a_entry.GetMember("clipIndex", &clipIndex) && clipIndex.IsNumber() &&
                a_list->root.Invoke("getClipByIndex", &renderer, &clipIndex, 1) &&
                (renderer.IsObject() || renderer.IsDisplayObject())) {
                RE::GFxValue listState;
                if (a_list->root.GetMember("listState", &listState) &&
                    (listState.IsObject() || listState.IsDisplayObject())) {
                    std::array args{ a_entry, listState };
                    if (renderer.Invoke("setEntry", nullptr, args.data(), args.size())) {
                        return RefreshRoute::kSkyUI;
                    }
                }
            }

            // CLIK ScrollingList::getRendererAt takes an absolute
            // data-provider index and returns null for an off-screen row.
            // Although it is declared private in AS2, it remains an invokable
            // member on the GFx object.
            RE::GFxValue indexArg{ static_cast<double>(a_itemIndex) };
            bool found =
                a_list->root.Invoke("getRendererAt", &renderer, &indexArg, 1) &&
                (renderer.IsObject() || renderer.IsDisplayObject());

            // UI overhauls may omit the prototype method while retaining
            // CLIK's renderer array. Fall back to the same index conversion
            // ScrollingList uses internally.
            if (!found) {
                RE::GFxValue renderers;
                RE::GFxValue scroll;
                if ((!a_list->root.GetMember("renderers", &renderers) || !renderers.IsArray()) &&
                    (!a_list->root.GetMember("_renderers", &renderers) ||
                     !renderers.IsArray())) {
                    return RefreshRoute::kNone;
                }
                if (!a_list->root.GetMember("scrollPosition", &scroll) ||
                    !scroll.IsNumber()) {
                    return RefreshRoute::kNone;
                }
                const auto rendererIndex =
                    VisibleRendererIndex(a_itemIndex, scroll.GetUInt(), renderers.GetArraySize());
                if (!rendererIndex ||
                    !renderers.GetElement(static_cast<std::uint32_t>(*rendererIndex), &renderer) ||
                    (!renderer.IsObject() && !renderer.IsDisplayObject())) {
                    return RefreshRoute::kNone;
                }
            }

            // Reformat only this row. DIII's InventoryListEntry hook reads the
            // updated _DIIIIcons array from setData; no data-provider rebuild,
            // selection reset, or mouse hit-test churn occurs.
            return renderer.Invoke("setData", nullptr, &a_entry, 1)
                       ? RefreshRoute::kCLIK
                       : RefreshRoute::kNone;
        }
    }

    void ListMarker::Apply() {
        if (!MenuWatcher::IsArmed()) {
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
        if (list) {
            ApplyToList(list);
        }
    }

    void ListMarker::ApplyToList(RE::ItemList* a_list) {
        // The gold DIII eye icon is the only previewed-row marker. Without
        // Dynamic Inventory Icon Injector active there is nothing to draw (the
        // " [P]" suffix and gold-tint fallbacks were removed by design).
        if (!DIIIIntegration::IsActive()) {
            return;
        }
        auto* view = a_list->view.get();
        if (!view) {
            return;
        }
        auto& session = PreviewSession::GetSingleton();
        const bool automatic = Settings::GetSingleton().previewOnHover;

        bool        changed        = false;
        std::size_t changedRows    = 0;
        std::size_t skyUIRefreshed = 0;
        std::size_t clikRefreshed  = 0;
        for (std::uint32_t i = 0; i < a_list->items.size(); ++i) {
            auto* item = a_list->items[i];
            if (!item) {
                continue;
            }
            auto* entry = item->data.objDesc;
            auto* obj   = entry ? entry->GetObject() : nullptr;
            if (!obj) {
                continue;
            }
            // Row-exact: several rows can share a base form (plain, enchanted,
            // tempered); only the row actually toggled gets the eye.
            const bool marked = session.IsPreviewedEntry(
                obj->GetFormID(), PreviewSession::VariantKey(entry));
            if (SyncDIIIIcon(view, item->obj, marked)) {
                changed = true;
                ++changedRows;
                switch (RefreshRenderer(a_list, i, item->obj)) {
                case RefreshRoute::kSkyUI:
                    ++skyUIRefreshed;
                    break;
                case RefreshRoute::kCLIK:
                    ++clikRefreshed;
                    break;
                case RefreshRoute::kNone:
                    break;
                }
            }
        }

        if (changed) {
            const auto refreshedRows = skyUIRefreshed + clikRefreshed;
            if (ShouldInvalidateMarkerList(automatic, changedRows, refreshedRows)) {
                // Unknown UI implementation: retain the broad manual-mode
                // fallback so deliberate previews still gain/lose their eye.
                a_list->root.Invoke("InvalidateData", nullptr, nullptr, 0);
                spdlog::debug("list markers synced (full-list compatibility fallback)");
            } else {
                spdlog::debug(
                    "list markers synced in place ({} mode, {} SkyUI + {} CLIK visible "
                    "row(s))",
                    automatic ? "automatic" : "manual", skyUIRefreshed, clikRefreshed);
            }
        }
    }

}  // namespace AP
