// SPDX-License-Identifier: GPL-3.0-or-later
// S043: toggle switches in the settings flyout, their ease-in-out motion, and saving smooth scrolling.
#include "hitokoto/core/settings.hpp"
#include "hitokoto/core/theme.hpp"
#include "hitokoto/core/toggle_switch.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace hitokoto::core;

namespace {

struct UpdateCase {
    const char* name;
    std::string original;
    bool smooth;
    std::string expected;
};

void expectSmoothScrollingUpdates(const std::vector<UpdateCase>& cases) {
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        const SettingsSourceUpdate update = updateSmoothScrollingInSettingsSource(c.original, c.smooth);
        EXPECT_FALSE(update.error.has_value()) << (update.error ? update.error->message : "");
        EXPECT_EQ(update.source, c.expected);
        const SettingsLoadResult before = parseSettings(c.original);
        const SettingsLoadResult after = parseSettings(update.source);
        Settings expected = before.settings;
        expected.smoothScrolling = c.smooth;
        EXPECT_EQ(after.settings, expected);
    }
}

class TempDir {
public:
    TempDir() {
        std::random_device rd;
        path_ =
            std::filesystem::temp_directory_path() / ("hitokoto-s043-" + std::to_string(rd()) + std::to_string(rd()));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST(S043_SmoothScrollingToggle, SwitchTokensFallbackAndResolve) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme base = builtinTheme(scheme);
        const SwitchStyle defaults = switchStyle(base);
        EXPECT_EQ(defaults.track, base.border);
        EXPECT_EQ(defaults.checkedTrack, base.accent);
        EXPECT_EQ(defaults.knob, base.background);
        EXPECT_EQ(defaults.checkedKnob, base.background);
        EXPECT_EQ(defaults.border, base.border);
        EXPECT_DOUBLE_EQ(defaults.borderWidth, 0.0);
        EXPECT_DOUBLE_EQ(defaults.width, 36.0);
        EXPECT_DOUBLE_EQ(defaults.height, 20.0);
        EXPECT_DOUBLE_EQ(defaults.radius, 10.0);
        EXPECT_DOUBLE_EQ(defaults.knobMargin, 2.0);
    }

