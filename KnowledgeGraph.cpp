/**
 * @file KnowledgeGraph.cpp
 * @brief 知识点依赖图管理模块实现
 *
 * 实现知识点依赖关系的加载、存储和复习路径推荐功能。
 * 核心算法：基于 DFS 的拓扑排序，时间复杂度 O(V+E)。
 */

#include "KnowledgeGraph.h"
#include "Stats.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

/**
 * @brief 知识点依赖图全局变量定义
 *
 * 邻接表存储结构：
 * - Key: 知识点名称
 * - Value: 该知识点的前置依赖列表
 *
 * 依赖方向：A -> B 表示 B 依赖于 A（需要先学 A 再学 B）
 */
std::unordered_map<std::string, std::vector<std::string>> g_knowledgePrereq;

/**
 * @brief 所有知识点节点集合
 *
 * 用于快速判断知识点是否存在，以及遍历统计
 */
std::unordered_set<std::string> g_allKnowledgeNodes;

/**
 * @brief 从文件加载知识点依赖图
 *
 * @details
 * 文件格式："知识点|前置1,前置2,前置3"
 * 示例：
 * @code
 * 数组|基本语法,变量
 * 链表|指针,结构体
 * 二叉树|链表,递归
 * @endcode
 *
 * 容错处理：
 * 1. 文件不存在：输出警告，返回 false，不影响系统其他功能
 * 2. 空行：自动跳过
 * 3. 格式错误行（无 '|' 分隔符）：输出行号提示，跳过该行
 * 4. 空知识点名：跳过该条记录
 *
 * 去重机制：
 * - g_allKnowledgeNodes 使用 unordered_set，自动去重
 * - 同一知识点出现多次时，后面的定义覆盖前面的
 *
 * 解析流程：
 * 1. 清空旧数据（支持重复加载）
 * 2. 逐行读取并解析
 * 3. 按 '|' 分隔知识点和前置依赖
 * 4. 去除字符串前后空白字符（空格、制表符、换行符）
 * 5. 按 ',' 分隔前置依赖列表
 * 6. 将所有知识点（包括前置依赖）加入节点集合
 * 7. 构建邻接表存储依赖关系
 *
 * @param filename 知识点依赖图文件路径
 * @return bool 加载成功返回 true，文件不存在返回 false
 *
 * @note 时间复杂度：O(N * M)
 *       - N: 文件行数
 *       - M: 每行平均前置依赖数量
 * @note 空间复杂度：O(V + E)
 *       - V: 知识点数量
 *       - E: 依赖边数量
 */
