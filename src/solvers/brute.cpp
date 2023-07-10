#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include <core/models.h>
#include <core/program.h>
#include <core/timer.h>

bool isTooClose(const std::vector<Point> &placements, const Point &newPlacement) {
    for (const auto &placement : placements) {
        if (newPlacement.distanceTo2(placement) < 100) {
            return true;
        }
    }

    return false;
}

Solution generateRandomSolution(const std::shared_ptr<Problem> &problem) {
    std::vector<Point> possiblePlacements;

    Point nextPlacement = problem->stage.bottomLeft;
    while (problem->stage.isInside(nextPlacement)) {
        if (!isTooClose(possiblePlacements, nextPlacement)) {
            possiblePlacements.emplace_back(nextPlacement);
        }

        nextPlacement.x += 10;
    }

    nextPlacement = {problem->stage.bottomLeft.x, problem->stage.bottomLeft.y + problem->stage.height};
    while (problem->stage.isInside(nextPlacement)) {
        if (!isTooClose(possiblePlacements, nextPlacement)) {
            possiblePlacements.emplace_back(nextPlacement);
        }

        nextPlacement.x += 10;
    }

    nextPlacement = problem->stage.bottomLeft;
    while (problem->stage.isInside(nextPlacement)) {
        if (!isTooClose(possiblePlacements, nextPlacement)) {
            possiblePlacements.emplace_back(nextPlacement);
        }

        nextPlacement.y += 10;
    }

    nextPlacement = {problem->stage.bottomLeft.x + problem->stage.width, problem->stage.bottomLeft.y};
    while (problem->stage.isInside(nextPlacement)) {
        if (!isTooClose(possiblePlacements, nextPlacement)) {
            possiblePlacements.emplace_back(nextPlacement);
        }

        nextPlacement.y += 10;
    }

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    std::uniform_real_distribution<double> xDist(problem->stage.bottomLeft.x,
                                                 problem->stage.bottomLeft.x + problem->stage.width);
    std::uniform_real_distribution<double> yDist(problem->stage.bottomLeft.y,
                                                 problem->stage.bottomLeft.y + problem->stage.height);

    while (possiblePlacements.size() < problem->musicians.size()) {
        while (true) {
            Point point(xDist(rng), yDist(rng));

            if (!isTooClose(possiblePlacements, point)) {
                possiblePlacements.emplace_back(point);
                break;
            }
        }
    }

    std::shuffle(possiblePlacements.begin(), possiblePlacements.end(), rng);

    std::vector<Point> placements;
    placements.reserve(problem->musicians.size());

    for (std::size_t i = 0; i < problem->musicians.size(); i++) {
        placements.emplace_back(possiblePlacements[i]);
    }

    return {problem, placements};
}

int main(int argc, char *argv[]) {
    Program program("brute");
    auto problems = program.parseArgs(argc, argv);

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    double randomTime = 30;
    double optimizeTime = 150;
    double submissionInterval = 60;

    for (const auto &problem : problems) {
        Solution bestSolution(problem, {}, {});
        long long bestScore = 0;

        if (program.isServerEnabled()) {
            std::cout << *problem << "Retrieving best global solution" << std::endl;
            auto bestGlobalSolution = program.getBestGlobalSolution(problem);
            if (bestGlobalSolution) {
                bestSolution = *bestGlobalSolution;
                bestScore = bestGlobalSolution->getScore();
                program.submit(bestSolution, bestScore);
            } else {
                std::cout << *problem << "No best global solution found" << std::endl;
            }
        }

        if (bestScore == 0) {
            std::cout << *problem << "Generating initial random solution" << std::endl;

            bestSolution = generateRandomSolution(problem);
            bestScore = bestSolution.getScore();
            program.submit(bestSolution, bestScore);
        }

        std::cout << *problem << "Finding best random solution for " << randomTime << " seconds" << std::endl;

        Timer randomTimer;
        std::size_t randomIteration = 0;

        while (randomTimer.elapsedSeconds() < randomTime) {
            randomIteration++;
            auto newSolution = generateRandomSolution(problem);

            auto newScore = newSolution.getScore();
            if (newScore > bestScore) {
                bestSolution = newSolution;
                bestScore = newScore;
            }
        }

        program.submit(bestSolution, bestScore);
        std::cout << *problem << "Generated " << randomIteration << " random solutions" << std::endl;

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
