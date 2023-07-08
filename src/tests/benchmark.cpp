#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <core/models.h>

#include <benchmark/benchmark.h>

Solution generateSolution(std::int64_t problemId) {
    auto projectRoot = std::filesystem::current_path();
    while (projectRoot.has_parent_path() && !std::filesystem::is_directory(projectRoot / "problems")) {
        projectRoot = projectRoot.parent_path();
    }

    auto problemFile = projectRoot / "problems" / (std::to_string(problemId) + ".json");
    Problem problem(problemFile);

    std::vector<Point> placements;

    Point nextPlacement = problem.stage.bottomLeft;
    for (std::size_t i = 0; i < problem.musicians.size(); i++) {
        placements.emplace_back(nextPlacement);

        nextPlacement.x += 10;
        if (!problem.stage.isInside(nextPlacement)) {
            nextPlacement.x = problem.stage.bottomLeft.x;
            nextPlacement.y += 10;
        }
    }

    return {problem, placements};
}

static void isValid(benchmark::State &state) {
    auto solution = generateSolution(state.range(0));

    for (auto _ : state) {
        solution.isValid();
    }
}

BENCHMARK(isValid)->Arg(1)->Arg(2)->Arg(5)->Arg(20)->Arg(42)->Arg(56)->Arg(73)->Arg(79)->Unit(benchmark::kMillisecond);

static void getScore(benchmark::State &state) {
    auto solution = generateSolution(state.range(0));

    for (auto _ : state) {
        solution.getScore();
    }
}

BENCHMARK(getScore)->Arg(1)->Arg(2)->Arg(5)->Arg(20)->Arg(42)->Arg(56)->Arg(73)->Arg(79)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
