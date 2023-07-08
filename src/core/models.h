#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <BS_thread_pool.hpp>
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

struct Problem {
    int id;

    Area room;
    Area stage;

    std::vector<int> musicians;
    std::vector<Attendee> attendees;

    Problem(int id,
            const Area &room,
            const Area &stage,
            const std::vector<int> &musicians,
            const std::vector<Attendee> &attendees)
            : id(id), room(room), stage(stage), musicians(musicians), attendees(attendees) {}

    explicit Problem(const std::filesystem::path &file) {
        auto data = readJson(file);

        id = getIdFromFile(file);

        room = {{0, 0}, data["room_width"].GetDouble(), data["room_height"].GetDouble()};

        const auto &stageBottomLeftArr = data["stage_bottom_left"].GetArray();
        stage = {{stageBottomLeftArr[0].GetDouble() + 10, stageBottomLeftArr[1].GetDouble() + 10},
                 data["stage_width"].GetDouble() - 20, data["stage_height"].GetDouble() - 20};

        const auto &musiciansArr = data["musicians"].GetArray();
        musicians.reserve(musiciansArr.Size());
        for (const auto &value : musiciansArr) {
            musicians.emplace_back(value.GetInt());
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
    }
};

struct Solution {
    Problem problem;
    std::vector<Point> placements;

    Solution(const Problem &problem, const std::vector<Point> &placements) : problem(problem), placements(placements) {}

    bool isValid() const {
        if (problem.musicians.size() != placements.size()) {
            return false;
        }

        for (const auto &placement : placements) {
            if (!problem.stage.isInside(placement)) {
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

    long long getScore() const {
        BS::thread_pool threadPool;

        std::vector<std::future<long long>> futures;
        futures.reserve(problem.attendees.size());

        for (const auto &attendee : problem.attendees) {
            futures.emplace_back(threadPool.submit([&] {
                long long score = 0;

                for (std::size_t i = 0; i < placements.size(); i++) {
                    if (attendee.tastes[problem.musicians[i]] == 0) {
                        continue;
                    }

                    bool isBlocked = false;
                    for (std::size_t j = 0; j < placements.size(); j++) {
                        if (i != j && isBlockingPlacement(placements[i], placements[j], attendee.position)) {
                            isBlocked = true;
                            break;
                        }
                    }

                    if (isBlocked) {
                        continue;
                    }

                    double taste = 1'000'000.0 * attendee.tastes[problem.musicians[i]];
                    double distance = attendee.position.distanceTo(placements[i]);
                    score += std::ceil(taste / std::pow(distance, 2));
                }

                return score;
            }));
        }

        long long score = 0;
        for (auto &future : futures) {
            score += future.get();
        }

        return score;
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
    bool isBlockingPlacement(const Point &musician1, const Point &musician2, const Point &attendee) const {
        // Based on https://math.stackexchange.com/a/275537

        double ax = musician1.x;
        double ay = musician1.y;

        double bx = attendee.x;
        double by = attendee.y;

        double cx = musician2.x;
        double cy = musician2.y;
        double r = 5;

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