bool loadKnowledgeGraphFromFile(const std::string& filename) {
    // 尝试打开文件
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        // 容错：文件不存在时给出警告，但不中断程序
        std::cout << "警告：未找到知识点依赖图文件 " << filename << "，复习路径推荐功能将不可用。\n";
        return false;
    }

    // 清空旧数据，支持重复加载
    g_knowledgePrereq.clear();
    g_allKnowledgeNodes.clear();

    std::string line;
    int lineNum = 0;  // 行号，用于错误提示

    // 逐行读取文件
    while (std::getline(fin, line)) {
        ++lineNum;

        // 容错：跳过空行
        if (line.empty()) continue;

        // 按 '|' 分隔知识点和前置依赖
        // 格式：知识点名称|前置1,前置2,前置3
        size_t pos = line.find('|');
        if (pos == std::string::npos) {
            // 容错：格式错误时输出行号提示，继续处理下一行
            std::cout << "知识图第 " << lineNum << " 行格式错误，已跳过。\n";
            continue;
        }

        // 提取当前知识点名称（'|' 左侧）
        std::string currentKnowledge = line.substr(0, pos);
        // 提取前置依赖字符串（'|' 右侧）
        std::string prereqStr = line.substr(pos + 1);

        // 去除知识点名称前后的空白字符（空格、制表符、换行符）
        // 这样可以容忍用户在输入时添加的额外空格
        size_t start = currentKnowledge.find_first_not_of(" \t\r\n");
        size_t end = currentKnowledge.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            currentKnowledge = currentKnowledge.substr(start, end - start + 1);
        }

        // 容错：跳过空知识点名
        if (currentKnowledge.empty()) continue;

        // 将当前知识点加入节点集合（去重由 unordered_set 自动处理）
        g_allKnowledgeNodes.insert(currentKnowledge);

        // 解析前置知识点列表（逗号分隔）
        std::vector<std::string> prereqs;
        std::stringstream ss(prereqStr);
        std::string prereq;

        // 按逗号分隔读取每个前置知识点
        while (std::getline(ss, prereq, ',')) {
            // 去除前置知识点的前后空格
            start = prereq.find_first_not_of(" \t\r\n");
            end = prereq.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                prereq = prereq.substr(start, end - start + 1);
            }

            // 只添加非空的前置知识点
            if (!prereq.empty()) {
                prereqs.push_back(prereq);
                // 前置知识点也需要加入节点集合（确保图的完整性）
                g_allKnowledgeNodes.insert(prereq);
            }
        }

        // 构建邻接表：当前知识点 -> 前置依赖列表
        // 注意：如果同一知识点出现多次，后面的定义会覆盖前面的
        g_knowledgePrereq[currentKnowledge] = prereqs;
    }

    // 输出加载统计信息
    std::cout << "知识点依赖图加载完成，共 " << g_allKnowledgeNodes.size() << " 个知识点。\n";
    return true;
}

/**
 * @brief DFS 深度优先搜索生成复习路径（拓扑排序）
 *
 * @details
 * 算法核心：后序遍历的 DFS 拓扑排序
 *
 * 遍历顺序（关键步骤）：
 * 1. 【防环剪枝】检查节点是否已访问，若已访问则直接返回
 * 2. 【标记访问】将当前节点标记为已访问（visited.insert）
 * 3. 【递归前置】遍历当前节点的所有前置依赖，递归调用 DFS
 * 4. 【后序加入】所有前置依赖处理完后，将当前节点加入路径末尾
 *
 * 拓扑顺序保证：
 * - 采用后序遍历（post-order traversal）方式
 * - 节点在其所有前置依赖之后加入路径
 * - 保证路径中任意节点的前置依赖都排在它之前
 * - 满足"先学基础，后学高级"的学习逻辑
 *
 * visited 防环机制：
 * - visited 记录已处理的节点，避免重复访问
 * - 防止图中存在环时导致无限递归（虽然应该是 DAG）
 * - 提高效率：已访问节点不再重复处理，O(1) 判断
 *
 * 示例执行流程：
 * @code
 * // 依赖关系：
 * // 图 -> [队列, 栈]
 * // 队列 -> [基本语法]
 * // 栈 -> [基本语法]
 *
 * dfsReviewPath("图", visited, path);
 *
 * // 执行过程：
 * // 1. 访问"图"，标记已访问
 * // 2. 递归访问"队列"
 * //    2.1. 访问"队列"，标记已访问
 * //    2.2. 递归访问"基本语法"
 * //         2.2.1. 访问"基本语法"，无前置依赖
 * //         2.2.2. 加入 path: ["基本语法"]
 * //    2.3. 加入 path: ["基本语法", "队列"]
 * // 3. 递归访问"栈"
 * //    3.1. 访问"栈"，标记已访问
 * //    3.2. 递归访问"基本语法"（已访问，直接返回）
 * //    3.3. 加入 path: ["基本语法", "队列", "栈"]
 * // 4. 加入 path: ["基本语法", "队列", "栈", "图"]
 * @endcode
 *
 * @param node 当前访问的知识点名称
 * @param visited 已访问节点集合（防环 + 去重）
 * @param path 生成的复习路径序列（按拓扑顺序排列）
 *
 * @note 时间复杂度：O(V + E)
 *       - 每个节点最多访问一次：O(V)
 *       - 每条边最多遍历一次：O(E)
 *       - visited 集合查询和插入：O(1)
 * @note 空间复杂度：O(V)
 *       - visited 集合大小：O(V)
 *       - 递归栈最大深度：O(V)（最坏情况为链式依赖）
 *       - path 数组大小：O(V)
 *
 * @warning 输入图必须是 DAG（有向无环图），否则可能栈溢出
 * @warning 调用前需要确保 visited 和 path 已初始化为空
 *
 * @see g_knowledgePrereq 依赖关系邻接表
 */
