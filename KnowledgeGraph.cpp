#include "KnowledgeGraph.h"
#include "Stats.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// 知识点依赖图全局变量定义
std::unordered_map<std::string, std::vector<std::string>> g_knowledgePrereq;
std::unordered_set<std::string> g_allKnowledgeNodes;

bool loadKnowledgeGraphFromFile(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cout << "警告：未找到知识点依赖图文件 " << filename << "，复习路径推荐功能将不可用。\n";
        return false;
    }

    g_knowledgePrereq.clear();
    g_allKnowledgeNodes.clear();

    std::string line;
    int lineNum = 0;
    while (std::getline(fin, line)) {
        ++lineNum;
        if (line.empty()) continue;

        // 按 '|' 分隔
        size_t pos = line.find('|');
        if (pos == std::string::npos) {
            std::cout << "知识图第 " << lineNum << " 行格式错误，已跳过。\n";
            continue;
        }

        std::string currentKnowledge = line.substr(0, pos);
        std::string prereqStr = line.substr(pos + 1);

        // 去除 currentKnowledge 前后空格
        size_t start = currentKnowledge.find_first_not_of(" \t\r\n");
        size_t end = currentKnowledge.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            currentKnowledge = currentKnowledge.substr(start, end - start + 1);
        }

        if (currentKnowledge.empty()) continue;

        // 将当前知识点加入节点集合
        g_allKnowledgeNodes.insert(currentKnowledge);

        // 解析前置知识点列表（逗号分隔）
        std::vector<std::string> prereqs;
        std::stringstream ss(prereqStr);
        std::string prereq;
        while (std::getline(ss, prereq, ',')) {
            // 去除空格
            start = prereq.find_first_not_of(" \t\r\n");
            end = prereq.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                prereq = prereq.substr(start, end - start + 1);
            }
            if (!prereq.empty()) {
                prereqs.push_back(prereq);
                g_allKnowledgeNodes.insert(prereq);
            }
        }

        g_knowledgePrereq[currentKnowledge] = prereqs;
    }

    std::cout << "知识点依赖图加载完成，共 " << g_allKnowledgeNodes.size() << " 个知识点。\n";
    return true;
}

void dfsReviewPath(const std::string& node, std::unordered_set<std::string>& visited, std::vector<std::string>& path) {
    if (visited.count(node)) return;
    visited.insert(node);

    // 先访问所有前置知识点
    auto it = g_knowledgePrereq.find(node);
    if (it != g_knowledgePrereq.end()) {
        for (const std::string& prereq : it->second) {
            dfsReviewPath(prereq, visited, path);
        }
    }

    // 再将当前节点加入路径
    path.push_back(node);
}

void recommendReviewPath() {
    if (g_allKnowledgeNodes.empty()) {
        std::cout << "知识点依赖图未加载，无法提供复习路径推荐。\n";
        std::cout << "请确保 data/knowledge_graph.txt 文件存在。\n";
        pauseForUser();
        return;
    }

    // 步骤 1：统计知识点掌握情况
    auto knowledgeStats = buildKnowledgeStats();

    std::cout << "========== 知识点复习路径推荐 ==========\n\n";
    std::cout << "===== 当前知识点掌握情况 =====\n";

    // 用于排序的临时结构
    struct KnowledgeItem {
        std::string name;
        int total;
        int correct;
        double accuracy;
        bool operator<(const KnowledgeItem& other) const {
            // 按正确率从低到高排序（掌握最弱的在前）
            return accuracy < other.accuracy;
        }
    };

    std::vector<KnowledgeItem> items;
    for (const std::string& kd : g_allKnowledgeNodes) {
        KnowledgeItem item;
        item.name = kd;
        if (knowledgeStats.count(kd)) {
            item.total = knowledgeStats[kd].total;
            item.correct = knowledgeStats[kd].correct;
            item.accuracy = knowledgeStats[kd].accuracy;
        } else {
            item.total = 0;
            item.correct = 0;
            item.accuracy = 0.0;
        }
        items.push_back(item);
    }

    std::sort(items.begin(), items.end());

    // 显示掌握情况
    for (size_t i = 0; i < items.size(); ++i) {
        std::cout << (i + 1) << ". [" << items[i].name << "]  "
             << "题数: " << items[i].total
             << "  正确: " << items[i].correct
             << "  正确率: " << items[i].accuracy << "%";
        if (items[i].total == 0) {
            std::cout << " (未练习)";
        }
        std::cout << "\n";
    }

    // 步骤 2：让用户选择目标知识点
    std::cout << "\n请输入要复习的知识点编号（1-" << items.size() << "）：";
    int choice;
    std::cin >> choice;

    if (choice < 1 || choice > (int)items.size()) {
        std::cout << "无效的选择。\n";
        pauseForUser();
        return;
    }

    std::string targetKnowledge = items[choice - 1].name;

    // 步骤 3：生成复习路径
    std::unordered_set<std::string> visited;
    std::vector<std::string> path;
    dfsReviewPath(targetKnowledge, visited, path);

    // 步骤 4：打印推荐复习路径
    std::cout << "\n========== 推荐复习路径 ==========\n";
    std::cout << "目标知识点：" << targetKnowledge << "\n\n";

    if (path.empty()) {
        std::cout << "未找到复习路径。\n";
        return;
    }

    std::cout << "建议按以下顺序复习：\n\n";
    for (size_t i = 0; i < path.size(); ++i) {
        const std::string& kd = path[i];
        std::cout << (i + 1) << ". " << kd;

        // 显示该知识点的掌握情况
        if (knowledgeStats.count(kd)) {
            std::cout << "  [题数: " << knowledgeStats[kd].total
                 << ", 正确率: " << knowledgeStats[kd].accuracy << "%]";
            if (knowledgeStats[kd].accuracy < 60.0 && knowledgeStats[kd].total > 0) {
                std::cout << " ⚠ 薄弱环节";
            }
        } else {
            std::cout << "  [未练习] ⚠ 需要学习";
        }

        // 箭头指向下一个
        if (i < path.size() - 1) {
            std::cout << " →";
        }
        std::cout << "\n";
    }

    std::cout << "\n建议：先巩固前置知识点，再学习后续内容，效果更佳！\n";
    std::cout << "====================================\n";

    // 复习路径推荐结束后暂停
    pauseForUser();
}
