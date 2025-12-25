/**
 * @file Report.cpp
 * @brief 学习报告导出模块实现文件
 * @details 实现学习报告的生成、格式化和文件输出功能
 */

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

/**
 * @brief 获取当前时间字符串（用于文件名）
 * @return 格式化的时间字符串，格式为 YYYYMMDD_HHMM
 *
 * @details 时间格式说明：
 *          - YYYY：4位年份（例如：2023）
 *          - MM：2位月份，左补零（01-12）
 *          - DD：2位日期，左补零（01-31）
 *          - HH：2位小时，左补零，24小时制（00-23）
 *          - MM：2位分钟，左补零（00-59）
 *          - 示例：20231225_1430 表示 2023年12月25日 14:30
 *
 * @note 使用本地时间而非 UTC 时间
 * @note Windows 平台使用 localtime_s，POSIX 平台使用 localtime_r 确保线程安全
 * @note 该格式适合文件名，不包含特殊字符（冒号、空格等）
 *
 * @complexity 时间复杂度 O(1)
 * @complexity 空间复杂度 O(1)
 */
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

/**
 * @brief 获取当前时间字符串（用于报告显示）
 * @return 格式化的时间字符串，格式为 YYYY-MM-DD HH:MM:SS
 *
 * @details 时间格式说明：
 *          - YYYY：4位年份（例如：2023）
 *          - MM：2位月份，左补零（01-12）
 *          - DD：2位日期，左补零（01-31）
 *          - HH：2位小时，左补零，24小时制（00-23）
 *          - MM：2位分钟，左补零（00-59）
 *          - SS：2位秒数，左补零（00-59）
 *          - 示例：2023-12-25 14:30:45
 *
 * @note 使用本地时间而非 UTC 时间
 * @note Windows 平台使用 localtime_s，POSIX 平台使用 localtime_r 确保线程安全
 * @note 该格式适合用户阅读，包含完整的日期时间信息
 *
 * @complexity 时间复杂度 O(1)
 * @complexity 空间复杂度 O(1)
 */
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

/**
 * @brief 导出当前用户的学习报告
 * @details 该函数是报告生成的主函数，完成以下工作流程：
 *
 *          **报告结构：**
 *          1. 报告头部
 *             - 标题：数据结构智能刷题系统学习报告
 *             - 当前用户ID
 *             - 生成时间（YYYY-MM-DD HH:MM:SS 格式）
 *             - 数据来源文件路径
 *
 *          2. 总体概览
 *             - 总作答题数：所有做题记录数量
 *             - 答对题数：正确作答的记录数量
 *             - 答错题数：错误作答的记录数量
 *             - 总体正确率：(答对题数 / 总作答题数) × 100%
 *             - 当前错题数：错题本中的题目数量
 *
 *          3. 按知识点统计
 *             - 遍历所有做题记录，按知识点分类统计
 *             - 统计内容：作答次数、答对次数、正确率
 *             - 以表格形式展示各知识点的掌握情况
 *
 *          4. 错题分布
 *             - 按知识点统计错题数：各知识点的错题数量分布
 *             - 按难度统计错题数：不同难度等级的错题数量分布
 *
 *          5. 复习建议
 *             - 薄弱知识点识别：标记正确率低于 60% 的知识点
 *             - 错题本练习建议：提示使用错题本功能
 *             - 智能推荐功能介绍：AI推荐、知识点路径、模拟考试等
 *
 *          **文件命名规则：**
 *          - 目录：reports/
 *          - 文件名格式：report_{用户ID}_{时间戳}.md
 *          - 时间戳格式：YYYYMMDD_HHMM（例如：report_student01_20231225_1430.md）
 *
 *          **目录创建：**
 *          - 自动创建 reports/ 目录（如果不存在）
 *          - 使用 std::filesystem::create_directories 递归创建
 *          - 如果创建失败则提示错误信息并返回
 *
 *          **时间格式化：**
 *          - 文件名时间：YYYYMMDD_HHMM（无特殊字符，适合文件系统）
 *          - 显示时间：YYYY-MM-DD HH:MM:SS（易读格式）
 *
 * @complexity 时间复杂度 O(M)，其中 M 为做题记录数量
 *             - 第一次遍历 g_records：统计总体情况和按知识点统计（O(M)）
 *             - 遍历 g_wrongQuestions：统计错题分布（O(W)，W ≤ M）
 *             - 遍历 knowledgeStats：查找薄弱知识点（O(K)，K << M）
 *             - 总体复杂度由第一次遍历决定：O(M)
 *
 * @complexity 空间复杂度 O(K + D + R)
 *             - knowledgeStats 映射表：O(K)，K 为知识点数量
 *             - wrongByKnowledge 映射表：O(K)
 *             - wrongByDifficulty 映射表：O(D)，D 为难度等级数量（通常为常数）
 *             - report 字符串流：O(R)，R 为报告内容大小
 *             - 主要开销在于 knowledgeStats 和 report 字符串
 *
 * @note 依赖全局变量：
 *       - g_currentUserId：当前登录用户ID
 *       - g_records：所有做题记录
 *       - g_wrongQuestions：错题集合
 *       - g_questionById：题目ID到题目对象的映射
 *
 * @note 文件输出：
 *       - 使用 Markdown 格式，便于阅读和转换
 *       - 使用 std::ofstream 写入文件
 *       - 输出成功后显示文件路径和生成信息
 *
 * @see getTimeStringForFilename() 生成文件名用时间戳
 * @see getTimeStringForDisplay() 生成显示用时间字符串
 * @see getReportsDir() 获取报告目录路径
 * @see pauseForUser() 等待用户确认
 */