void dfsReviewPath(const std::string& node, std::unordered_set<std::string>& visited, std::vector<std::string>& path) {
    // 【防环剪枝】如果节点已访问，直接返回（避免重复处理和环路）
    if (visited.count(node)) return;

    // 【标记访问】将当前节点标记为已访问
    visited.insert(node);

    // 【递归前置】先访问所有前置知识点（保证前置依赖优先入队）
    auto it = g_knowledgePrereq.find(node);
    if (it != g_knowledgePrereq.end()) {
        // 遍历当前知识点的所有前置依赖
        for (const std::string& prereq : it->second) {
            // 递归处理每个前置知识点
            dfsReviewPath(prereq, visited, path);
        }
    }

    // 【后序加入】所有前置依赖处理完毕后，将当前节点加入路径
    // 这是拓扑排序的关键：保证节点在其所有依赖之后
    path.push_back(node);
}

/**
 * @brief 知识点复习路径推荐（主功能入口）
 *
 * @details
 * 这是一个交互式功能，结合用户答题统计和知识点依赖图，
 * 为用户提供科学的复习路径规划。
 *
 * 功能流程（6 个步骤）：
 *
 * 【步骤 1】检查依赖图加载状态
 * - 若未加载（g_allKnowledgeNodes 为空），输出错误提示并返回
 * - 确保后续步骤有数据可用
 *
 * 【步骤 2】统计知识点掌握情况
 * - 调用 buildKnowledgeStats() 获取每个知识点的答题统计
 * - 统计数据包括：总题数、正确数、正确率
 * - 对未练习的知识点设置正确率为 0.0%
 *
 * 【步骤 3】排序并显示掌握情况
 * - 按正确率从低到高排序（薄弱知识点优先显示）
 * - 排序依据：正确率越低，越需要复习
 * - 显示格式：编号 + 知识点名 + 题数 + 正确数 + 正确率
 * - 标注未练习的知识点
 *
 * 【步骤 4】用户选择目标知识点
 * - 提示用户输入要复习的知识点编号
 * - 输入验证：检查编号范围是否合法
 * - 非法输入直接返回
 *
 * 【步骤 5】生成复习路径（核心算法）
 * - 使用 DFS 拓扑排序生成路径
 * - 路径包含目标知识点及其所有直接/间接前置依赖
 * - 保证学习顺序符合依赖关系
 *
 * 【步骤 6】显示复习路径并标注薄弱环节
 * - 按拓扑顺序显示复习路径
 * - 结合统计标注每个知识点的掌握程度：
 *   1. 未练习：题数为 0，标记"⚠ 需要学习"
 *   2. 薄弱环节：正确率 < 60% 且题数 > 0，标记"⚠ 薄弱环节"
 *   3. 掌握良好：正确率 >= 60%，无特殊标记
 * - 使用箭头 "→" 指示学习路径方向
 * - 给出复习建议
 *
 * 统计标注逻辑：
 * @code
 * if (total == 0) {
 *     // 未练习，需要从头学习
 *     标记为 "⚠ 需要学习"
 * } else if (accuracy < 60.0) {
 *     // 练习过但掌握不好，是薄弱环节
 *     标记为 "⚠ 薄弱环节"
 * } else {
 *     // 掌握良好，无特殊标记
 * }
 * @endcode
 *
 * 输出示例：
 * @code
 * ========== 知识点复习路径推荐 ==========
 *
 * ===== 当前知识点掌握情况 =====
 * 1. [队列]  题数: 5  正确: 2  正确率: 40.0%
 * 2. [图]  题数: 3  正确: 1  正确率: 33.3%
 * 3. [栈]  题数: 0  正确: 0  正确率: 0.0% (未练习)
 * 4. [基本语法]  题数: 10  正确: 8  正确率: 80.0%
 *
 * 请输入要复习的知识点编号（1-4）：2
 *
 * ========== 推荐复习路径 ==========
 * 目标知识点：图
 *
 * 建议按以下顺序复习：
 *
 * 1. 基本语法  [题数: 10, 正确率: 80.0%] →
 * 2. 队列  [题数: 5, 正确率: 40.0%] ⚠ 薄弱环节 →
 * 3. 栈  [未练习] ⚠ 需要学习 →
 * 4. 图  [题数: 3, 正确率: 33.3%] ⚠ 薄弱环节
 *
 * 建议：先巩固前置知识点，再学习后续内容，效果更佳！
 * @endcode
 *
 * 时间复杂度分析：
 * 1. 统计知识点掌握情况：O(V)，遍历所有知识点
 * 2. 排序：O(V log V)，使用标准库排序
 * 3. DFS 生成路径：O(V + E)，拓扑排序
 * 4. 显示路径：O(V)，遍历路径数组
 * 总时间复杂度：O(V log V + V + E) = O(V log V + E)
 *
 * @note 时间复杂度：O(V log V + E)
 *       - V: 知识点数量
 *       - E: 依赖边数量
 *       - 瓶颈在排序和 DFS
 * @note 空间复杂度：O(V)
 *       - 统计数组、visited 集合、path 数组均为 O(V)
 * @note 交互式函数，需要用户输入
 * @note 依赖 Stats 模块的 buildKnowledgeStats() 函数
 *
 * @warning 如果知识点依赖图未加载，会提示错误并返回
 * @warning 如果用户输入无效编号，会提示错误并返回
 *
 * @see loadKnowledgeGraphFromFile 加载依赖图
 * @see dfsReviewPath DFS 生成路径算法
 * @see buildKnowledgeStats 获取知识点统计（Stats.h）
 */
