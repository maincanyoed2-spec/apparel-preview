#!/usr/bin/env bash
# Build the Apparel Preview FOMOD installer zip (the release artifact).
#
# Apparel Preview is one DLL that loads on both SE 1.5.97 and AE 1.6.317+, so
# there is no SE/AE file choice and no options; everything installs together and
# the FOMOD is a branded page with a short explanation and an endorse reminder.
# For a SINGLE-runtime mod that needs different SE and AE DLLs, add a step with a
# type="SelectExactlyOne" group ("Skyrim SE 1.5.97" / "Skyrim AE 1.6.x"), each
# plugin installing its own DLL folder; the rest of this layout is reusable.
#
# Package layout (zip root): fomod/{info.xml,ModuleConfig.xml}, Images/, core/
# (game files, installed to Data), + LICENSE and a short README.txt at the root
# (NOT installed). Docs single-source-of-truth is GitHub; the download carries
# only LICENSE (GPL) + a README.txt pointer, no CHANGELOG/KNOWN-ISSUES/THIRD-PARTY.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(sed -n 's/^project(ApparelPreview VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
DLL="$ROOT/build/release/ApparelPreview.dll"
STAGE="$ROOT/release/fomod-stage"
ZIP="$ROOT/release/ApparelPreview-$VER.zip"

[ -f "$DLL" ] || { echo "no DLL at $DLL - build first"; exit 1; }

# ⚠⚠ THE INSTALLER COPY IS UNISEX. The player's character can be anyone, so no
# string in ModuleConfig.xml refers to them as "her" or "she". One sentence about
# interface mods carried it in all three installers at once (user, 2026-08-28);
# two were fixed by hand and the third was missed, so it is checked here instead
# of remembered. Ahead of the rm below, so a refusal leaves the old zip alone.
#
# ⚠ ONLY ModuleConfig.xml. nexus/DESCRIPTION.bbcode legitimately thanks a real
# person as "her" and must not be swept up in this.
if grep -qiE '\b(her|hers|she|herself)\b' "$ROOT/fomod/ModuleConfig.xml"; then
    echo "fomod/ModuleConfig.xml calls the player's character 'her'. The installer copy is unisex:"
    grep -niE '\b(her|hers|she|herself)\b' "$ROOT/fomod/ModuleConfig.xml"
    echo "Use 'your character', or they/them."
    exit 1
fi

rm -rf "$STAGE" "$ZIP"

# FOMOD metadata + banner (ModuleConfig references Images\apparelpreview.png).
mkdir -p "$STAGE/fomod" "$STAGE/Images"
cp "$ROOT/fomod/info.xml" "$ROOT/fomod/ModuleConfig.xml" "$STAGE/fomod/"
cp "$ROOT/fomod/banner.png" "$STAGE/Images/apparelpreview.png"

# core: the game files that install to Data (DLL + the eye-icon SWF + the DIII
# rule, harmless without DIII). No docs in core - see below.
mkdir -p "$STAGE/core/SKSE/Plugins"
cp "$DLL" "$STAGE/core/SKSE/Plugins/"
cp -r "$ROOT/dist/SKSE/Plugins/DIII" "$STAGE/core/SKSE/Plugins/DIII"
cp -r "$ROOT/dist/Interface" "$STAGE/core/Interface"
# ---- starting settings, one file per answer --------------------------------
#
# Apparel Preview shipped no INI at all until 1.0.0: the plugin wrote its own on
# first run and the installer asked nothing. That is fine for a setting you
# discover later and wrong for how you PREVIEW, which is a control-scheme
# preference a player has an opinion about before they have pressed anything.
#
# ⚠⚠ STARTING SETTINGS AND NOTHING ELSE. Both flavours install the same files;
# the only difference is one key. The plugin fills in every key this template
# does not carry on first run, so a partial file here cannot strand a setting.
#
# ⚠ BOTH FLAVOURS GO THROUGH ONE sed, INCLUDING THE ONE THAT CHANGES NOTHING.
# The template is CRLF, `cp` keeps CRLF and `sed -i` writes LF, so mixing the
# two makes the flavours differ on every line while holding identical settings.
# The sibling repo's guard caught exactly that and refused to package.
mkdir -p "$STAGE/settings/on-key" "$STAGE/settings/on-hover"
TPL="$ROOT/dist/optional/settings/ApparelPreview.ini"
grep -q '^bPreviewOnHover = false' "$TPL" ||
    { echo "settings template no longer ships bPreviewOnHover = false"; exit 1; }
sed -e 's/^$/^/; s/^\^$//' "$TPL" > "$STAGE/settings/on-key/ApparelPreview.ini"
sed -e 's/^bPreviewOnHover = false/bPreviewOnHover = true/' \
    "$TPL" > "$STAGE/settings/on-hover/ApparelPreview.ini"
grep -q '^bPreviewOnHover = true' "$STAGE/settings/on-hover/ApparelPreview.ini" ||
    { echo "on-hover flavour: bPreviewOnHover substitution failed"; exit 1; }
grep -q '^bPreviewOnHover = false' "$STAGE/settings/on-key/ApparelPreview.ini" ||
    { echo "on-key flavour: lost the shipped default"; exit 1; }
DIFFLINES="$(diff "$STAGE/settings/on-key/ApparelPreview.ini" \
                  "$STAGE/settings/on-hover/ApparelPreview.ini" | grep -c '^[<>]' || true)"
[ "$DIFFLINES" = "2" ] ||
    { echo "the two flavours differ by $DIFFLINES lines, expected 2"; exit 1; }

# Docs at the ARCHIVE ROOT (not installed to Data). Single-source-of-truth is
# GitHub: ship LICENSE (GPL requires it in the download) + a short README.txt
# pointer only. Full README/CHANGELOG/KNOWN-ISSUES/THIRD-PARTY live on GitHub.
cp "$ROOT/LICENSE" "$STAGE/"
cat > "$STAGE/README.txt" <<'EOF'
Apparel Preview
See armor, clothing and jewelry on your character before you equip it,
right from the container, barter and inventory menus.

Documentation, changelog, source code and issue tracker:
  https://github.com/maartenharms/apparel-preview

Licensed under GPL-3.0 (see LICENSE).
EOF

(cd "$STAGE" && powershell -NoProfile -Command \
    "Compress-Archive -Path * -DestinationPath '$(cygpath -w "$ZIP")' -Force")

echo "packaged FOMOD: $ZIP"
unzip -l "$ZIP"
