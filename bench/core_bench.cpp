// SPDX-License-Identifier: GPL-3.0-or-later
// Core micro-benchmarks for the large-file path (S007) and theme loading (S005/S006).
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/text_codec.hpp"
#include "hitokoto/core/theme.hpp"

#include <benchmark/benchmark.h>

#include <string>

using namespace hitokoto::core;

namespace {

std::string largeDocument(std::size_t target) {
    std::string out;
    out.reserve(target + 512);
    int i = 0;
    while (out.size() < target) {
        ++i;
        out.append(static_cast<std::size_t>(1 + i % 3), '#');
        out += " Section " + std::to_string(i) + "\n\n";
        out +=
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor \xC3\xA4 incididunt.\n\n";
        out += "- first item of the list\n- second item of the list\n\n";
    }
    return out;
}

const std::string& tenMegabytes() {
    static const std::string doc = largeDocument(10u << 20);
    return doc;
}

void S007_Decode10MB(benchmark::State& state) {
    for (auto _ : state) {
        auto r = decodeUtf8(tenMegabytes());
        benchmark::DoNotOptimize(r);
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(tenMegabytes().size()));
}
BENCHMARK(S007_Decode10MB)->Unit(benchmark::kMillisecond);

void S007_Encode10MB(benchmark::State& state) {
    const auto decoded = decodeUtf8(tenMegabytes()).value.value();
    for (auto _ : state) {
        auto r = encodeUtf8(decoded.text, decoded.hasBom, decoded.uniformEnding);
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(S007_Encode10MB)->Unit(benchmark::kMillisecond);

void S007_RenderAllLines10MB(benchmark::State& state) {
    const auto decoded = decodeUtf8(tenMegabytes()).value.value();
    const auto registry = ElementRegistry::withBuiltins();
    for (auto _ : state) {
        std::size_t start = 0;
        std::size_t styled = 0;
        const std::u16string_view text = decoded.text;
        while (start <= text.size()) {
            const auto nl = text.find(u'\n', start);
            const auto line =
                text.substr(start, nl == std::u16string_view::npos ? std::u16string_view::npos : nl - start);
            styled += registry.render(line, {false}).empty() ? 0u : 1u;
            if (nl == std::u16string_view::npos)
                break;
            start = nl + 1;
        }
        benchmark::DoNotOptimize(styled);
    }
}
BENCHMARK(S007_RenderAllLines10MB)->Unit(benchmark::kMillisecond);

void S002_RenderOneHeadingLine(benchmark::State& state) {
    const auto registry = ElementRegistry::withBuiltins();
    for (auto _ : state) {
        auto r = registry.render(u"## A heading of typical length", {state.range(0) != 0});
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(S002_RenderOneHeadingLine)->Arg(0)->Arg(1);

void S005_ParseBuiltinTheme(benchmark::State& state) {
    for (auto _ : state) {
        auto r = parseTheme(builtinThemeSource(ColorScheme::Dark), Theme{}, true);
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(S005_ParseBuiltinTheme)->Unit(benchmark::kMicrosecond);

}  // namespace
