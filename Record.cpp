#include "Record.h"
#include "Stats.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

// 全局做题记录定义
std::vector<Record> g_records;
std::unordered_map<int, std::vector<Record>> g_recordsByQuestion;
std::unordered_set<int> g_wrongQuestions;

// 当前用户 ID 定义
std::string g_currentUserId;

// 获取当前用户的记录文件路径
std::string getRecordFilePath() {
    if (g_currentUserId.empty()) {
        return "data/records.csv"; // 默认路径
    }
    return "data/records_" + g_currentUserId + ".csv";
}

// 用户登录：设置当前用户 ID
void loginUser(const std::string& userId) {
    g_currentUserId = userId;
    std::cout << "当前用户：" << g_currentUserId << std::endl;
}

// 清空当前用户的所有记录数据（用于切换用户）
void clearUserRecords() {
    g_records.clear();
    g_recordsByQuestion.clear();
    g_wrongQuestions.clear();
}

bool loadRecordsFromFile(const std::string& filename) {
    // 清空旧数据
    clearUserRecords();

    std::ifstream fin(filename);
    if (!fin.is_open()) {
        // 没有记录文件不算错误，可能是第一次使用
        std::cout << "未找到当前用户的做题记录文件，将从空记录开始。\n";
        return true;
    }

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() < 4) continue;

        Record r;
        try {
            r.questionId = std::stoi(fields[0]);
            r.correct = (fields[1] == "1");
            r.usedSeconds = std::stoi(fields[2]);
            r.timestamp = std::stoll(fields[3]);
        } catch (...) {
            continue;
        }

        g_records.push_back(r);
        g_recordsByQuestion[r.questionId].push_back(r);
    }

    // 根据"最后一次作答结果"构建当前错题集合
    for (auto& p : g_recordsByQuestion) {
        int qid = p.first;
        auto& vec = p.second;
        if (!vec.empty()) {
            const Record& last = vec.back();
            if (!last.correct) {
                g_wrongQuestions.insert(qid);
            }
        }
    }

    std::cout << "历史做题记录加载完成，共 " << g_records.size() << " 条，当前错题数 "
     << g_wrongQuestions.size() << " 道。\n";

    // 构建统计信息
    buildQuestionStats();

    return true;
}

void appendRecordToFile(const Record& r, const std::string& filename) {
    std::ofstream fout(filename, std::ios::app);
    if (!fout.is_open()) {
        std::cout << "警告：无法写入做题记录文件。\n";
        return;
    }
    fout << r.questionId << ','
         << (r.correct ? 1 : 0) << ','
         << r.usedSeconds << ','
         << r.timestamp << '\n';
}

void doQuestion(const Question& q) {
    using namespace std::chrono;

    std::cout << "题号: " << q.id << std::endl;
    std::cout << "[知识点] " << q.knowledge << "    [难度] " << q.difficulty << std::endl;
    std::cout << q.text << std::endl;
    for (size_t i = 0; i < q.options.size(); ++i) {
        std::cout << i << ". " << q.options[i] << std::endl;
    }

    std::cout << "请输入你的答案编号：";

    auto start = steady_clock::now();

    int userAns;
    std::cin >> userAns;

    auto end = steady_clock::now();
    int usedSeconds = (int)duration_cast<seconds>(end - start).count();
    if (usedSeconds <= 0) usedSeconds = 1;

    bool correct = (userAns == q.answer);
    if (correct) {
        std::cout << "回答正确！\n";
    } else {
        std::cout << "回答错误，正确答案是：" << q.answer
             << "，" << q.options[q.answer] << std::endl;
    }

    // 构建记录
    Record r;
    r.questionId = q.id;
    r.correct = correct;
    r.usedSeconds = usedSeconds;
    r.timestamp = (long long)std::time(nullptr);

    // 更新内存结构
    g_records.push_back(r);
    g_recordsByQuestion[q.id].push_back(r);

    // 更新错题集合：最后一次答对就从错题集中移除，答错就加入
    if (correct) {
        g_wrongQuestions.erase(q.id);
    } else {
        g_wrongQuestions.insert(q.id);
    }

    // 追加写入文件，使用当前用户的记录路径
    appendRecordToFile(r, getRecordFilePath());
}
