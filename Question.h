/**
 * @file Question.h
 * @brief 题库管理模块 - 题目结构与加载
 *
 * 【模块职责】
 * 1. 定义题目数据结构 Question
 * 2. 维护全局题库容器：g_questions（顺序存储）、g_questionById（快速查询）
 * 3. 从 CSV 文件加载题库并建立索引
 *
 * 【关键数据结构】
 * - Question 结构体：单道题目的完整信息
 * - std::vector<Question> g_questions：所有题目的顺序存储
 * - std::unordered_map<int, const Question*> g_questionById：题号 -> 题目指针的哈希索引
 *
 * 【输入文件格式】
 * - 文件名：data/questions.csv
 * - 格式：CSV（逗号分隔），UTF-8 编码
 * - 列定义（9 列）：
 *   [0] id          题号（整数）
 *   [1] text        题目文本
 *   [2] option0     选项 A
 *   [3] option1     选项 B
 *   [4] option2     选项 C
 *   [5] option3     选项 D
 *   [6] answer      正确答案下标（0~3）
 *   [7] knowledge   知识点名称
 *   [8] difficulty  难度等级（1~5）
 *
 * 【与其他模块依赖】
 * - Record.cpp：通过 g_questionById 查询题目详情
 * - Stats.cpp：通过 g_questionById 获取知识点信息
 * - Recommender.cpp：遍历 g_questions 计算推荐分数
 * - App.cpp：随机抽题、考试模式等
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief 题目数据结构
 *
 * 存储一道选择题的完整信息，包括题目文本、选项、答案、知识点、难度等。
 */
struct Question {
    int id;                          ///< 题号（唯一标识）
    std::string text;                ///< 题目文本（问题描述）
    std::vector<std::string> options;///< 选项列表（通常 4 个：A/B/C/D）
    int answer;                      ///< 正确答案的下标（0~3，对应 options[answer]）
    std::string knowledge;           ///< 所属知识点（如"栈与队列"、"二叉树"等）
    int difficulty;                  ///< 难度等级（1~5，1 最简单，5 最难）
};

/**
 * @brief 全局题库容器（顺序存储）
 *
 * 存储所有从 CSV 加载的题目，保持加载顺序。
 * 适用场景：随机抽题、遍历题库、考试模式等。
 */
extern std::vector<Question> g_questions;

/**
 * @brief 全局题号索引（快速查询）
 *
 * 题号 -> g_questions 索引的哈希映射，提供 O(1) 查题功能。
 * 使用索引而非指针，避免 vector 扩容导致的指针失效问题。
 * 适用场景：根据做题记录的 questionId 快速获取题目详情。
 *
 * @warning 索引基于 g_questions 的当前状态，题库加载后不应修改 g_questions（push_back/erase）
 */
extern std::unordered_map<int, size_t> g_questionById;

/**
 * @brief 从 CSV 文件加载题库
 *
 * 【功能】
 * - 读取 questions.csv 文件（UTF-8 编码）
 * - 逐行解析，每行一道题（逗号分隔）
 * - 填充 g_questions（顺序容器）
 * - 建立 g_questionById 索引（题号 -> 指针）
 *
 * 【解析策略】
 * - 空行：跳过
 * - 列数不足（< 9 列）：输出警告，跳过该行
 * - 字段解析失败（stoi 抛出异常）：输出警告，跳过该行
 * - UTF-8 换行符：std::getline 自动处理 LF/CRLF
 *
 * @param filename CSV 文件路径（绝对路径或相对路径）
 * @return true  加载成功且至少有一道题
 * @return false 文件打开失败或题库为空
 *
 * @note 容错策略：部分行解析失败不影响其他行，最终返回成功加载的题目数量
 * @note 输出信息：控制台显示"题库加载完成，共读取到 X 道题"
 * @complexity O(N)，N 为题目数量（逐行解析 + 哈希索引构建）
 */
bool loadQuestionsFromFile(const std::string& filename);
