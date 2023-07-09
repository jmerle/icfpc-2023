#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <oneapi/tbb.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

rapidjson::Document readJson(const std::filesystem::path &file) {
    std::FILE *fp = std::fopen(file.c_str(), "r");

    char readBuffer[65536];
    rapidjson::FileReadStream inputStream(fp, readBuffer, sizeof(readBuffer));

    rapidjson::Document doc;
    doc.ParseStream(inputStream);

    std::fclose(fp);
    return doc;
}

int getIdFromFile(const std::filesystem::path &file) {
    std::string fileName = file.filename();
    std::string extension = file.extension();
    return std::stoi(fileName.substr(0, fileName.size() - extension.size()));
}

struct Point {
    double x;
    double y;

    Point() : x(0), y(0) {}

    Point(double x, double y) : x(x), y(y) {}

    double distanceTo(const Point &other) const {
        return std::sqrt(std::pow(other.x - x, 2) + std::pow(other.y - y, 2));
    }
};

struct Area {
    Point bottomLeft;
    double width;
    double height;

    Area() : width(0), height(0) {}

    Area(const Point &bottomLeft, double width, double height) : bottomLeft(bottomLeft), width(width), height(height) {}

    bool isInside(const Point &point) const {
        return point.x >= bottomLeft.x
               && point.x <= bottomLeft.x + width
               && point.y >= bottomLeft.y
               && point.y <= bottomLeft.y + height;
    }
};

struct Attendee {
    Point position;
    std::vector<double> tastes;

    Attendee() = default;

    Attendee(const Point &position, const std::vector<double> &tastes) : position(position), tastes(tastes) {}
};

struct Pillar {
    Point center;
    double radius;

    Pillar() = default;

    Pillar(const Point &center, double radius) : center(center), radius(radius) {}
};

struct Problem {
    int id;

    Area room;
    Area stage;

    std::vector<int> musicians;
    std::vector<Attendee> attendees;
    std::vector<Pillar> pillars;

    Problem(int id,
            const Area &room,
            const Area &stage,
            const std::vector<int> &musicians,
            const std::vector<Attendee> &attendees,
            const std::vector<Pillar> &pillars)
            : id(id), room(room), stage(stage), musicians(musicians), attendees(attendees), pillars(pillars) {
        postProcessInput();
    }

    explicit Problem(const std::filesystem::path &file) {
        auto data = readJson(file);

        id = getIdFromFile(file);

        room = {{0, 0}, data["room_width"].GetDouble(), data["room_height"].GetDouble()};

        const auto &stageBottomLeftArr = data["stage_bottom_left"].GetArray();
        stage = {{stageBottomLeftArr[0].GetDouble(), stageBottomLeftArr[1].GetDouble()},
                 data["stage_width"].GetDouble(), data["stage_height"].GetDouble()};

        const auto &musiciansArr = data["musicians"].GetArray();
        musicians.reserve(musiciansArr.Size());
        for (const auto &musicianValue : musiciansArr) {
            musicians.emplace_back(musicianValue.GetInt());
        }

        const auto &attendeesArr = data["attendees"].GetArray();
        attendees.reserve(attendeesArr.Size());
        for (const auto &attendeeValue : attendeesArr) {
            const auto &attendeeObj = attendeeValue.GetObject();

            Point position(attendeeObj["x"].GetDouble(), attendeeObj["y"].GetDouble());
            std::vector<double> tastes;

            const auto &tastesArr = attendeeObj["tastes"].GetArray();
            tastes.reserve(tastesArr.Size());
            for (const auto &tasteValue : tastesArr) {
                tastes.emplace_back(tasteValue.GetDouble());
            }

            attendees.emplace_back(position, tastes);
        }

        const auto &pillarsArr = data["pillars"].GetArray();
        pillars.reserve(pillarsArr.Size());
        for (const auto &pillarValue : pillarsArr) {
            const auto &pillarObj = pillarValue.GetObject();

            const auto &centerArr = pillarObj["center"].GetArray();
            Point center(centerArr[0].GetDouble(), centerArr[1].GetDouble());

            double radius = pillarObj["radius"].GetDouble();

            pillars.emplace_back(center, radius);
        }

        postProcessInput();
    }

    friend std::ostream &operator<<(std::ostream &stream, const Problem &problem) {
        return stream << "[Problem " << problem.id << "] ";
    }

private:
    void postProcessInput() {
        stage.bottomLeft.x += 10;
        stage.bottomLeft.y += 10;
        stage.width -= 20;
        stage.height -= 20;
    }
};

enum class ScoreType {
    AUTO,
    LIGHTNING,
    FULL
};

struct Solution {
    std::shared_ptr<Problem> problem;
    std::vector<Point> placements;