void recommendReviewPath() {
    // 【步骤 1】检查依赖图是否已加载
    if (g_allKnowledgeNodes.empty()) {
        std::cout << "知识点依赖图未加载，无法提供复习路径推荐。\n";
        std::cout << "请确保 data/knowledge_graph.txt 文件存在。\n";
        pauseForUser();
        return;
    }

    // 【步骤 2】统计知识点掌握情况
    // 从 Stats 模块获取用户的答题统计数据
    auto knowledgeStats = buildKnowledgeStats();

    std::cout << "========== 知识点复习路径推荐 ==========\n\n";
    std::cout << "===== 当前知识点掌握情况 =====\n";

    // 【步骤 3】排序并显示掌握情况

    // 定义临时结构体用于排序
    // 包含知识点的统计信息：名称、总题数、正确数、正确率
    struct KnowledgeItem {
        std::string name;      // 知识点名称
        int total;             // 总答题数
        int correct;           // 正确答题数
        double accuracy;       // 正确率（百分比）

        // 重载 < 运算符，用于排序
        // 按正确率从低到高排序（掌握最弱的在前，优先推荐复习）
        bool operator<(const KnowledgeItem& other) const {
            return accuracy < other.accuracy;
        }
    };

    // 构建知识点统计列表
    std::vector<KnowledgeItem> items;
    for (const std::string& kd : g_allKnowledgeNodes) {
        KnowledgeItem item;
        item.name = kd;

        // 如果有统计数据，使用实际数据
        if (knowledgeStats.count(kd)) {
            item.total = knowledgeStats[kd].total;
            item.correct = knowledgeStats[kd].correct;
            item.accuracy = knowledgeStats[kd].accuracy;
        } else {
            // 如果没有统计数据（未练习），设置为 0
            item.total = 0;
            item.correct = 0;
            item.accuracy = 0.0;
        }
        items.push_back(item);
    }

    // // 按正确率从低到高排序（O(V log V)）
    // std::sort(items.begin(), items.end());

    // --- 优化版冒泡排序 ---
    for (size_t i = 0; i < items.size() - 1; ++i) {
        bool swapped = false; // 优化：标志位，记录这一轮是否发生过交换
        for (size_t j = 0; j < items.size() - 1 - i; ++j) {
            // 如果后一个比前一个“小”（即正确率更低），则交换
            if (items[j + 1] < items[j]) {
                std::swap(items[j], items[j + 1]);
                swapped = true;
            }
        }
        // 如果这一轮一次交换都没发生，说明已经排好序了，直接跳出
        if (!swapped) break;
    }

        // 显示知识点掌握情况列表
    for (size_t i = 0; i < items.size(); ++i) {
        std::cout << (i + 1) << ". [" << items[i].name << "]  "
             << "题数: " << items[i].total
             << "  正确: " << items[i].correct
             << "  正确率: " << items[i].accuracy << "%";
        // 标注未练习的知识点
        if (items[i].total == 0) {
            std::cout << " (未练习)";
        }
        std::cout << "\n";
    }

    // 【步骤 4】让用户选择目标知识点
    std::cout << "\n请输入要复习的知识点编号（1-" << items.size() << "）：";

    int choice = -1;
    // 使用 getline 读取，与系统其他部分保持一致，避免残留换行符
    std::string line;
    std::getline(std::cin, line);

    // 尝试解析输入
    try {
        choice = std::stoi(line);
    } catch (...) {
        choice = -1; // 解析失败
    }

    // 输入验证：检查编号范围
    if (choice < 1 || choice > (int)items.size()) {
        std::cout << "无效的选择。\n";
        pauseForUser();
        return;
    }

    // 获取用户选择的目标知识点
    std::string targetKnowledge = items[choice - 1].name;

    // 【步骤 5】生成复习路径（DFS 拓扑排序，O(V+E)）
    std::unordered_set<std::string> visited;  // 已访问节点集合
    std::vector<std::string> path;            // 复习路径序列
    dfsReviewPath(targetKnowledge, visited, path);

    // 【步骤 6】显示复习路径并标注薄弱环节
    std::cout << "\n========== 推荐复习路径 ==========\n";
    std::cout << "目标知识点：" << targetKnowledge << "\n\n";

    // 异常情况：路径为空（理论上不应该发生）
    if (path.empty()) {
        std::cout << "未找到复习路径。\n";
        return;
    }

    std::cout << "建议按以下顺序复习：\n\n";

    // 遍历路径，显示每个知识点及其掌握情况
    for (size_t i = 0; i < path.size(); ++i) {
        const std::string& kd = path[i];
        std::cout << (i + 1) << ". " << kd;

        // 显示该知识点的掌握情况和标注
        if (knowledgeStats.count(kd)) {
            // 有统计数据：显示题数和正确率
            std::cout << "  [题数: " << knowledgeStats[kd].total
                 << ", 正确率: " << knowledgeStats[kd].accuracy << "%]";

            // 标注薄弱环节：正确率 < 60% 且有答题记录
            if (knowledgeStats[kd].accuracy < 60.0 && knowledgeStats[kd].total > 0) {
                std::cout << " ⚠ 薄弱环节";
            }
        } else {
            // 无统计数据：标注为未练习，需要学习
            std::cout << "  [未练习] ⚠ 需要学习";
        }

        // 箭头指向下一个知识点（最后一个不显示箭头）
        if (i < path.size() - 1) {
            std::cout << " →";
        }
        std::cout << "\n";
    }

    // 给出复习建议
    std::cout << "\n建议：先巩固前置知识点，再学习后续内容，效果更佳！\n";
    std::cout << "====================================\n";

    // 复习路径推荐结束后暂停，等待用户确认
    pauseForUser();
}
