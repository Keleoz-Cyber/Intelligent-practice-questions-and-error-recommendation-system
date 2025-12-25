/**
 * @file Question.cpp
 * @brief 题库管理模块 - 实现
 *
 * 【实现要点】
 * 1. 使用 std::ifstream 读取 CSV 文件（UTF-8 编码）
 * 2. 逐行解析：std::getline(line) + std::stringstream 按逗号分隔字段
 * 3. 容错处理：空行跳过、列数不足跳过、数值解析失败跳过（捕获 std::stoi 异常）
 * 4. 索引构建：遍历 g_questions，为每个题目建立 id -> Question* 的哈希映射
 * 5. 复杂度：O(N)，N 为题目数量（单次遍历 + O(1) 哈希插入）
 *
 * 【鲁棒性】
 * - 文件不存在：返回 false，输出错误信息
 * - 部分行格式错误：跳过错误行，继续解析其他行
 * - 题库为空：返回 false（即使文件存在但无有效题目）
 */

#include "Question.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>

// 全局题库容器定义
std::vector<Question> g_questions;
std::unordered_map<int, size_t> g_questionById;

/**
 * @brief 从 CSV 文件加载题库（实现）
 *
 * 【解析流程】
 * 1. 打开文件：std::ifstream，若失败返回 false
 * 2. 逐行读取：std::getline(fin, line)
 * 3. 按逗号分隔：std::getline(ss, field, ',')，收集到 fields 数组
 * 4. 校验字段数：至少 9 列，否则跳过
 * 5. 解析各字段：
 *    - id, answer, difficulty：使用 std::stoi（可能抛异常）
 *    - text, knowledge：字符串直接赋值
 *    - options：fields[2~5] 构成 4 个选项
 * 6. 异常处理：try-catch 捕获 std::stoi 失败，输出警告并跳过该行
 * 7. 加入容器：push_back 到 g_questions
 * 8. 建立索引：遍历 g_questions，为每个题目的 id 建立指针映射
 *
 * 【示例输入行】
 * 1,栈的基本操作是什么？,入栈,出栈,获取栈顶元素,以上都是,3,栈与队列,2
 *
 * 【示例输出】（控制台）
 * 题库加载完成，共读取到 50 道题。
 */
bool loadQuestionsFromFile(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cout << "无法打开题库文件: " << filename << std::endl;
        return false;
    }

    std::string line;
    int lineNum = 0;

    // 逐行读取并解析
    while (std::getline(fin, line)) {
        ++lineNum;
        // 跳过空行
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> fields;

        // 按逗号分隔字段
        // 注意：若题目文本中含逗号，需更复杂的 CSV 解析器（当前假设文本中无逗号）
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        // 校验字段数量：至少 9 列
        if (fields.size() < 9) {
            std::cout << "第 " << lineNum << " 行字段数量不足，已跳过。\n";
            continue;
        }

        // 解析字段并构造 Question 对象
        Question q;
        try {
            q.id = std::stoi(fields[0]);          // 题号
            q.text = fields[1];                   // 题目文本
            q.options = {fields[2], fields[3], fields[4], fields[5]};  // 4 个选项
            q.answer = std::stoi(fields[6]);      // 正确答案下标（0~3）
            q.knowledge = fields[7];              // 知识点
            q.difficulty = std::stoi(fields[8]);  // 难度（1~5）
        } catch (...) {
            // 捕获 std::stoi 抛出的异常（非法数值格式）
            std::cout << "第 " << lineNum << " 行解析出错，已跳过。\n";
            continue;
        }

        // 加入全局题库
        g_questions.push_back(q);
    }

    // 建立题号 -> 索引的映射（O(N)）
    // 使用索引而非指针，避免 vector 扩容导致指针失效
    for (size_t i = 0; i < g_questions.size(); ++i) {
        g_questionById[g_questions[i].id] = i;
    }

    // 输出加载结果
    std::cout << "题库加载完成，共读取到 " << g_questions.size() << " 道题。\n";

    // 若题库为空，返回 false
    return !g_questions.empty();
}
