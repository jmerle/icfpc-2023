#include <cstddef>
#include <vector>

#include <core/models.h>
#include <core/program.h>

int main(int argc, char *argv[]) {
    Program program("starter");
    auto problems = program.parseArgs(argc, argv);

    for (const auto &problem : problems) {
        std::vector<Point> placements;

        Point nextPlacement = problem->stage.bottomLeft;
        for (std::size_t i = 0; i < problem->musicians.size(); i++) {
            placements.emplace_back(nextPlacement);

            nextPlacement.x += 10;
            if (!problem->stage.isInside(nextPlacement)) {
                nextPlacement.x = problem->stage.bottomLeft.x;
                nextPlacement.y += 10;
            }
        }

        Solution solution(problem, placements);
        program.submit(solution);
    }

    return 0;
}