    const auto parsed = parseTheme(
        "[switch]\n"
        "track = \"#111111\"\n"
        "checked-track = \"#222222\"\n"
        "knob = \"#333333\"\n"
        "checked-knob = \"#444444\"\n"
        "border = \"#555555\"\n"
        "border-width = 1\n"
        "width = 44\n"
        "height = 24\n"
        "radius = 6\n"
        "knob-margin = 3\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    EXPECT_TRUE(parsed.diagnostics.empty());
    const SwitchStyle custom = switchStyle(*parsed.theme);
    EXPECT_EQ(custom.track.toHex(), "#111111");
    EXPECT_EQ(custom.checkedTrack.toHex(), "#222222");
    EXPECT_EQ(custom.knob.toHex(), "#333333");
    EXPECT_EQ(custom.checkedKnob.toHex(), "#444444");
    EXPECT_EQ(custom.border.toHex(), "#555555");
    EXPECT_DOUBLE_EQ(custom.borderWidth, 1.0);
    EXPECT_DOUBLE_EQ(custom.width, 44.0);
    EXPECT_DOUBLE_EQ(custom.height, 24.0);
    EXPECT_DOUBLE_EQ(custom.radius, 6.0);
    EXPECT_DOUBLE_EQ(custom.knobMargin, 3.0);
}

TEST(S043_SmoothScrollingToggle, UpdateSettingChangesOnlyThatKeyOfItsSection) {
    expectSmoothScrollingUpdates({
        {"value in [editor] with a comment and other keys",
         "# mine\n[appearance]\ntheme = \"dark\"\n\n[editor]\nsmooth-scrolling = true # fast\n"
         "front-matter-as-properties = false\n",
         false,
         "# mine\n[appearance]\ntheme = \"dark\"\n\n[editor]\nsmooth-scrolling = false # fast\n"
         "front-matter-as-properties = false\n"},
        {"top-level key before any section", "smooth-scrolling = false\n[appearance]\ntheme = \"dark\"\n", true,
         "smooth-scrolling = true\n[appearance]\ntheme = \"dark\"\n"},
        {"same key under an unknown section stays",
         "[other]\nsmooth-scrolling = true\n[editor]\nsmooth-scrolling = true\n", false,
         "[other]\nsmooth-scrolling = true\n[editor]\nsmooth-scrolling = false\n"},
        {"already the value", "[editor]\nsmooth-scrolling = false\n", false, "[editor]\nsmooth-scrolling = false\n"},
        {"no value", "[editor]\nsmooth-scrolling =\n", true, "[editor]\nsmooth-scrolling = true\n"},
    });
}

TEST(S043_SmoothScrollingToggle, UpdateSettingAddsTheKeyBelowTheSectionOrTheSectionAtTheEnd) {
    expectSmoothScrollingUpdates({
        {"[editor] without the key", "[editor]\nfront-matter-as-properties = false\n", false,
         "[editor]\nsmooth-scrolling = false\nfront-matter-as-properties = false\n"},
        {"no [editor]", "[appearance]\ntheme = \"dark\"\n", false,
         "[appearance]\ntheme = \"dark\"\n\n[editor]\nsmooth-scrolling = false\n"},
        {"empty file", "", false, "[editor]\nsmooth-scrolling = false\n"},
        {"[editor] as the last line without a line break", "[appearance]\ntheme = \"dark\"\n[editor]", false,
         "[appearance]\ntheme = \"dark\"\n[editor]\nsmooth-scrolling = false\n"},
    });
}

TEST(S043_SmoothScrollingToggle, UpdateSettingKeepsLineEndingsAndAByteOrderMark) {
    expectSmoothScrollingUpdates({
        {"CRLF", "[editor]\r\nsmooth-scrolling = true\r\n", false, "[editor]\r\nsmooth-scrolling = false\r\n"},
        {"CRLF, section added", "[appearance]\r\ntheme = \"dark\"\r\n", false,
         "[appearance]\r\ntheme = \"dark\"\r\n\r\n[editor]\r\nsmooth-scrolling = false\r\n"},
        {"byte order mark", "\xEF\xBB\xBF[editor]\nsmooth-scrolling = true\n", false,
         "\xEF\xBB\xBF[editor]\nsmooth-scrolling = false\n"},
    });
}

TEST(S043_SmoothScrollingToggle, UpdateSettingRefusesAFormItCannotChange) {
    const std::vector<std::pair<const char*, std::string>> cases = {
        // TOML 1.0: a dotted key defines the table editor; a header [editor] may not define it again.
        {"dotted key", "editor.smooth-scrolling = true\n"},
        {"header with spaces", "[ editor ]\nsmooth-scrolling = true\n"},
        {"quoted key", "[editor]\n\"smooth-scrolling\" = true\n"},
    };
    for (const auto& [name, original] : cases) {
        SCOPED_TRACE(name);
        const SettingsSourceUpdate update = updateSmoothScrollingInSettingsSource(original, false);
        EXPECT_TRUE(update.error.has_value());
        EXPECT_TRUE(update.source.empty());
    }
}

TEST(S043_SmoothScrollingToggle, UpdateSmoothScrollingInFileWritesANewFile) {
    TempDir dir;
    const std::filesystem::path config = dir.path() / "sub" / "config.toml";
    EXPECT_FALSE(updateSmoothScrollingSettingInFile(config, false).has_value());
    const SettingsLoadResult loaded = loadSettingsFile(config);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_FALSE(loaded.settings.smoothScrolling);

    // An existing file changes only in that value.
    {
        std::ofstream out(config, std::ios::binary | std::ios::trunc);
        out << "# mine\n[editor]\nsmooth-scrolling = false\n";
    }
    EXPECT_FALSE(updateSmoothScrollingSettingInFile(config, true).has_value());
    const ReadResult read = readFile(config);
    if (!read.bytes) {
        FAIL() << (read.error ? read.error->message : "cannot read config.toml");
    }
    EXPECT_EQ(*read.bytes, "# mine\n[editor]\nsmooth-scrolling = true\n");

    // A folder of that name is no file to change.
    const std::filesystem::path folder = dir.path() / "folder.toml";
    std::filesystem::create_directories(folder);
    EXPECT_TRUE(updateSmoothScrollingSettingInFile(folder, true).has_value());
    EXPECT_TRUE(std::filesystem::is_directory(folder));
}

TEST(S043_SmoothScrollingToggle, EaseInOutProgressIsSlowAtBothEnds) {
    EXPECT_DOUBLE_EQ(easeInOut(0.0), 0.0);
    EXPECT_DOUBLE_EQ(easeInOut(0.5), 0.5);
    EXPECT_DOUBLE_EQ(easeInOut(1.0), 1.0);
    EXPECT_LT(easeInOut(0.2), 0.2);  // eases in: behind a linear movement at the start
    EXPECT_GT(easeInOut(0.8), 0.8);  // eases out: ahead of a linear movement near the end
    EXPECT_DOUBLE_EQ(easeInOut(-1.0), 0.0);
    EXPECT_DOUBLE_EQ(easeInOut(2.0), 1.0);
    double last = 0;
    for (int i = 1; i <= 100; ++i) {
        const double v = easeInOut(i / 100.0);
        EXPECT_GE(v, last);
        last = v;
    }
}

TEST(S043_SmoothScrollingToggle, KnobTravelsInsideTheTrackAndColoursBlend) {
    SwitchStyle style;
    style.width = 36;
    style.height = 20;
    style.knobMargin = 2;
    style.track = Color{0, 0, 0, 255};
    style.checkedTrack = Color{200, 100, 50, 255};
    style.knob = Color{10, 10, 10, 255};
    style.checkedKnob = Color{250, 250, 250, 255};

    const ToggleGeometry off = toggleGeometry(style, 0.0);
    EXPECT_DOUBLE_EQ(off.knobDiameter, 16.0);
    EXPECT_DOUBLE_EQ(off.knobLeft, 2.0);
    EXPECT_DOUBLE_EQ(off.knobTop, 2.0);
    EXPECT_EQ(off.track, style.track);
    EXPECT_EQ(off.knob, style.knob);

    const ToggleGeometry on = toggleGeometry(style, 1.0);
    EXPECT_DOUBLE_EQ(on.knobLeft, 18.0);
    EXPECT_EQ(on.track, style.checkedTrack);
    EXPECT_EQ(on.knob, style.checkedKnob);

    const ToggleGeometry half = toggleGeometry(style, 0.5);
    EXPECT_DOUBLE_EQ(half.knobLeft, 10.0);
    EXPECT_EQ(half.track, (Color{100, 50, 25, 255}));

    // A margin larger than half the height leaves no knob, never a negative size.
    style.knobMargin = 15;
    EXPECT_DOUBLE_EQ(toggleGeometry(style, 0.0).knobDiameter, 0.0);
}
