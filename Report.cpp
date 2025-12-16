#include "Report.h"
#include "Record.h"
#include "Question.h"
#include "Stats.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <map>

// 获取当前时间字符串（用于文件名）：格式 YYYYMMDD_HHMM
std::string getTimeStringForFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm local_time_buf;
#ifdef _WIN32
    localtime_s(&local_time_buf, &now_c);
#else
    localtime_r(&now_c, &local_time_buf);
#endif
    std::tm* local_time = &local_time_buf;

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (local_time->tm_year + 1900)
        << std::setw(2) << (local_time->tm_mon + 1)
        << std::setw(2) << local_time->tm_mday
        << "_"
        << std::setw(2) << local_time->tm_hour
        << std::setw(2) << local_time->tm_min;
    return oss.str();
}

// 获取当前时间字符串（用于报告显示）：格式 YYYY-MM-DD HH:MM:SS
std::string getTimeStringForDisplay() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm local_time_buf;
#ifdef _WIN32
    localtime_s(&local_time_buf, &now_c);
#else
    localtime_r(&now_c, &local_time_buf);
#endif
    std::tm* local_time = &local_time_buf;

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (local_time->tm_year + 1900) << "-"
        << std::setw(2) << (local_time->tm_mon + 1) << "-"
        << std::setw(2) << local_time->tm_mday << " "
        << std::setw(2) << local_time->tm_hour << ":"
        << std::setw(2) << local_time->tm_min << ":"
        << std::setw(2) << local_time->tm_sec;
    return oss.str();
}

