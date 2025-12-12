#include "Stats.h"
#include "Record.h"
#include "Question.h"
#include "Utils.h"
#include <iostream>

// 全局统计信息定义
std::unordered_map<int, QuestionStat> g_questionStats;

void buildQuestionStats() {
    g_questionStats.clear();
    for (const auto& p : g_recordsByQuestion) {
        int qid = p.first;
        const auto& vec = p.second;
        QuestionStat st;
        for (const auto& r : vec) {
            st.totalAttempts++;
            if (r.correct) st.correctAttempts++;
            st.totalTime += r.usedSeconds;
            if (r.timestamp > st.lastTimestamp) {
                st.lastTimestamp = r.timestamp;
            }
        }
        g_questionStats[qid] = st;
    }
}

std::unordered_map<std::string, KnowledgeStat> buildKnowledgeStats() {
    std::unordered_map<std::string, KnowledgeStat> ks;

    // 根据每条记录找到题目对应的知识点
    for (const auto& r : g_records) {
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue;
        const std::string& kd = itQ->second->knowledge;
        ks[kd].total++;
        if (r.correct) ks[kd].correct++;
    }

    // 计算正确率
    for (auto& p : ks) {
        if (p.second.total > 0) {
            p.second.accuracy = p.second.correct * 100.0 / p.second.total;
        }
    }

    return ks;
}

void showStatistics() {
    if (g_records.empty()) {
        std::cout << "当前还没有任何做题记录。\n";
        pauseForUser();
        return;
    }

    // 总体统计
    int total = (int)g_records.size();
    int correct = 0;
    for (const auto& r : g_records) {
        if (r.correct) correct++;
    }
    double acc = correct * 1.0 / total * 100.0;

    std::cout << "===== 总体统计 =====\n";
    std::cout << "总作答题数： " << total << "\n";
    std::cout << "答对题数：   " << correct << "\n";
    std::cout << "总体正确率： " << acc << "%\n\n";

    // 知识点统计
    std::cout << "===== 按知识点统计 =====\n";

    std::unordered_map<std::string, KnowledgeStat> ks;

    // 根据每条记录找到题目对应的知识点
    for (const auto& r : g_records) {
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue;
        const std::string& kd = itQ->second->knowledge;
        ks[kd].total++;
        if (r.correct) ks[kd].correct++;
    }

    for (const auto& p : ks) {
        const std::string& kd = p.first;
        const KnowledgeStat& s = p.second;
        double kacc = s.correct * 1.0 / s.total * 100.0;
        std::cout << "[知识点] " << kd
             << "  总题数: " << s.total
             << "  正确数: " << s.correct
             << "  正确率: " << kacc << "%\n";
    }

    std::cout << "\n当前错题数： " << g_wrongQuestions.size() << " 道。\n";

    // 统计显示后暂停
    pauseForUser();
}
