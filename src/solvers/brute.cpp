#include <cstddef>
#include <iostream>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include <core/models.h>
#include <core/program.h>
#include <core/timer.h>

Solution generateRandomSolution(const std::shared_ptr<Problem> &problem) {
    std::vector<Point> placements;
    placements.reserve(problem->musicians.size());

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    std::uniform_real_distribution<double> xDist(problem->stage.bottomLeft.x,
                                                 problem->stage.bottomLeft.x + problem->stage.width);
    std::uniform_real_distribution<double> yDist(problem->stage.bottomLeft.y,
                                                 problem->stage.bottomLeft.y + problem->stage.height);

    for (std::size_t i = 0; i < problem->musicians.size(); i++) {
        while (true) {
            Point point(xDist(rng), yDist(rng));

            bool tooClose = false;
            for (const auto &placement : placements) {
                if (point.distanceTo(placement) < 10) {
                    tooClose = true;
                    break;
                }
            }

            if (tooClose) {
                continue;
            }

            placements.emplace_back(point);
            break;
        }
    }

    return {problem, placements};
}

int main(int argc, char *argv[]) {
    Program program("brute");
    auto problems = program.parseArgs(argc, argv);

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    double randomTime = 30;
    double optimizeTime = 240;
    double submissionInterval = 30;

    for (const auto &problem : problems) {
        std::cout << *problem << "Finding best random solution for " << randomTime << " seconds" << std::endl;

        auto bestSolution = generateRandomSolution(problem);
        auto bestScore = bestSolution.getScore();

        Timer randomTimer;
        std::size_t randomIteration = 1;

        while (randomTimer.elapsedSeconds() < randomTime) {
            randomIteration++;
            auto newSolution = generateRandomSolution(problem);

            auto newScore = newSolution.getScore();
            if (newScore > bestScore) {
                bestSolution = newSolution;
                bestScore = newScore;
            }
        }

        std::cout << *problem << "Generated " << randomIteration << " random solutions" << std::endl;
        program.submit(bestSolution, bestScore);

        std::uniform_int_distribution<std::size_t> indexDist(0, problem->musicians.size() - 1);
        std::uniform_real_distribution<double> deltaDist(-5.0, 5.0);
        std::uniform_real_distribution<double> xDist(problem->stage.bottomLeft.x,
                                                     problem->stage.bottomLeft.x + problem->stage.width);
        std::uniform_real_distribution<double> yDist(problem->stage.bottomLeft.y,
                                                     problem->stage.bottomLeft.y + problem->stage.height);

        Timer optimizeTimer;
        Timer submissionTimer;

        std::size_t optimizeIteration = 0;

        std::cout << *problem << "Optimizing for "
                  << optimizeTime << " seconds, reporting every "
                  << submissionInterval << " seconds"
                  << std::endl;

        while (optimizeTimer.elapsedSeconds() < optimizeTime) {
            optimizeIteration++;
            Solution newSolution(bestSolution);

            switch (optimizeIteration % 3) {
                case 0: {
                    std::swap(newSolution.placements[indexDist(rng)], newSolution.placements[indexDist(rng)]);
                    break;
                }
                case 1: {
                    auto &placement = newSolution.placements[indexDist(rng)];
                    placement.x += deltaDist(rng);
                    placement.y += deltaDist(rng);
                    break;
                }
                case 2: {
                    auto &placement = newSolution.placements[indexDist(rng)];
                    placement.x = xDist(rng);
                    placement.y = yDist(rng);
                    break;
                }
            }

            if (!newSolution.isValid()) {
                continue;
            }

            auto newScore = newSolution.getScore();
            if (newScore > bestScore) {
                bestSolution = newSolution;
                bestScore = newScore;
            }

            if (submissionTimer.elapsedSeconds() >= submissionInterval) {
                program.submit(bestSolution, bestScore);
                submissionTimer.reset();
            }
        }

        program.submit(bestSolution, bestScore);
        std::cout << *problem << "Ran " << optimizeIteration << " optimization iterations" << std::endl;
    }

    return 0;
}
