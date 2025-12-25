/**
 * @file Stats.cpp
 * @brief 统计模块实现文件
 *
 * 实现要点：
 * 1. **题目统计聚合**：遍历 g_recordsByQuestion，对每道题的所有记录进行累加统计
 *    - 累计作答次数、答对次数、总用时
 *    - 追踪最近作答时间戳（用于复习推荐）
 *    - 时间复杂度：O(M)，其中 M 为总记录数
 *
 * 2. **知识点统计聚合**：遍历 g_records，根据题目 ID 查找所属知识点
 *    - 使用哈希表累加每个知识点的总题数和答对数
 *    - 最终计算各知识点的正确率（correct / total * 100）
 *    - 时间复杂度：O(M)，查找题目为 O(1)
 *
 * 3. **统计展示**：分两部分展示
 *    - 总体统计：遍历所有记录计算总正确率
 *    - 知识点统计：实时构建知识点哈希表并展示
 *    - 时间复杂度：O(M)
 */

#include "Stats.h"
#include "Record.h"
#include "Question.h"
#include "Utils.h"
#include <iostream>

// 全局统计信息定义
std::unordered_map<int, QuestionStat> g_questionStats;

/**
 * @brief 从做题记录构建题目统计信息
 *
 * 实现逻辑：
 * 1. 清空旧的统计数据
 * 2. 遍历 g_recordsByQuestion（按题目 ID 组织的记录）
 * 3. 对每道题目的所有记录进行聚合：
 *    - 累加总作答次数
 *    - 累加答对次数（通过 r.correct 判断）
 *    - 累加总用时
 *    - 更新最近作答时间（取最大时间戳）
 * 4. 将聚合结果存入 g_questionStats
 *
 * @complexity O(M)，其中 M 为总记录数
 */
void buildQuestionStats() {
    // 清空旧数据，准备重建
    g_questionStats.clear();

    // 遍历按题目分组的记录
    for (const auto& p : g_recordsByQuestion) {
        int qid = p.first;                 // 题目 ID
        const auto& vec = p.second;        // 该题的所有作答记录

        QuestionStat st;                   // 初始化统计结构

        // 遍历该题的所有作答记录，进行聚合
        for (const auto& r : vec) {
            st.totalAttempts++;            // 累加作答次数
            if (r.correct) st.correctAttempts++;  // 累加答对次数
            st.totalTime += r.usedSeconds; // 累加总用时

            // 更新最近作答时间（保留最大时间戳）
            if (r.timestamp > st.lastTimestamp) {
                st.lastTimestamp = r.timestamp;
            }
        }

        // 将该题的统计信息存入全局映射表
        g_questionStats[qid] = st;
    }
}

/**
 * @brief 构建知识点统计信息
 *
 * 实现逻辑：
 * 1. 创建临时哈希表用于累加各知识点的统计数据
 * 2. 遍历所有做题记录 g_records：
 *    - 通过记录中的题目 ID 查找题目对象
 *    - 获取题目所属的知识点
 *    - 累加该知识点的总题数和答对题数
 * 3. 第二次遍历：计算每个知识点的正确率（百分比形式）
 *    - accuracy = correct / total * 100.0
 * 4. 返回构建好的知识点统计映射表
 *
 * @return 知识点统计映射表（知识点名称 -> 统计信息）
 * @complexity O(M)，其中 M 为总记录数，查找题目为 O(1) 哈希查找
 */
std::unordered_map<std::string, KnowledgeStat> buildKnowledgeStats() {
    // 初始化知识点统计哈希表
    std::unordered_map<std::string, KnowledgeStat> ks;

    // 第一步：遍历所有记录，累加各知识点的题数和答对数
    for (const auto& r : g_records) {
        // 通过题目 ID 查找题目索引（O(1) 哈希查找）
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue; // 题目不存在则跳过

        size_t qIdx = itQ->second;
        if (qIdx >= g_questions.size()) continue; // 索引异常则跳过

        // 获取题目所属的知识点
        const std::string& kd = g_questions[qIdx].knowledge;

        // 累加该知识点的统计数据
        ks[kd].total++;                      // 总题数 +1
        if (r.correct) ks[kd].correct++;     // 如果答对，答对数 +1
    }

    // 第二步：计算各知识点的正确率
    for (auto& p : ks) {
        if (p.second.total > 0) {
            // 正确率 = 答对数 / 总题数 * 100（转为百分比）
            p.second.accuracy = p.second.correct * 100.0 / p.second.total;
        }
    }

    // 返回构建好的知识点统计映射表
    return ks;
}

/**
 * @brief 显示统计信息（交互式展示）
 *
 * 实现逻辑：
 * 1. 检查是否有做题记录，无记录则提示并返回
 * 2. 总体统计：
 *    - 遍历 g_records 计算总题数和答对题数
 *    - 计算总体正确率并输出
 * 3. 知识点统计：
 *    - 再次遍历 g_records，按知识点累加题数和答对数
 *    - 计算各知识点正确率并逐一输出
 * 4. 输出当前错题数量（从 g_wrongQuestions 获取）
 * 5. 调用 pauseForUser() 等待用户确认
 *
 * @complexity O(M)，其中 M 为总记录数（需要遍历两次记录）
 * @note 该函数会阻塞等待用户按键
 */
void showStatistics() {
    // 检查是否有做题记录
    if (g_records.empty()) {
        std::cout << "当前还没有任何做题记录。\n";
        pauseForUser();
        return;
    }

    // ==================== 第一部分：总体统计 ====================
    int total = (int)g_records.size();  // 总作答题数
    int correct = 0;                     // 答对题数

    // 遍历所有记录，统计答对题数
    for (const auto& r : g_records) {
        if (r.correct) correct++;
    }

    // 计算总体正确率（百分比形式）
    double acc = correct * 1.0 / total * 100.0;

    // 输出总体统计信息
    std::cout << "===== 总体统计 =====\n";
    std::cout << "总作答题数： " << total << "\n";
    std::cout << "答对题数：   " << correct << "\n";
    std::cout << "总体正确率： " << acc << "%\n\n";

    // ==================== 第二部分：知识点统计 ====================
    std::cout << "===== 按知识点统计 =====\n";

    // 初始化知识点统计哈希表
    std::unordered_map<std::string, KnowledgeStat> ks;

    // 遍历所有记录，按知识点累加统计数据
    for (const auto& r : g_records) {
        // 通过题目 ID 查找题目索引
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue; // 题目不存在则跳过

        size_t qIdx = itQ->second;
        if (qIdx >= g_questions.size()) continue; // 索引异常则跳过

        // 获取题目所属的知识点
        const std::string& kd = g_questions[qIdx].knowledge;

        // 累加该知识点的统计数据
        ks[kd].total++;                    // 总题数 +1
        if (r.correct) ks[kd].correct++;   // 如果答对，答对数 +1
    }

    // 遍历知识点统计表，输出各知识点的详细信息
    for (const auto& p : ks) {
        const std::string& kd = p.first;   // 知识点名称
        const KnowledgeStat& s = p.second; // 知识点统计信息

        // 计算该知识点的正确率
        double kacc = s.correct * 1.0 / s.total * 100.0;

        // 输出该知识点的统计信息
        std::cout << "[知识点] " << kd
             << "  总题数: " << s.total
             << "  正确数: " << s.correct
             << "  正确率: " << kacc << "%\n";
    }

    // ==================== 第三部分：错题统计 ====================
    std::cout << "\n当前错题数： " << g_wrongQuestions.size() << " 道。\n";

    // 等待用户按键确认
    pauseForUser();
}
