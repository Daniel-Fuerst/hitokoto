// SPDX-License-Identifier: GPL-3.0-or-later
// S024 (ADR 0007): the properties of a note's front matter, read from its YAML with a parser of our
// own for a subset of YAML 1.2. Read only: nothing here writes YAML (ADR 0002).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// Type of a property, inferred from its value (S024, "Types").
enum class PropertyType : std::uint8_t {
    Text,
    Number,
    Checkbox,
    Date,
    DateTime,
    List,
    Tags,
    Unsupported,  ///< a mapping, or a sequence holding a mapping or a sequence: shown as JSON
};

/// Fields of a date (`YYYY-MM-DD`) or a date and time (`YYYY-MM-DDTHH:MM[:SS]`); -1 = not given.
struct PropertyDate {
    int year = -1;
    int month = -1;
    int day = -1;
    int hour = -1;
    int minute = -1;
    int second = -1;
    friend bool operator==(const PropertyDate&, const PropertyDate&) = default;
};

/// One top-level key of the front matter.
struct Property {
    std::u16string key;  ///< as written, without quotes
    PropertyType type = PropertyType::Text;
    /// Text: the scalar's value (quotes and escapes resolved, plain and folded scalars folded, block
    /// scalars with their line breaks), empty for null. Number, Checkbox, Date, DateTime: the value
    /// as written. Unsupported: the value as JSON (`JSON.stringify`). List, Tags: empty.
    std::u16string text;
    std::vector<std::u16string> items;  ///< List, Tags: the items' text
    bool checked = false;               ///< Checkbox
    PropertyDate date;                  ///< Date, DateTime
    std::size_t keyLine = 0;            ///< index of the key's line in the YAML lines passed in
    std::size_t valueLine = 0;          ///< where a click on the value puts the caret: line ...
    std::size_t valueColumn = 0;        ///< ... and column (UTF-16), see S024 "When the YAML shows"
    friend bool operator==(const Property&, const Property&) = default;
};

/// Why front matter is not shown as properties (S024, "Invalid").
enum class PropertiesErrorReason : std::uint8_t {
    DuplicateKey,
    BadIndentation,
    TabInIndentation,
    UnclosedQuote,
    UnclosedFlowCollection,
    NotAMapping,
    DocumentEnd,
    TooDeep,
    // Valid YAML outside the subset of ADR 0007:
    AnchorOrAlias,
    Tag,
    ComplexKey,
    Directive,
    IndentationIndicator,
};

/// Whether a reason is wrong YAML (false) or valid YAML outside the subset (true).
[[nodiscard]] bool isUnsupported(PropertiesErrorReason reason) noexcept;
/// The reason as the badge names it, e.g. "duplicate key", "anchor or alias".
[[nodiscard]] std::string_view reasonText(PropertiesErrorReason reason) noexcept;

/// S024 code review O5: the compact JSON of an unsupported value as `JSON.stringify(value, null, 2)`
/// writes it, at most `maxLines` lines, then a line "…" if more follows. Only that prefix is read.
[[nodiscard]] std::u16string prettyJson(std::u16string_view json, std::size_t maxLines);

struct PropertiesError {
    PropertiesErrorReason reason = PropertiesErrorReason::NotAMapping;
    std::size_t line = 0;  ///< index of the line in the YAML lines passed in
    friend bool operator==(const PropertiesError&, const PropertiesError&) = default;
};

struct Properties {
    std::vector<Property> rows;            ///< in document order; empty if `error` is set
    std::optional<PropertiesError> error;  ///< the first error; the front matter then stays text
};

/// Reads the YAML lines between the two `---` lines of closed front matter (without them). Never
/// throws; linear in the length of the input; nesting deeper than 64 levels is an error.
[[nodiscard]] Properties parseProperties(std::span<const std::u16string> yamlLines);

}  // namespace hitokoto::core
