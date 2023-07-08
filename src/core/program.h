#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND 30
#define CPPHTTPLIB_READ_TIMEOUT_SECOND 30
#define CPPHTTPLIB_WRITE_TIMEOUT_SECOND 30

#include <httplib.h>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <core/config.h>
#include <core/models.h>

extern const unsigned char _binary_source_zip_start;
extern const unsigned char _binary_source_zip_end;

class Program {
    std::string target;

    std::filesystem::path projectRoot;

    httplib::Client server;
    bool serverEnabled;

    std::unordered_map<int, int> localScores;
    std::unordered_map<int, int> globalScores;

    std::mutex submitMutex;

public:
    explicit Program(const std::string &name)
            : target(name), server(getEnv("SERVER_URL", "")), serverEnabled(!getEnv("SERVER_URL", "").empty()) {
        projectRoot = std::filesystem::current_path();
        while (projectRoot.has_parent_path() && !std::filesystem::is_directory(projectRoot / "problems")) {
            projectRoot = projectRoot.parent_path();
        }

        server.set_basic_auth(getEnv("SUBMITTER_USERNAME", "submitter"), getEnv("SUBMITTER_PASSWORD", "hunter2"));
    }

    std::vector<Problem> parseArgs(int argc, char *argv[]) {
        auto problemsRoot = projectRoot / "problems";

        std::vector<Problem> problems;
        if (argc <= 1) {
            for (const auto &entry : std::filesystem::directory_iterator(problemsRoot)) {
                if (entry.path().extension() == ".json" && entry.is_regular_file()) {
                    problems.emplace_back(entry.path());
                }
            }
        } else {
            for (int i = 1; i < argc; i++) {
                std::string id(argv[i]);
                auto path = problemsRoot / (id + ".json");

                if (!std::filesystem::is_regular_file(path)) {
                    std::cout << std::filesystem::absolute(path).string()
                              << " does not exist, skipping "
                              << id
                              << std::endl;
                    continue;
                }

                problems.emplace_back(path);
            }
        }

        std::sort(problems.begin(), problems.end(), [](const Problem &a, const Problem &b) {
            return a.id < b.id;
        });

        if (problems.empty()) {
            std::cout << "No problems to solve" << std::endl;
        } else {
            std::cout << "Solving " << problems.size() << " problem";

            if (problems.size() != 1) {
                std::cout << 's';
            }

            std::cout << ':';

            for (const auto &problem : problems) {
                std::cout << ' ' << problem.id;
            }

            std::cout << std::endl;
        }

        if (serverEnabled) {
            loadGlobalScores();
        }

        return problems;
    }

    void submit(const Solution &solution) {
        if (!solution.isValid()) {
            return;
        }

        int score = solution.getScore();
        if (score < 0) {
            return;
        }

        std::unique_lock submitLock(submitMutex);

        auto localImprovement = isImprovement(localScores, solution.problem.id, score, "local");
        if (!localImprovement.empty()) {
            auto outputDirectory = projectRoot / "results" / target;
            auto outputFile = outputDirectory / (std::to_string(solution.problem.id) + ".json");

            if (!std::filesystem::is_directory(outputDirectory)) {
                std::filesystem::create_directories(outputDirectory);
            }

            std::ofstream outputStream(outputFile);
            rapidjson::OStreamWrapper outputStreamWrapper(outputStream);
            rapidjson::Writer<rapidjson::OStreamWrapper> writer(outputStreamWrapper);
            solution.toJson().Accept(writer);

            std::cout << localImprovement << std::endl;
            localScores[solution.problem.id] = score;
        }

        if (!serverEnabled) {
            return;
        }

        auto globalImprovement = isImprovement(globalScores, solution.problem.id, score, "global");
        if (!globalImprovement.empty()) {
            rapidjson::StringBuffer solutionBuffer;
            rapidjson::Writer<rapidjson::StringBuffer> solutionWriter(solutionBuffer);
            solution.toJson().Accept(solutionWriter);

            std::string source(&_binary_source_zip_start, &_binary_source_zip_end);

            httplib::MultipartFormDataItems formData = {
                    {"problemId",     std::to_string(solution.problem.id), "",              ""},
                    {"score",         std::to_string(score),               "",              ""},
                    {"target",        target,                              "",              ""},
                    {"solutionFile",  solutionBuffer.GetString(),          "solution.json", "application/json"},
                    {"sourceArchive", source,                              "source.zip",    "application/zip"}
            };

            auto response = server.Post("/submit", formData);
            if (response) {
                rapidjson::Document responseData;
                responseData.Parse(response->body.c_str());

                bool newBest = responseData["new_best"].GetBool();
                if (newBest) {
                    std::cout << globalImprovement << std::endl;
                }

                globalScores[solution.problem.id] = responseData["best_score"].GetInt();
            } else {
                std::cout << "[Problem "
                          << solution.problem.id
                          << "] Something went wrong while submitting new global best: "
                          << httplib::to_string(response.error())
                          << std::endl;
            }
        }
    }

private:
    void loadGlobalScores() {
        std::cout << "Loading global scores" << std::endl;

        auto response = server.Get("/scores");
        if (!response) {
            std::cout << "Something went wrong while retrieving the global scores: "
                      << httplib::to_string(response.error())
                      << std::endl;
            return;
        }

        rapidjson::Document responseData;
        responseData.Parse(response->body.c_str());

        for (const auto &pair : responseData.GetObject()) {
            int id = std::stoi(pair.name.GetString());
            int score = pair.value.GetInt();

            globalScores[id] = score;
        }

        std::vector<int> globalIds;
        globalIds.reserve(globalScores.size());
        for (const auto &pair : globalScores) {
            globalIds.emplace_back(pair.first);
        }

        std::sort(globalIds.begin(), globalIds.end());
        for (int id : globalIds) {
            std::cout << "[Problem " << id << "] Best global score: " << globalScores[id] << std::endl;
        }
    }

    std::string isImprovement(const std::unordered_map<int, int> &scores,
                              int problemId,
                              int score,
                              const std::string &label) const {
        if (scores.contains(problemId)) {
            if (scores.at(problemId) >= score) {
                return "";
            }

            return "[Problem " + std::to_string(problemId) + "] Found new best " + label + " score: "
                   + std::to_string(scores.at(problemId)) + " -> " + std::to_string(score);
        }

        return "[Problem " + std::to_string(problemId) + "] Found new best " + label + " score: "
               + std::to_string(score);
    }
};