void exportLearningReport() {
    std::ostringstream report;

    // ========================================
    // 1. 报告头部
    // ========================================
    report << "# 学习报告（数据结构智能刷题系统）\n\n";
    report << "---\n\n";
    report << "**当前用户**：" << g_currentUserId << "\n\n";
    report << "**生成时间**：" << getTimeStringForDisplay() << "\n\n";
    report << "**数据来源**：`data/records_" << g_currentUserId << ".csv`\n\n";
    report << "---\n\n";

    // ========================================
    // 2. 总体统计
    // ========================================
    // 遍历所有做题记录，统计总题数和正确题数
    int totalRecords = (int)g_records.size();
    int correctRecords = 0;
    for (const auto& r : g_records) {
        if (r.correct) correctRecords++;
    }

    report << "## 一、总体概览\n\n";

    if (totalRecords == 0) {
        // 如果没有做题记录，提示用户
        report << "> 当前用户尚无做题记录，暂无法生成详细报告。\n\n";
    } else {
        // 计算总体正确率
        double accuracy = correctRecords * 100.0 / totalRecords;

        report << "| 统计项 | 数值 |\n";
        report << "|--------|------|\n";
        report << "| 总作答题数 | " << totalRecords << " |\n";
        report << "| 答对题数 | " << correctRecords << " |\n";
        report << "| 答错题数 | " << (totalRecords - correctRecords) << " |\n";
        report << "| 总体正确率 | " << std::fixed << std::setprecision(1) << accuracy << "% |\n";
        report << "| 当前错题数 | " << g_wrongQuestions.size() << " |\n\n";
    }

    // ========================================
    // 3. 按知识点统计
    // ========================================
    if (totalRecords > 0) {
        report << "## 二、按知识点统计\n\n";

        // 使用 map 统计每个知识点的作答情况
        // key: 知识点名称, value: {总作答次数, 正确作答次数}
        std::map<std::string, std::pair<int, int>> knowledgeStats;

        // 遍历所有做题记录，按知识点累计统计
        for (const auto& r : g_records) {
            auto itQ = g_questionById.find(r.questionId);
            if (itQ == g_questionById.end()) continue;  // 题目不存在，跳过

            size_t qIdx = itQ->second;
            if (qIdx >= g_questions.size()) continue;  // 索引异常，跳过

            const std::string& knowledge = g_questions[qIdx].knowledge;
            knowledgeStats[knowledge].first++;  // 该知识点总作答次数 +1
            if (r.correct) {
                knowledgeStats[knowledge].second++;  // 该知识点正确作答次数 +1
            }
        }

        // 生成知识点统计表格
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

        // ========================================
        // 4. 错题分布统计
        // ========================================
        if (!g_wrongQuestions.empty()) {
            report << "## 三、错题分布（简要）\n\n";

            // 使用 map 统计错题分布
            // key: 知识点名称, value: 该知识点的错题数量
            std::map<std::string, int> wrongByKnowledge;
            // key: 难度等级, value: 该难度的错题数量
            std::map<int, int> wrongByDifficulty;

            // 遍历错题集合，按知识点和难度分类统计
            for (int qid : g_wrongQuestions) {
                auto itQ = g_questionById.find(qid);
                if (itQ == g_questionById.end()) continue;  // 题目不存在，跳过

                size_t qIdx = itQ->second;
                if (qIdx >= g_questions.size()) continue;  // 索引异常，跳过

                const Question& q = g_questions[qIdx];
                wrongByKnowledge[q.knowledge]++;     // 该知识点错题数 +1
                wrongByDifficulty[q.difficulty]++;   // 该难度错题数 +1
            }

            // 生成按知识点统计错题数表格
            report << "### 3.1 按知识点统计错题数\n\n";
            report << "| 知识点 | 错题数 |\n";
            report << "|--------|--------|\n";
            for (const auto& pair : wrongByKnowledge) {
                report << "| " << pair.first << " | " << pair.second << " |\n";
            }
            report << "\n";

            // 生成按难度统计错题数表格
            report << "### 3.2 按难度统计错题数\n\n";
            report << "| 难度等级 | 错题数 |\n";
            report << "|----------|--------|\n";
            for (const auto& pair : wrongByDifficulty) {
                report << "| " << pair.first << " | " << pair.second << " |\n";
            }
            report << "\n";
        }

        // ========================================
        // 5. 复习建议
        // ========================================
        report << "## 四、复习建议（基于当前数据）\n\n";

        // 识别薄弱知识点：找出正确率低于 60% 的知识点
        std::vector<std::pair<std::string, double>> weakKnowledge;
        for (const auto& pair : knowledgeStats) {
            const std::string& knowledge = pair.first;
            int total = pair.second.first;
            int correct = pair.second.second;
            double acc = (total > 0) ? (correct * 100.0 / total) : 0.0;

            // 如果该知识点正确率低于 60%，加入薄弱知识点列表
            if (total > 0 && acc < 60.0) {
                weakKnowledge.push_back({knowledge, acc});
            }
        }

        // 输出薄弱知识点列表
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

        // 错题本练习建议
        if (!g_wrongQuestions.empty()) {
            report << "### 错题本练习建议\n\n";
            report << "当前共有 **" << g_wrongQuestions.size() << "** 道错题，建议：\n\n";
            report << "1. 使用系统的\"错题本练习\"功能进行针对性复习\n";
            report << "2. 对于反复出错的题目，可以查阅相关知识点的教材或资料\n";
            report << "3. 利用\"知识点复习路径推荐\"功能，系统学习相关前置知识\n\n";
        }

        // AI 推荐功能介绍
        report << "### 智能推荐功能\n\n";
        report << "系统提供以下功能帮助你高效复习：\n\n";
        report << "- **AI 智能推荐**：基于错误率、时间间隔和难度的多维度推荐算法\n";
        report << "- **知识点复习路径推荐**：基于知识点依赖图生成个性化学习路径\n";
        report << "- **模拟考试模式**：全面检测学习成果\n\n";
    }

    // ========================================
    // 6. 报告结尾
    // ========================================
    report << "---\n\n";
    report << "*本报告由数据结构智能刷题系统自动生成*\n";

    // ========================================
    // 7. 文件输出
    // ========================================

    // 创建 reports 目录（如果不存在）
    // 使用 std::filesystem::create_directories 可以递归创建多级目录
    std::filesystem::path reportsDir = getReportsDir();
    try {
        std::filesystem::create_directories(reportsDir);
    } catch (const std::exception& e) {
        std::cout << "创建 reports 目录失败：" << e.what() << "\n";
        pauseForUser();
        return;
    }

    // 生成文件名：report_{用户ID}_{时间戳}.md
    // 例如：report_student01_20231225_1430.md
    std::filesystem::path filepath = reportsDir / ("report_" + g_currentUserId + "_" + getTimeStringForFilename() + ".md");
    std::string filename = filepath.string();

    // 将报告内容写入文件
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        std::cout << "无法创建报告文件：" << filename << "\n";
        pauseForUser();
        return;
    }

    fout << report.str();
    fout.close();

    // ========================================
    // 8. 输出成功信息
    // ========================================
    std::cout << "\n========================================\n";
    std::cout << "学习报告导出成功！\n";
    std::cout << "========================================\n";
    std::cout << "文件路径：" << filename << "\n";
    std::cout << "用户：" << g_currentUserId << "\n";
    std::cout << "生成时间：" << getTimeStringForDisplay() << "\n";
    std::cout << "========================================\n";

    pauseForUser();
}