void exportLearningReport() {
    std::ostringstream report;

    // 1. 报告头部
    report << "# 学习报告（数据结构智能刷题系统）\n\n";
    report << "---\n\n";
    report << "**当前用户**：" << g_currentUserId << "\n\n";
    report << "**生成时间**：" << getTimeStringForDisplay() << "\n\n";
    report << "**数据来源**：`data/records_" << g_currentUserId << ".csv`\n\n";
    report << "---\n\n";

    // 2. 总体统计
    int totalRecords = (int)g_records.size();
    int correctRecords = 0;
    for (const auto& r : g_records) {
        if (r.correct) correctRecords++;
    }

    report << "## 一、总体概览\n\n";

    if (totalRecords == 0) {
        report << "> 当前用户尚无做题记录，暂无法生成详细报告。\n\n";
    } else {
        double accuracy = correctRecords * 100.0 / totalRecords;

        report << "| 统计项 | 数值 |\n";
        report << "|--------|------|\n";
        report << "| 总作答题数 | " << totalRecords << " |\n";
        report << "| 答对题数 | " << correctRecords << " |\n";
        report << "| 答错题数 | " << (totalRecords - correctRecords) << " |\n";
        report << "| 总体正确率 | " << std::fixed << std::setprecision(1) << accuracy << "% |\n";
        report << "| 当前错题数 | " << g_wrongQuestions.size() << " |\n\n";
    }

    // 3. 按知识点统计
    if (totalRecords > 0) {
        report << "## 二、按知识点统计\n\n";

        // 统计每个知识点的作答情况
        std::map<std::string, std::pair<int, int>> knowledgeStats; // {知识点: {总次数, 正确次数}}

        for (const auto& r : g_records) {
            auto itQ = g_questionById.find(r.questionId);
            if (itQ == g_questionById.end()) continue;

            const std::string& knowledge = itQ->second->knowledge;
            knowledgeStats[knowledge].first++;  // 总次数
            if (r.correct) {
                knowledgeStats[knowledge].second++;  // 正确次数
            }
        }

        report << "| 知识点 | 作答次数 | 答对次数 | 正确率 |\n";
        report << "|--------|----------|----------|--------|\n";

        for (const auto& pair : knowledgeStats) {
            const std::string& knowledge = pair.first;
            int total = pair.second.first;
            int correct = pair.second.second;
            double acc = (total > 0) ? (correct * 100.0 / total) : 0.0;

            report << "| " << knowledge << " | " << total << " | " << correct
                   << " | " << std::fixed << std::setprecision(1) << acc << "% |\n";
        }
        report << "\n";

        // 4. 错题分布统计
        if (!g_wrongQuestions.empty()) {
            report << "## 三、错题分布（简要）\n\n";

            // 按知识点统计错题数
            std::map<std::string, int> wrongByKnowledge;
            // 按难度统计错题数
            std::map<int, int> wrongByDifficulty;

            for (int qid : g_wrongQuestions) {
                auto itQ = g_questionById.find(qid);
                if (itQ == g_questionById.end()) continue;

                const Question* q = itQ->second;
                wrongByKnowledge[q->knowledge]++;
                wrongByDifficulty[q->difficulty]++;
            }

            report << "### 3.1 按知识点统计错题数\n\n";
            report << "| 知识点 | 错题数 |\n";
            report << "|--------|--------|\n";
            for (const auto& pair : wrongByKnowledge) {
                report << "| " << pair.first << " | " << pair.second << " |\n";
            }
            report << "\n";

            report << "### 3.2 按难度统计错题数\n\n";
            report << "| 难度等级 | 错题数 |\n";
            report << "|----------|--------|\n";
            for (const auto& pair : wrongByDifficulty) {
                report << "| " << pair.first << " | " << pair.second << " |\n";
            }
            report << "\n";
        }

        // 5. 复习建议
        report << "## 四、复习建议（基于当前数据）\n\n";

        // 找出正确率低于 60% 的知识点
        std::vector<std::pair<std::string, double>> weakKnowledge;
        for (const auto& pair : knowledgeStats) {
            const std::string& knowledge = pair.first;
            int total = pair.second.first;
            int correct = pair.second.second;
            double acc = (total > 0) ? (correct * 100.0 / total) : 0.0;

            if (total > 0 && acc < 60.0) {
                weakKnowledge.push_back({knowledge, acc});
            }
        }

        if (!weakKnowledge.empty()) {
            report << "### 薄弱知识点\n\n";
            report << "以下知识点正确率较低（< 60%），建议重点复习：\n\n";
            for (const auto& pair : weakKnowledge) {
                report << "- **" << pair.first << "**：正确率 "
                       << std::fixed << std::setprecision(1) << pair.second << "%\n";
            }
            report << "\n";
        } else {
            report << "恭喜！所有已练习的知识点正确率均达到 60% 以上，继续保持！\n\n";
        }

        // 错题本建议
        if (!g_wrongQuestions.empty()) {
            report << "### 错题本练习建议\n\n";
            report << "当前共有 **" << g_wrongQuestions.size() << "** 道错题，建议：\n\n";
            report << "1. 使用系统的\"错题本练习\"功能进行针对性复习\n";
            report << "2. 对于反复出错的题目，可以查阅相关知识点的教材或资料\n";
            report << "3. 利用\"知识点复习路径推荐\"功能，系统学习相关前置知识\n\n";
        }

        // AI 推荐建议
        report << "### 智能推荐功能\n\n";
        report << "系统提供以下功能帮助你高效复习：\n\n";
        report << "- **AI 智能推荐**：基于错误率、时间间隔和难度的多维度推荐算法\n";
        report << "- **知识点复习路径推荐**：基于知识点依赖图生成个性化学习路径\n";
        report << "- **模拟考试模式**：全面检测学习成果\n\n";
    }

    // 6. 报告结尾
    report << "---\n\n";
    report << "*本报告由数据结构智能刷题系统自动生成*\n";

    // 创建 reports 目录（如果不存在）
    std::filesystem::path reportsDir = getReportsDir();
    try {
        std::filesystem::create_directories(reportsDir);
    } catch (const std::exception& e) {
        std::cout << "创建 reports 目录失败：" << e.what() << "\n";
        pauseForUser();
        return;
    }

    // 生成文件名
    std::filesystem::path filepath = reportsDir / ("report_" + g_currentUserId + "_" + getTimeStringForFilename() + ".md");
    std::string filename = filepath.string();

    // 写入文件
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        std::cout << "无法创建报告文件：" << filename << "\n";
        pauseForUser();
        return;
    }

    fout << report.str();
    fout.close();

    std::cout << "\n========================================\n";
    std::cout << "学习报告导出成功！\n";
    std::cout << "========================================\n";
    std::cout << "文件路径：" << filename << "\n";
    std::cout << "用户：" << g_currentUserId << "\n";
    std::cout << "生成时间：" << getTimeStringForDisplay() << "\n";
    std::cout << "========================================\n";

    pauseForUser();
}
