#include <memory>
#include <string>
#include <vector>

#include <core/models.h>

#include <gtest/gtest.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

class SolutionFixture : public ::testing::Test {
protected:
    std::shared_ptr<Problem> createExampleProblem() const {
        int id = 1;
        Area room({0, 0}, 2000, 5000);
        Area stage({500, 0}, 1000, 200);
        std::vector<int> musicians{0, 1, 0};
        std::vector<Attendee> attendees{
                {{100,  500},  {1000, -1000}},
                {{200,  1000}, {200,  200}},
                {{1100, 800},  {800,  1500}}
        };
        std::vector<Pillar> pillars;

        return std::make_shared<Problem>(id, room, stage, musicians, attendees, pillars);
    }

    Solution createExampleSolution() const {
        auto problem = createExampleProblem();
        std::vector<Point> placements{
                {590,  10},
                {1100, 100},
                {1100, 150}
        };
        std::vector<double> volumes{1, 1, 1};

        return {problem, placements, volumes};
    }
};

TEST_F(SolutionFixture, IsValidExample) {
    auto solution = createExampleSolution();

    EXPECT_TRUE(solution.isValid());
}

TEST_F(SolutionFixture, IsValidFalseWhenMoreOrLessPlacementsThanMusicians) {
    auto solution = createExampleSolution();

    EXPECT_TRUE(solution.isValid());
    solution.placements.pop_back();
    EXPECT_FALSE(solution.isValid());
    solution.placements.emplace_back(1100, 150);
    EXPECT_TRUE(solution.isValid());
    solution.placements.emplace_back(1100, 200);
    EXPECT_FALSE(solution.isValid());
}

TEST_F(SolutionFixture, IsValidFalseWhenMusicianOutsideStage) {
    auto solution = createExampleSolution();

    solution.placements.pop_back();
    solution.placements.emplace_back(499.9, 0.0);
    EXPECT_FALSE(solution.isValid());
}

TEST_F(SolutionFixture, IsValidFalseWhenMusicianTooCloseToOtherMusician) {
    auto solution = createExampleSolution();

    solution.placements.pop_back();
    solution.placements.emplace_back(580.1, 10.0);
    EXPECT_FALSE(solution.isValid());

    solution.placements.pop_back();
    solution.placements.emplace_back(580.0, 10.0);
    EXPECT_TRUE(solution.isValid());
}

TEST_F(SolutionFixture, GetScoreExampleAuto) {
    auto solution = createExampleSolution();

    EXPECT_EQ(solution.getScore(ScoreType::AUTO, false), 5343);
}

TEST_F(SolutionFixture, GetScoreExampleLightning) {
    auto solution = createExampleSolution();

    EXPECT_EQ(solution.getScore(ScoreType::LIGHTNING, false), 5343);
}

TEST_F(SolutionFixture, GetScoreExampleFull) {
    auto solution = createExampleSolution();

    EXPECT_EQ(solution.getScore(ScoreType::FULL, false), 5357);
}

TEST_F(SolutionFixture, GetScoreExtendedExample) {
    int id = 1;
    Area room({0, 0}, 2000, 5000);
    Area stage({500, 0}, 1000, 200);
    std::vector<int> musicians{0, 1, 0};
    std::vector<Attendee> attendees{
            {{100,  500},  {1000, -1000}},
            {{200,  1000}, {200,  200}},
            {{1100, 800},  {800,  1500}}
    };
    std::vector<Pillar> pillars{
            {{345, 255}, 4}
    };

    auto problem = std::make_shared<Problem>(id, room, stage, musicians, attendees, pillars);
    std::vector<Point> placements{
            {590,  10},
            {1100, 100},
            {1100, 150}
    };
    std::vector<double> volumes{1, 1, 1};

    Solution solution(problem, placements, volumes);

    EXPECT_EQ(solution.getScore(ScoreType::FULL, false), 3270);
}

TEST_F(SolutionFixture, ToJsonExample) {
    auto solution = createExampleSolution();

    auto jsonDoc = solution.toJson();

    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(jsonBuffer);
    jsonDoc.Accept(jsonWriter);
    std::string json = jsonBuffer.GetString();

    EXPECT_EQ(json,
              "{\"placements\":[{\"x\":590.0,\"y\":10.0},{\"x\":1100.0,\"y\":100.0},{\"x\":1100.0,\"y\":150.0}],\"volumes\":[1.0,1.0,1.0]}");
}

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
