#include "PCH.h"

#include "SettingsUI.h"

#include "InputListener.h"  // the preview key, and re-resolving it on a rebind
#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"

#include <SimpleIni.h>  // CSimpleIniA, referenced by FUCK_API.h's INI callback typedefs

#include "FUCK_API.h"

namespace AP {

    namespace {
        bool g_capturingKb = false;

        struct KeyMap {
            ImGuiKey      imKey;
            std::uint32_t dik;
            const char*   name;
        };
        // DIK codes per DirectInput.
        //
        // ⚠⚠ IT WAS LETTERS AND SIX F-KEYS, AND EVERY OTHER KEY LOOKED BROKEN
        // (field 2026-08-16). A capture that only recognises part of the
        // keyboard cannot say so: the panel sat there reading "press a key"
        // while the tester pressed one, nothing happened, and the only honest
        // conclusion available to them was that the rebind does not work.
        // Digits, punctuation, the numpad, the navigation block and the arrows
        // are all ordinary rebind targets and all of them were silently dead.
        //
        // ⚠ WHAT IS STILL LEFT OUT, AND WHY IT IS DELIBERATE. F1/F5 and F9-F12
        // are quicksave/quickload and friends. Escape, Enter, Tab, Space and
        // Backspace drive the very menus this key is pressed inside, so binding
        // one would take the preview at the cost of leaving the container.
        constexpr KeyMap kKeyMap[] = {
            { ImGuiKey_A, 0x1E, "A" }, { ImGuiKey_B, 0x30, "B" },
            { ImGuiKey_C, 0x2E, "C" }, { ImGuiKey_D, 0x20, "D" },
            { ImGuiKey_E, 0x12, "E" }, { ImGuiKey_F, 0x21, "F" },
            { ImGuiKey_G, 0x22, "G" }, { ImGuiKey_H, 0x23, "H" },
            { ImGuiKey_I, 0x17, "I" }, { ImGuiKey_J, 0x24, "J" },
            { ImGuiKey_K, 0x25, "K" }, { ImGuiKey_L, 0x26, "L" },
            { ImGuiKey_M, 0x32, "M" }, { ImGuiKey_N, 0x31, "N" },
            { ImGuiKey_O, 0x18, "O" }, { ImGuiKey_P, 0x19, "P" },
            { ImGuiKey_Q, 0x10, "Q" }, { ImGuiKey_R, 0x13, "R" },
            { ImGuiKey_S, 0x1F, "S" }, { ImGuiKey_T, 0x14, "T" },
            { ImGuiKey_U, 0x16, "U" }, { ImGuiKey_V, 0x2F, "V" },
            { ImGuiKey_W, 0x11, "W" }, { ImGuiKey_X, 0x2D, "X" },
            { ImGuiKey_Y, 0x15, "Y" }, { ImGuiKey_Z, 0x2C, "Z" },
            { ImGuiKey_F2, 0x3C, "F2" }, { ImGuiKey_F3, 0x3D, "F3" },
            { ImGuiKey_F4, 0x3E, "F4" }, { ImGuiKey_F6, 0x40, "F6" },
            { ImGuiKey_F7, 0x41, "F7" }, { ImGuiKey_F8, 0x42, "F8" },
            { ImGuiKey_1, 0x02, "1" }, { ImGuiKey_2, 0x03, "2" },
            { ImGuiKey_3, 0x04, "3" }, { ImGuiKey_4, 0x05, "4" },
            { ImGuiKey_5, 0x06, "5" }, { ImGuiKey_6, 0x07, "6" },
            { ImGuiKey_7, 0x08, "7" }, { ImGuiKey_8, 0x09, "8" },
            { ImGuiKey_9, 0x0A, "9" }, { ImGuiKey_0, 0x0B, "0" },
            { ImGuiKey_Minus, 0x0C, "-" }, { ImGuiKey_Equal, 0x0D, "=" },
            { ImGuiKey_LeftBracket, 0x1A, "[" }, { ImGuiKey_RightBracket, 0x1B, "]" },
            { ImGuiKey_Semicolon, 0x27, ";" }, { ImGuiKey_Apostrophe, 0x28, "'" },
            { ImGuiKey_GraveAccent, 0x29, "`" }, { ImGuiKey_Backslash, 0x2B, "\\" },
            { ImGuiKey_Comma, 0x33, "," }, { ImGuiKey_Period, 0x34, "." },
            { ImGuiKey_Slash, 0x35, "/" },
            { ImGuiKey_Keypad7, 0x47, "Numpad 7" }, { ImGuiKey_Keypad8, 0x48, "Numpad 8" },
            { ImGuiKey_Keypad9, 0x49, "Numpad 9" },
            { ImGuiKey_KeypadSubtract, 0x4A, "Numpad -" },
            { ImGuiKey_Keypad4, 0x4B, "Numpad 4" }, { ImGuiKey_Keypad5, 0x4C, "Numpad 5" },
            { ImGuiKey_Keypad6, 0x4D, "Numpad 6" },
            { ImGuiKey_KeypadAdd, 0x4E, "Numpad +" },
            { ImGuiKey_Keypad1, 0x4F, "Numpad 1" }, { ImGuiKey_Keypad2, 0x50, "Numpad 2" },
            { ImGuiKey_Keypad3, 0x51, "Numpad 3" }, { ImGuiKey_Keypad0, 0x52, "Numpad 0" },
            { ImGuiKey_KeypadDecimal, 0x53, "Numpad ." },
            { ImGuiKey_KeypadMultiply, 0x37, "Numpad *" },
            { ImGuiKey_KeypadDivide, 0xB5, "Numpad /" },
            { ImGuiKey_Insert, 0xD2, "Insert" }, { ImGuiKey_Delete, 0xD3, "Delete" },
            { ImGuiKey_Home, 0xC7, "Home" }, { ImGuiKey_End, 0xCF, "End" },
            { ImGuiKey_PageUp, 0xC9, "Page Up" }, { ImGuiKey_PageDown, 0xD1, "Page Down" },
            { ImGuiKey_UpArrow, 0xC8, "Up arrow" }, { ImGuiKey_LeftArrow, 0xCB, "Left arrow" },
            { ImGuiKey_RightArrow, 0xCD, "Right arrow" },
            { ImGuiKey_DownArrow, 0xD0, "Down arrow" },
            { ImGuiKey_CapsLock, 0x3A, "Caps Lock" },
        };