    Solution(const std::shared_ptr<Problem> &problem, const std::vector<Point> &placements)
            : problem(problem), placements(placements) {}

    bool isValid() const {
        if (problem->musicians.size() != placements.size()) {
            return false;
        }

        for (const auto &placement : placements) {
            if (!problem->stage.isInside(placement)) {
                return false;
            }
        }

        for (std::size_t i = 0; i < placements.size(); i++) {
            for (std::size_t j = 0; j < placements.size(); j++) {
                if (i != j && placements[i].distanceTo(placements[j]) < 10) {
                    return false;
                }
            }
        }

        return true;
    }

    long long getScore(ScoreType type = ScoreType::AUTO) const {
        if (type == ScoreType::AUTO) {
            type = problem->id <= 55 ? ScoreType::LIGHTNING : ScoreType::FULL;
        }

        std::vector<double> closenessFactors;
        if (type == ScoreType::FULL) {
            closenessFactors.reserve(placements.size());
            for (std::size_t i = 0; i < placements.size(); i++) {
                double closeness = 1;

                for (std::size_t j = 0; j < placements.size(); j++) {
                    if (i != j && problem->musicians[i] == problem->musicians[j]) {
                        closeness += 1.0 / placements[i].distanceTo(placements[j]);
                    }
                }

                closenessFactors.emplace_back(closeness);
            }
        }

        return oneapi::tbb::parallel_reduce(
                oneapi::tbb::blocked_range<std::size_t>(0, problem->attendees.size()),
                static_cast<long long>(0),
                [&](const oneapi::tbb::blocked_range<std::size_t> &range, long long init) {
                    for (std::size_t attendeeIdx = range.begin(); attendeeIdx != range.end(); attendeeIdx++) {
                        const auto &attendee = problem->attendees[attendeeIdx];
                        for (std::size_t i = 0; i < placements.size(); i++) {
                            if (attendee.tastes[problem->musicians[i]] == 0) {
                                continue;
                            }

                            bool isBlocked = false;
                            for (std::size_t j = 0; j < placements.size(); j++) {
                                if (i != j && isBlocking(placements[i], attendee.position, placements[j], 5)) {
                                    isBlocked = true;
                                    break;
                                }
                            }

                            if (isBlocked) {
                                continue;
                            }

                            if (type == ScoreType::FULL) {
                                for (const auto &pillar : problem->pillars) {
                                    if (isBlocking(placements[i], attendee.position, pillar.center, pillar.radius)) {
                                        isBlocked = true;
                                        break;
                                    }
                                }

                                if (isBlocked) {
                                    continue;
                                }
                            }

                            double taste = 1'000'000.0 * attendee.tastes[problem->musicians[i]];
                            double distance = attendee.position.distanceTo(placements[i]);
                            double impact = std::ceil(taste / std::pow(distance, 2));

                            if (type == ScoreType::LIGHTNING) {
                                init += impact;
                            } else {
                                init += std::ceil(closenessFactors[i] * impact);
                            }
                        }
                    }

                    return init;
                },
                [](long long lhs, long long rhs) {
                    return lhs + rhs;
                }
        );
    }

    rapidjson::Document toJson() const {
        rapidjson::Document doc;
        doc.SetObject();

        rapidjson::Value placementsArr;
        placementsArr.SetArray();

        for (const auto &placement : placements) {
            rapidjson::Value placementObj;
            placementObj.SetObject();

            rapidjson::Value xValue;
            xValue.SetDouble(placement.x);

            rapidjson::Value yValue;
            yValue.SetDouble(placement.y);

            placementObj.AddMember("x", xValue, doc.GetAllocator());
            placementObj.AddMember("y", yValue, doc.GetAllocator());

            placementsArr.PushBack(placementObj, doc.GetAllocator());
        }

        doc.AddMember("placements", placementsArr, doc.GetAllocator());
        return doc;
    }

private:
    bool isBlocking(const Point &from, const Point &to, const Point &blockingCenter, double blockingRadius) const {
        // Based on https://math.stackexchange.com/a/275537

        double ax = from.x;
        double ay = from.y;

        double bx = to.x;
        double by = to.y;

        double cx = blockingCenter.x;
        double cy = blockingCenter.y;
        double r = blockingRadius;

        ax -= cx;
        ay -= cy;

        bx -= cx;
        by -= cy;

        double a = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
        double b = 2 * (ax * (bx - ax) + ay * (by - ay));
        double c = ax * ax + ay * ay - r * r;

        double disc = b * b - 4 * a * c;
        if (disc <= 0) {
            return false;
        }

        double discSqrt = std::sqrt(disc);
        double t1 = (-b + discSqrt) / (2 * a);
        double t2 = (-b - discSqrt) / (2 * a);
        return (0 < t1 && t1 < 1) || (0 < t2 && t2 < 1);
    }
};
