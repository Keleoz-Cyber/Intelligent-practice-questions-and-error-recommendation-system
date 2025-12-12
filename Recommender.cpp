#include "Recommender.h"
#include "Record.h"
#include "Utils.h"
#include <iostream>
#include <queue>
#include <ctime>
#include <cmath>

double computeRecommendScore(const Question& q, const QuestionStat& st, long long now) {
    // 1. 错误率部分
    double errorRate = 1.0; // 没做过就当成 1（需要了解）
    if (st.totalAttempts > 0) {
        errorRate = (st.totalAttempts - st.correctAttempts) * 1.0 / st.totalAttempts;
    }

    // 2. 时间间隔部分：最近一次做题距现在多少天（越久没做权重越高）
    double timeGapDays = 1.0; // 默认 1 天
    if (st.lastTimestamp > 0) {
        double seconds = difftime(now, (time_t)st.lastTimestamp);
        if (seconds < 0) seconds = 0;
        timeGapDays = seconds / 86400.0;
    } else {
        // 从未做过，给它一个中等偏上的时间权重
        timeGapDays = 7.0;
    }
    // 把时间间隔映射到 0~1，超过 7 天按 1 算
    double timeScore = timeGapDays / 7.0;
    if (timeScore > 1.0) timeScore = 1.0;

    // 3. 难度权重，难度 1~5 映射到 0.2~1.0
    double diffScore = 0.2 + (q.difficulty - 1) * 0.2;
    if (diffScore < 0.2) diffScore = 0.2;
    if (diffScore > 1.0) diffScore = 1.0;

    // 4. 没做过的题稍微加一点 bonus
    double unseenBonus = (st.totalAttempts == 0 ? 0.2 : 0.0);

    // 综合评分：可以给出一套权重，你可以在报告里写
    double score = 0.6 * errorRate + 0.3 * timeScore + 0.1 * diffScore + unseenBonus;

    // 限制到 [0, 2] 之间（理论上不会超）
    if (score < 0.0) score = 0.0;
    if (score > 2.0) score = 2.0;

    return score;
}

void aiRecommendMode() {
    if (g_questions.empty()) {
        std::cout << "题库为空，无法推荐。\n";
        pauseForUser();
        return;
    }

    // 每次进入推荐模式时，确保统计是最新的（因为刚刚可能做了新题）
    buildQuestionStats();

    long long now = (long long)std::time(nullptr);

    std::priority_queue<RecommendItem> pq;

    // 对每道题计算推荐分数，丢进堆里
    for (const auto& q : g_questions) {
        QuestionStat st;
        auto it = g_questionStats.find(q.id);
        if (it != g_questionStats.end()) {
            st = it->second;
        }
        double s = computeRecommendScore(q, st, now);
        pq.push({q.id, s});
    }

    int K = 5; // 一次推荐 5 道，你可以改
    if ((int)g_questions.size() < K) {
        K = (int)g_questions.size();
    }

    std::cout << "【AI 智能推荐模式】本次为你推荐 " << K << " 道题。\n";
    std::cout << "根据你的历史做题记录，优先推荐错误率高、长期未练习或难度较高的题目。\n\n";

    std::vector<int> selected;
    selected.reserve(K);
    while (!pq.empty() && (int)selected.size() < K) {
        selected.push_back(pq.top().questionId);
        pq.pop();
    }

    // 逐题练习
    for (size_t i = 0; i < selected.size(); ++i) {
        int qid = selected[i];
        auto itQ = g_questionById.find(qid);
        if (itQ == g_questionById.end()) continue;

        std::cout << "-----------------------------\n";
        std::cout << "第 " << (i + 1) << " 道推荐题：\n";
        doQuestion(*itQ->second);
        std::cout << "\n";
    }

    std::cout << "本轮 AI 推荐练习结束。\n";

    // AI 推荐练习结束后暂停
    pauseForUser();
}
