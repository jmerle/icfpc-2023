#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
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

struct ThousandsSeparator : std::numpunct<char> {
    char_type do_thousands_sep() const override {
        return ',';
    }

    string_type do_grouping() const override {
        return "\3";
    }
};

class Program {
    std::string target;

    std::filesystem::path projectRoot;

    httplib::Client server;
    bool serverEnabled;

    std::unordered_map<int, long long> localScores;
    std::unordered_map<int, long long> globalScores;

public:
    explicit Program(const std::string &name)
            : target(name), server(getEnv("SERVER_URL", "")), serverEnabled(!getEnv("SERVER_URL", "").empty()) {
        std::cout.imbue(std::locale(std::cout.getloc(), new ThousandsSeparator()));

        projectRoot = std::filesystem::current_path();
        while (projectRoot.has_parent_path() && !std::filesystem::is_directory(projectRoot / "problems")) {
            projectRoot = projectRoot.parent_path();
        }

        server.set_basic_auth(getEnv("SUBMITTER_USERNAME", "submitter"), getEnv("SUBMITTER_PASSWORD", "hunter2"));
    }

    std::vector<std::shared_ptr<Problem>> parseArgs(int argc, char *argv[]) {
        auto problemsRoot = projectRoot / "problems";

        std::vector<std::shared_ptr<Problem>> problems;
        if (argc <= 1) {
            for (const auto &entry : std::filesystem::directory_iterator(problemsRoot)) {
                if (entry.path().extension() == ".json" && entry.is_regular_file()) {
                    problems.emplace_back(std::make_shared<Problem>(entry.path()));
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

                problems.emplace_back(std::make_shared<Problem>(path));
            }
        }

        std::sort(problems.begin(), problems.end(),
                  [](const std::shared_ptr<Problem> &a, const std::shared_ptr<Problem> &b) {
                      return a->id < b->id;
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
                std::cout << ' ' << problem->id;
            }

            std::cout << std::endl;
        }

        if (serverEnabled) {
            loadGlobalScores();
        }

        return problems;
    }

    std::optional<Solution> getBestGlobalSolution(const std::shared_ptr<Problem> &problem) {
        if (!serverEnabled || !globalScores.contains(problem->id)) {
            return std::nullopt;
        }

        auto response = server.Get("/problems/" + std::to_string(problem->id) + "/solution");
        if (!response || response->status >= 400) {
            return std::nullopt;
        }

        rapidjson::Document responseData;
        responseData.Parse(response->body.c_str());

        return Solution(problem, responseData);
    }

    void submit(Solution &solution) {
        submit(solution, solution.getScore());
    }

    void submit(const Solution &solution, long long score) {
        if (!solution.isValid()) {
            return;
        }

        if (score <= 0) {
            return;
        }

        auto localImprovement = isImprovement(localScores, solution.problem->id, score, "local");
        if (!localImprovement.empty()) {
            auto outputDirectory = projectRoot / "results" / target;
            auto outputFile = outputDirectory / (std::to_string(solution.problem->id) + ".json");

            if (!std::filesystem::is_directory(outputDirectory)) {
                std::filesystem::create_directories(outputDirectory);
            }

            std::ofstream outputStream(outputFile);
            rapidjson::OStreamWrapper outputStreamWrapper(outputStream);
            rapidjson::Writer<rapidjson::OStreamWrapper> writer(outputStreamWrapper);
            solution.toJson().Accept(writer);

            std::cout << localImprovement << std::endl;
            localScores[solution.problem->id] = score;
        }

        if (!serverEnabled) {
            return;
        }

        auto globalImprovement = isImprovement(globalScores, solution.problem->id, score, "global");
        if (!globalImprovement.empty()) {
            rapidjson::StringBuffer solutionBuffer;
            rapidjson::Writer<rapidjson::StringBuffer> solutionWriter(solutionBuffer);
            solution.toJson().Accept(solutionWriter);

            std::string source(&_binary_source_zip_start, &_binary_source_zip_end);

            httplib::MultipartFormDataItems formData = {
                    {"problemId",     std::to_string(solution.problem->id), "",              ""},
                    {"score",         std::to_string(score),                "",              ""},
                    {"target",        target,                               "",              ""},
                    {"solutionFile",  solutionBuffer.GetString(),           "solution.json", "application/json"},
                    {"sourceArchive", source,                               "source.zip",    "application/zip"}
            };

            auto response = server.Post("/submit", formData);
            if (response && response->status < 400) {
                rapidjson::Document responseData;
                responseData.Parse(response->body.c_str());

                bool newBest = responseData["new_best"].GetBool();
                if (newBest) {
                    std::cout << globalImprovement << std::endl;
                }

                globalScores[solution.problem->id] = responseData["best_score"].GetInt64();
            } else if (response) {
                std::cout << *solution.problem
                          << "Received HTTP "
                          << response->status
                          << " while submitting new global best"
                          << std::endl;
            } else {
                std::cout << *solution.problem
                          << "Something went wrong while submitting new global best: "
                          << httplib::to_string(response.error())
                          << std::endl;
            }
        }
    }

    bool isServerEnabled() const {
        return serverEnabled;
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
            long long score = pair.value.GetInt64();

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

    std::string isImprovement(const std::unordered_map<int, long long> &scores,
                              int problemId,
                              long long score,
                              const std::string &label) const {
        std::stringstream stream;

        auto thousandsSeparator = std::make_unique<ThousandsSeparator>();
        stream.imbue(std::locale(stream.getloc(), thousandsSeparator.release()));

        if (scores.contains(problemId)) {
            if (scores.at(problemId) >= score) {
                return stream.str();
            }

            stream << "[Problem " << problemId << "] Found new best " << label << " score: "
                   << scores.at(problemId) << " -> " << score;
            return stream.str();
        }

        stream << "[Problem " << problemId << "] Found new best " << label << " score: " << score;
        return stream.str();
    }
};
