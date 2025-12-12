#include "App.h"
#include "Question.h"
#include "Record.h"
#include "Stats.h"
#include "Recommender.h"
#include "KnowledgeGraph.h"
#include "Report.h"
#include "Utils.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <algorithm>

void showMenu() {
    std::cout << "=============================\n";
    std::cout << " 数据结构智能刷题与错题推荐系统\n";
    std::cout << "=============================\n";
    std::cout << "1. 随机刷题\n";
    std::cout << "2. 错题本练习\n";
    std::cout << "3. AI 智能推荐练习\n";
    std::cout << "4. 做题统计查看\n";
    std::cout << "5. 模拟考试模式\n";
    std::cout << "6. 知识点复习路径推荐\n";
    std::cout << "7. 导出学习报告\n";
    std::cout << "8. 切换用户\n";
    std::cout << "0. 退出\n";
    std::cout << "请选择：";
}

void randomPractice() {
    if (g_questions.empty()) {
        std::cout << "题库为空，请先导入题库。\n";
        pauseForUser();
        return;
    }

    std::srand((unsigned)std::time(nullptr));
    int idx = std::rand() % g_questions.size();
    const Question& q = g_questions[idx];

    doQuestion(q);

    // 答题结束后暂停
    pauseForUser();
}

void wrongBookMode() {
    if (g_wrongQuestions.empty()) {
        std::cout << "当前没有错题，先去刷题或者做几道题再来吧。\n";
        pauseForUser();
        return;
    }

    // 把错题号拷贝到 vector 里便于随机
    std::vector<int> ids;
    ids.reserve(g_wrongQuestions.size());
    for (int id : g_wrongQuestions) {
        ids.push_back(id);
    }

    std::srand((unsigned)std::time(nullptr));
    int idx = std::rand() % ids.size();
    int qid = ids[idx];

    auto it = g_questionById.find(qid);
    if (it == g_questionById.end()) {
        std::cout << "错误：找不到错题对应的题目内容。\n";
        pauseForUser();
        return;
    }

    std::cout << "【错题本练习】\n";
    doQuestion(*it->second);

    // 答题结束后暂停
    pauseForUser();
}

void examMode() {
    if (g_questions.empty()) {
        std::cout << "题库为空，无法进行考试。\n";
        pauseForUser();
        return;
    }

    // 步骤 1：让用户输入考试题目数量
    std::cout << "========== 模拟考试模式 ==========\n";
    std::cout << "题库共有 " << g_questions.size() << " 道题。\n";
    std::cout << "请输入本次考试的题目数量：";

    int N;
    std::cin >> N;

    // 限制范围
    if (N < 1) N = 1;
    if (N > (int)g_questions.size()) N = (int)g_questions.size();

    std::cout << "本次考试共 " << N << " 道题。\n";

    // 步骤 2（可选）：简单提示时间限制
    std::cout << "提示：本次考试不设置强制时间限制，请按自己的节奏完成。\n";
    std::cout << "（系统会记录每道题的作答时间）\n\n";

    // 步骤 3：随机抽取 N 道互不重复的题目
    std::vector<int> indices;
    indices.reserve(g_questions.size());
    for (size_t i = 0; i < g_questions.size(); ++i) {
        indices.push_back(i);
    }

    // 使用 C++11 的 random_device 和 mt19937 进行洗牌
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(indices.begin(), indices.end(), rng);

    // 取前 N 个
    std::vector<int> selectedIndices(indices.begin(), indices.begin() + N);

    // 步骤 4：记录考试开始前的记录数
    size_t prevSize = g_records.size();

    std::cout << "考试开始！\n";
    std::cout << "====================================\n\n";

    // 顺序出题
    for (int i = 0; i < N; ++i) {
        std::cout << "【第 " << (i + 1) << "/" << N << " 题】\n";
        const Question& q = g_questions[selectedIndices[i]];
        doQuestion(q);
        std::cout << "\n";
    }

    std::cout << "====================================\n";
    std::cout << "考试结束！正在统计成绩...\n\n";

    // 步骤 5：统计本次考试结果
    int totalExam = (int)(g_records.size() - prevSize);
    int correctExam = 0;

    for (size_t i = prevSize; i < g_records.size(); ++i) {
        if (g_records[i].correct) {
            correctExam++;
        }
    }

    double examAcc = 0.0;
    if (totalExam > 0) {
        examAcc = correctExam * 100.0 / totalExam;
    }

    // 步骤 6：按知识点统计本次考试表现
    struct KnowledgeStat {
        int total = 0;
        int correct = 0;
    };
    std::unordered_map<std::string, KnowledgeStat> examKnowledge;

    for (size_t i = prevSize; i < g_records.size(); ++i) {
        const Record& r = g_records[i];
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue;

        const std::string& kd = itQ->second->knowledge;
        examKnowledge[kd].total++;
        if (r.correct) {
            examKnowledge[kd].correct++;
        }
    }

    // 步骤 7：打印考试报告
    std::cout << "========== 考试成绩报告 ==========\n";
    std::cout << "本次考试题目数： " << totalExam << "\n";
    std::cout << "答对题数：       " << correctExam << "\n";
    std::cout << "答错题数：       " << (totalExam - correctExam) << "\n";
    std::cout << "本次正确率：     " << examAcc << "%\n\n";

    std::cout << "===== 按知识点统计 =====\n";
    for (const auto& p : examKnowledge) {
        const std::string& kd = p.first;
        const KnowledgeStat& s = p.second;
        double kacc = 0.0;
        if (s.total > 0) {
            kacc = s.correct * 100.0 / s.total;
        }
        std::cout << "[知识点] " << kd
             << "  题数: " << s.total
             << "  正确: " << s.correct
             << "  正确率: " << kacc << "%\n";
    }

    std::cout << "====================================\n";
    std::cout << "考试记录已保存，可在\"做题统计查看\"中查看历史数据。\n";

    // 考试结束后暂停
    pauseForUser();
}

void switchUser() {
    std::cout << "\n========== 切换用户 ==========\n";
    std::cout << "请输入新的学号或用户名：";

    std::string userId;
    std::cin.ignore(); // 清除输入缓冲区
    while (true) {
        std::getline(std::cin, userId);
        // 去除前后空格
        userId.erase(0, userId.find_first_not_of(" \t\n\r"));
        userId.erase(userId.find_last_not_of(" \t\n\r") + 1);

        if (!userId.empty()) {
            break;
        }
        std::cout << "用户标识不能为空，请重新输入：";
    }

    // 切换用户
    loginUser(userId);

    // 重新加载新用户的做题记录
    loadRecordsFromFile(getRecordFilePath());

    std::cout << "已成功切换到用户：" << g_currentUserId << "\n";
    std::cout << "====================================\n";

    // 切换用户后暂停
    pauseForUser();
}

void runMenuLoop() {
    while (true) {
        // 每次循环开始时清屏
        clearScreen();

        showMenu();
        int choice;
        if (!(std::cin >> choice)) {
            break;
        }
        if (choice == 0) {
            std::cout << "再见！\n";
            break;
        } else if (choice == 1) {
            randomPractice();
        } else if (choice == 2) {
            wrongBookMode();
        } else if (choice == 3) {
            aiRecommendMode();
        } else if (choice == 4) {
            showStatistics();
        } else if (choice == 5) {
            examMode();
        } else if (choice == 6) {
            recommendReviewPath();
        } else if (choice == 7) {
            exportLearningReport();
        } else if (choice == 8) {
            switchUser();
        } else {
            std::cout << "无效选项，请重新输入。\n";
            pauseForUser();
        }
    }
}