        const KeyMap* PollCapturedKey() {
            for (const auto& entry : kKeyMap) {
                if (FUCK::IsKeyPressed(entry.imKey, false)) {
                    return &entry;
                }
            }
            return nullptr;
        }

        // What to CALL a binding, which is not the same question as what this
        // panel can capture: the value can also arrive from the INI or from the
        // game's own Item Zoom mapping, and those are not limited to the table
        // above. An unknown code keeps the raw hex rather than being renamed to
        // something plausible.
        std::string NameForDIK(std::uint32_t a_dik) {
            for (const auto& entry : kKeyMap) {
                if (entry.dik == a_dik) {
                    return entry.name;
                }
            }
            return fmt::format("DIK 0x{:X}", a_dik);
        }

        // Tooltip for the item just drawn (shown only on hover).
        void Tip(const char* a_desc) {
            if (FUCK::IsItemHovered()) {
                FUCK::SetTooltip(a_desc);
            }
        }

        void DrawPanel() {
            auto& cfg   = Settings::GetSingleton();
            bool  dirty = false;

            FUCK::TextDisabled("Preview apparel on your character from the "
                               "container, barter and inventory menus.");

            // Master switch. Unchecking it live is a real kill: drop any active
            // preview and disarm now rather than waiting for the menu to close.
            if (FUCK::Checkbox("Enable Apparel Preview", &cfg.enabled)) {
                dirty = true;
                if (!cfg.enabled) {
                    MenuWatcher::ForceReset("disabled via settings");
                }
            }
            Tip("Master switch. Unchecking it drops any active preview right away");

            FUCK::SeparatorText("Where to preview");
            dirty |= FUCK::Checkbox("Container menu (looting)", &cfg.enableContainer);
            Tip("Show previews while looting chests, corpses and containers");
            dirty |= FUCK::Checkbox("Barter menu (shopping)", &cfg.enableBarter);
            Tip("Show previews while trading with a merchant");
            dirty |= FUCK::Checkbox("Inventory menu (your own items)", &cfg.enableInventory);
            Tip("Show previews in your own inventory");

            FUCK::SeparatorText("Options");
            dirty |= FUCK::Checkbox("Preview highlighted apparel automatically",
                                    &cfg.previewOnHover);
            Tip("Preview one apparel item at a time as you move the highlight. "
                "No key press needed");
            dirty |= FUCK::Checkbox("Allow shield preview", &cfg.allowShields);
            Tip("Include shields. Some sit oddly on the arm; turn this off to skip them");
            dirty |= FUCK::Checkbox("Notify when a piece cannot fit your body",
                                    &cfg.notifyUnfitPreview);
            Tip("Show a message when a piece has no armour for your race or body. "
                "With this off the preview is declined silently");

            FUCK::SeparatorText("Inspect mode and camera");
            dirty |= FUCK::Checkbox("Turn off Skyrim's inspect mode for apparel",
                                    &cfg.blockApparelInspect);
            // ⚠ THIS CHECKBOX IS THE ANSWER TO "can we get 3D item inspect for
            // gear" (field 2026-08-16, asked as though no setting existed).
            // It does exist, inverted: this is what TAKES inspect away, so
            // unchecking it hands it back. The second sentence is the part the
            // question really needed, since one control cannot do both jobs.
            Tip("The Item Zoom control toggles the worn preview instead of "
                "opening Skyrim's inspect mode on apparel.\n"
                "Uncheck it to get Skyrim's 3D inspect back for armour and "
                "clothing, then bind the preview to its own key below");
            dirty |= FUCK::Checkbox("Block camera POV switching while a preview menu is open",
                                    &cfg.blockPOVWhileArmed);
            Tip("Keeps the POV key or right-stick click from flipping the camera "
                "while a preview menu is open. Only matters with unpaused-menu "
                "mods like Skyrim Souls RE");

            FUCK::SeparatorText("Floating item preview");
            dirty |= FUCK::Checkbox("Hide while a worn preview is active",
                                    &cfg.clearModelOnPreview);
            Tip("Turn this on only if Skyrim's floating item preview sits over "
                "your character in your UI layout");
            dirty |= FUCK::Checkbox("Always hide for apparel",
                                    &cfg.suppressHoverModel);
            Tip("Hides the floating item preview whenever an apparel row is selected. "
                "The item card and the worn preview still show");

            FUCK::SeparatorText("Preview key");
            bool keyChanged = false;
            if (cfg.previewKeyDIK == 0) {
                FUCK::Text("Following the game's 'Item Zoom' binding (%s)",
                           NameForDIK(InputListener::KeyboardKey()).c_str());
            } else {
                FUCK::Text("Bound to %s", NameForDIK(cfg.previewKeyDIK).c_str());
            }
            FUCK::SameLine(0.0f, 8.0f);
            if (!g_capturingKb) {
                if (FUCK::Button("Rebind##kb")) {
                    g_capturingKb = true;
                }
            } else {
                FUCK::Text("Press a key");
                if (const auto* key = PollCapturedKey()) {
                    cfg.previewKeyDIK = key->dik;
                    g_capturingKb     = false;
                    dirty             = true;
                    keyChanged        = true;
                    spdlog::info("SettingsUI: preview key rebound to {} (DIK 0x{:X}).",
                                 key->name, key->dik);
                }
            }
            if (cfg.previewKeyDIK != 0) {
                FUCK::SameLine(0.0f, 8.0f);
                if (FUCK::Button("Reset to Item Zoom##kb")) {
                    cfg.previewKeyDIK = 0;
                    dirty             = true;
                    keyChanged        = true;
                }
            }
            Tip("Toggles a worn preview on the highlighted item. Not needed when "
                "automatic preview is on");
            // ⚠⚠ THE REBIND USED TO CHANGE NOTHING UNTIL SKYRIM WAS RESTARTED,
            // and it was reported as the binding simply not working (field
            // 2026-08-16). InputListener resolves the key ONCE, at kDataLoaded,
            // and that message fires once per PROCESS: the old tooltip's "takes
            // effect after the next game load" was not even true, since loading
            // a save does not deliver it again. The panel wrote the setting, the
            // listener kept the key it resolved at launch, and nothing anywhere
            // said so.
            //
            // ⚠ POSTED TO THE MAIN THREAD, NOT CALLED HERE. This panel draws on
            // the render thread, and ResolveBindings reads ControlMap and
            // UserEvents to fall back to the game's own Item Zoom mapping.
            if (keyChanged) {
                if (auto* const task = SKSE::GetTaskInterface()) {
                    task->AddTask([] { InputListener::ResolveBindings(); });
                }
            }

            FUCK::SeparatorText("Actions");
            if (FUCK::Button("Clear active worn preview")) {
                PreviewSession::GetSingleton().Clear();
            }
            Tip("Remove any worn preview showing on your character right now");

            if (dirty) {
                cfg.Save();
            }
        }

        // FLICK sidebar entry: the user opens FUCK (hotkey / controller menu) and
        // picks "Apparel Preview".
        class SettingsTool : public FUCK::ITool {
        public:
            const char* Name() const override { return "Apparel Preview"; }
            void        Draw() override { DrawPanel(); }
        };

        SettingsTool g_settingsTool;  // process-lifetime; registered pointer stays valid
    }

    void SettingsUI::Register() {
        // Soft dependency: without FUCK.dll the mod stays INI-only with one log
        // line. The name passed here is what FLICK shows in its sidebar / the
        // registered-plugin panel.
        if (!FUCK::Connect("Apparel Preview")) {
            spdlog::info("SettingsUI: FUCK / FLICK not present; INI-only mode.");
            return;
        }
        FUCK::RegisterTool(&g_settingsTool);
        spdlog::info("SettingsUI: settings registered as a FLICK (FUCK) sidebar tool.");
    }

}  // namespace AP
