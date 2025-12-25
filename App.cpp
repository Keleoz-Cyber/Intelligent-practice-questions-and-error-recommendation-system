/**
 * @file App.cpp
 * @brief 应用程序主界面与交互控制模块实现
 *
 * 实现菜单显示、用户交互、功能调度等核心逻辑。
 * 【清屏设计】清屏统一放在 runMenuLoop 循环开头，不在各功能函数结束时清屏，
 * 确保用户可以完整查看功能输出后再按回车返回菜单。
 *
 * @author DS_AI_Quiz Team
 * @date 2025
 */

#include "App.h"
#include "Question.h"
#include "Record.h"
#include "Stats.h"
#include "Recommender.h"
#include "KnowledgeGraph.h"
#include "Report.h"
#include "Utils.h"
#include <iostream>
#include <random>
#include <algorithm>

/**
 * @brief 显示主菜单
 *
 * 输出系统标题和所有功能选项的文字界面。
 * 本函数只负责显示，不处理输入逻辑。
 */
void showMenu() {
    std::cout << "=============================\n";
    std::cout << " 数据结构智能刷题与错题推荐系统\n";
    std::cout << "=============================\n";
    std::cout << "1. 随机刷题\n";
    std::cout << "2. 错题本练习\n";
    std::cout << "3. AI 智能推荐练习\n";
    std::cout << "4. 做题统计查看\n";
    std::cout << "5. 模拟考试模式\n";
    std::cout << "6. 知识点复习路径推荐\n";
    std::cout << "7. 导出学习报告\n";
    std::cout << "8. 切换用户\n";
    std::cout << "0. 退出\n";
    std::cout << "请选择：";
}

/**
 * @brief 随机刷题模式
 *
 * 从全局题库 g_questions 中随机选择一道题进行练习。
 *
 * 流程说明：
 * 1. 检查题库是否为空
 * 2. 使用 srand/rand 生成随机索引
 * 3. 调用 doQuestion() 执行答题逻辑（包含判分、记录、错题管理）
 * 4. 答题结束后暂停，等待用户按回车返回菜单
 *
 * @note 使用 time(nullptr) 作为随机种子，每次运行时随机性不同
 * @see doQuestion() 答题核心函数（Question 模块）
 */
void randomPractice() {
    if (g_questions.empty()) {
        std::cout << "题库为空，请先导入题库。\n";
        pauseForUser();
        return;
    }

    // 使用全局 RNG 生成随机索引
    std::uniform_int_distribution<size_t> dist(0, g_questions.size() - 1);
    size_t idx = dist(globalRng());
    const Question& q = g_questions[idx];

    doQuestion(q);

    // 答题结束后暂停，等待用户按回车返回菜单
    pauseForUser();
}

/**
 * @brief 错题本练习模式
 *
 * 从当前用户的错题集中随机选择一道题进行针对性练习。
 *
 * 流程说明：
 * 1. 检查错题集 g_wrongQuestions（unordered_set）是否为空
 * 2. 将错题 ID 复制到 vector，因为 unordered_set 不支持随机访问
 * 3. 使用 srand/rand 随机选择一个错题 ID
 * 4. 通过 g_questionById 映射查找对应的题目对象
 * 5. 调用 doQuestion() 执行答题逻辑
 *    - 如果答对，doQuestion 内部会自动将该题从错题集移除
 *    - 如果答错，该题继续保留在错题集中
 * 6. 答题结束后暂停，等待用户按回车返回菜单
 *
 * 交互说明：
 * - 错题集为空时提示用户先去做题
 * - 支持错题反复练习，直至掌握
 *
 * @note 错题集使用 unordered_set 存储，查询效率 O(1)，但需要转换为 vector 才能随机访问
 * @see doQuestion() 答题核心函数（Question 模块）
 * @see g_wrongQuestions 全局错题集
 * @see g_questionById 全局题目 ID 映射表
 */
void wrongBookMode() {
    if (g_wrongQuestions.empty()) {
        std::cout << "当前没有错题，先去刷题或者做几道题再来吧。\n";
        pauseForUser();
        return;
    }

    // 把错题号从 unordered_set 拷贝到 vector，便于随机访问
    std::vector<int> ids;
    ids.reserve(g_wrongQuestions.size());
    for (int id : g_wrongQuestions) {
        ids.push_back(id);
    }

    // 使用全局 RNG 随机选择一道错题
    std::uniform_int_distribution<size_t> dist(0, ids.size() - 1);
    size_t randomIdx = dist(globalRng());
    int qid = ids[randomIdx];

    // 通过 ID 查找题目对象
    auto it = g_questionById.find(qid);
    if (it == g_questionById.end()) {
        std::cout << "错误：找不到错题对应的题目内容。\n";
        pauseForUser();
        return;
    }

    // 使用索引访问题目
    size_t qIdx = it->second;
    if (qIdx >= g_questions.size()) {
        std::cout << "错误：题目索引异常。\n";
        pauseForUser();
        return;
    }

    std::cout << "【错题本练习】\n";
    doQuestion(g_questions[qIdx]);

    // 答题结束后暂停，等待用户按回车返回菜单
    pauseForUser();
}

/**
 * @brief 模拟考试模式
 *
 * 提供完整的模拟考试体验，包括自定义题目数量、随机抽题、成绩统计、知识点分析。
 *
 * 流程说明：
 * 1. 检查题库是否为空
 * 2. 让用户输入考试题目数量 N，范围限制为 [1, 题库总数]
 * 3. 使用 shuffle + 截取策略抽取 N 道互不重复的题目：
 *    - 创建包含所有题目索引的 vector
 *    - 使用 std::shuffle + mt19937 随机打乱索引顺序（Fisher-Yates 算法）
 *    - 截取前 N 个索引作为本次考试题目
 * 4. 记录考试开始前的 g_records 大小（prevSize），用于后续统计本次考试数据
 * 5. 顺序出题，每道题调用 doQuestion() 进行答题和记录
 * 6. 考试结束后统计本次成绩：
 *    - 计算总题数、答对数、答错数、正确率
 *    - 按知识点分组统计每个知识点的答题情况（使用 unordered_map）
 * 7. 打印详细考试报告，包含整体成绩和知识点维度的分析
 * 8. 答题结束后暂停，等待用户按回车返回菜单
 *
 * 抽题去重策略分析：
 * - 算法：Fisher-Yates shuffle + 截取前 N 个元素
 * - 时间复杂度：O(M)，M 为题库总数（shuffle 的复杂度）
 * - 空间复杂度：O(M)，需要存储全部题目索引
 * - 正确性保证：shuffle 保证每个排列等概率，截取前 N 个等价于无放回随机抽取
 * - 优点：实现简单、逻辑清晰、完全随机且无重复
 * - 适用场景：题库规模在万级别以内性能良好
 * - 可优化方向：如果 N << M，可使用 Reservoir Sampling 降低空间复杂度到 O(N)
 *
 * 交互说明：
 * - 用户可自定义考试题目数量，适应不同练习需求
 * - 不设置强制时间限制，用户可按自己节奏完成
 * - 系统自动记录每道题的作答时间（由 doQuestion 内部处理）
 * - 考试报告提供整体和知识点两个维度的统计分析
 *
 * @note 使用 std::random_device 和 std::mt19937 生成高质量随机数，避免简单随机数的偏差
 * @see doQuestion() 答题核心函数（Question 模块）
 * @see g_records 全局做题记录
 * @see g_questionById 全局题目 ID 映射表
 */
void examMode() {
    if (g_questions.empty()) {
        std::cout << "题库为空，无法进行考试。\n";
        pauseForUser();
        return;
    }

    // 步骤 1：让用户输入考试题目数量（使用健壮输入函数）
    std::cout << "========== 模拟考试模式 ==========\n";
    std::cout << "题库共有 " << g_questions.size() << " 道题。\n";

    int N;
    if (!readIntSafely("请输入本次考试的题目数量：", N, 1, (int)g_questions.size(), false)) {
        std::cout << "已取消考试。\n";
        pauseForUser();
        return;
    }

    std::cout << "本次考试共 " << N << " 道题。\n";

    // 步骤 2（可选）：简单提示时间限制
    std::cout << "提示：本次考试不设置强制时间限制，请按自己的节奏完成。\n";
    std::cout << "（系统会记录每道题的作答时间）\n\n";

    // 步骤 3：随机抽取 N 道互不重复的题目
    // 使用 shuffle + 截取策略保证完全随机且无重复
    std::vector<int> indices;
    indices.reserve(g_questions.size());
    for (size_t i = 0; i < g_questions.size(); ++i) {
        indices.push_back(i);
    }

    // 使用全局 mt19937 进行洗牌（Fisher-Yates 算法）
    std::shuffle(indices.begin(), indices.end(), globalRng());

    // 取前 N 个索引作为本次考试题目（时间复杂度 O(N)）
    std::vector<int> selectedIndices(indices.begin(), indices.begin() + N);

    // 步骤 4：记录考试开始前的记录数，用于后续统计本次考试的答题数据
    size_t prevSize = g_records.size();

    std::cout << "考试开始！\n";
    std::cout << "====================================\n\n";

    // 步骤 5：顺序出题，每道题调用 doQuestion 进行答题和记录
    for (int i = 0; i < N; ++i) {
        std::cout << "【第 " << (i + 1) << "/" << N << " 题】\n";
        const Question& q = g_questions[selectedIndices[i]];
        doQuestion(q);
        std::cout << "\n";
    }

    std::cout << "====================================\n";
    std::cout << "考试结束！正在统计成绩...\n\n";

    // 步骤 6：统计本次考试结果（整体维度）
    // 通过比较考试前后的 g_records 大小，定位本次考试的记录范围
    int totalExam = (int)(g_records.size() - prevSize);
    int correctExam = 0;

    for (size_t i = prevSize; i < g_records.size(); ++i) {
        if (g_records[i].correct) {
            correctExam++;
        }
    }

    double examAcc = 0.0;
    if (totalExam > 0) {
        examAcc = correctExam * 100.0 / totalExam;
    }

    // 步骤 7：按知识点统计本次考试表现（知识点维度）
    // 定义局部结构体存储每个知识点的统计数据
    struct KnowledgeStat {
        int total = 0;     // 该知识点的总题数
        int correct = 0;   // 该知识点的答对数
    };
    std::unordered_map<std::string, KnowledgeStat> examKnowledge;

    // 遍历本次考试的所有记录，按知识点分组统计
    for (size_t i = prevSize; i < g_records.size(); ++i) {
        const Record& r = g_records[i];
        auto itQ = g_questionById.find(r.questionId);
        if (itQ == g_questionById.end()) continue;

        size_t qIdx = itQ->second;
        if (qIdx >= g_questions.size()) continue;

        const std::string& kd = g_questions[qIdx].knowledge;
        examKnowledge[kd].total++;
        if (r.correct) {
            examKnowledge[kd].correct++;
        }
    }

    // 步骤 8：打印详细考试报告
    std::cout << "========== 考试成绩报告 ==========\n";
    std::cout << "本次考试题目数： " << totalExam << "\n";
    std::cout << "答对题数：       " << correctExam << "\n";
    std::cout << "答错题数：       " << (totalExam - correctExam) << "\n";
    std::cout << "本次正确率：     " << examAcc << "%\n\n";

    std::cout << "===== 按知识点统计 =====\n";
    for (const auto& p : examKnowledge) {
        const std::string& kd = p.first;
        const KnowledgeStat& s = p.second;
        double kacc = 0.0;
        if (s.total > 0) {
            kacc = s.correct * 100.0 / s.total;
        }
        std::cout << "[知识点] " << kd
             << "  题数: " << s.total
             << "  正确: " << s.correct
             << "  正确率: " << kacc << "%\n";
    }

    std::cout << "====================================\n";
    std::cout << "考试记录已保存，可在\"做题统计查看\"中查看历史数据。\n";

    // 考试结束后暂停，等待用户按回车返回菜单
    pauseForUser();
}

/**
 * @brief 切换用户
 *
 * 允许用户切换到不同的学号或用户名，实现多用户独立数据管理。
 *
 * 流程说明：
 * 1. 提示用户输入新的学号或用户名
 * 2. 调用 cin.ignore() 清除输入缓冲区残留的换行符（避免 getline 读到空行）
 * 3. 使用 std::getline 读取用户输入（支持包含空格的用户标识）
 * 4. 去除输入字符串前后的空格、制表符、换行符
 * 5. 验证输入非空，如果为空则循环重新提示
 * 6. 调用 loginUser() 切换当前用户（Record 模块）
 * 7. 调用 loadRecordsFromFile() 重新加载新用户的做题记录
 * 8. 输出切换成功提示
 * 9. 调用 pauseForUser() 等待用户按回车返回菜单
 *
 * 交互说明：
 * - 支持多用户共享题库，独立管理做题记录和错题本
 * - 切换后自动加载新用户的历史数据
 * - 输入验证：不允许空用户标识
 * - 用户标识可以包含空格（使用 getline 读取）
 *
 * 技术细节：
 * - cin.ignore() 清除输入缓冲区，防止上次输入残留的换行符被 getline 读取
 * - 使用 find_first_not_of / find_last_not_of 去除字符串前后空白字符
 *
 * @note 必须先调用 cin.ignore() 清除缓冲区，否则 getline 可能读到空行
 * @see loginUser() 用户登录逻辑（Record 模块）
 * @see loadRecordsFromFile() 加载做题记录（Record 模块）
 * @see getRecordFilePath() 获取当前用户的记录文件路径
 * @see g_currentUserId 全局当前用户 ID
 */
void switchUser() {
    std::cout << "\n========== 切换用户 ==========\n";
    std::cout << "请输入新的学号或用户名：";

    std::string userId;
    // std::cin.ignore(); // 删除此行：因为上层菜单使用的是 readIntSafely (getline)，缓冲区已经是干净的
    while (true) {
        std::getline(std::cin, userId);
        // 去除前后空格、制表符、换行符
        userId.erase(0, userId.find_first_not_of(" \t\n\r"));
        userId.erase(userId.find_last_not_of(" \t\n\r") + 1);

        if (!userId.empty()) {
            break;
        }
        std::cout << "用户标识不能为空，请重新输入：";
    }

    // 切换用户（更新全局 g_currentUserId）
    loginUser(userId);

    // 重新加载新用户的做题记录（清空旧记录，加载新记录）
    loadRecordsFromFile(getRecordFilePath());

    std::cout << "已成功切换到用户：" << g_currentUserId << "\n";
    std::cout << "====================================\n";

    // 切换用户后暂停，等待用户按回车返回菜单
    pauseForUser();
}

/**
 * @brief 主菜单循环
 *
 * 应用程序的主控制器，负责菜单显示、用户输入处理、功能模块调度。
 *
 * 核心流程：
 * 1. 【清屏设计】循环开始时调用 clearScreen()，确保菜单显示清爽
 *    - 放在循环开头的原因：用户看完上一功能的输出后，按回车才清屏显示菜单
 *    - 避免放在循环结尾：会导致功能输出立即被清除，用户无法查看结果
 *    - 避免放在各功能函数内：分散管理清屏逻辑，不清晰且用户体验差
 * 2. 调用 showMenu() 显示菜单选项
 * 3. 读取用户输入的选项（std::cin >> choice）
 * 4. 根据选项调度对应功能模块：
 *    - choice 0：退出程序
 *    - choice 1：randomPractice()（随机刷题模式）
 *    - choice 2：wrongBookMode()（错题本练习模式）
 *    - choice 3：aiRecommendMode()（AI 智能推荐练习，Recommender 模块）
 *    - choice 4：showStatistics()（做题统计查看，Stats 模块）
 *    - choice 5：examMode()（模拟考试模式）
 *    - choice 6：recommendReviewPath()（知识点复习路径推荐，KnowledgeGraph 模块）
 *    - choice 7：exportLearningReport()（导出学习报告，Report 模块）
 *    - choice 8：switchUser()（切换用户）
 *    - 其他：提示无效选项，调用 pauseForUser() 等待用户确认
 * 5. 功能执行完毕后，用户按回车键返回，进入下一次循环（此时清屏）
 *
 * clearScreen 位置设计原则（重要！）：
 * - ✅ 正确做法：放在循环开头
 *   - 用户执行完某功能后，看到完整输出结果
 *   - 用户按回车后，系统清屏并显示菜单
 *   - 交互自然流畅，体验良好
 *
 * - ❌ 错误做法 1：放在循环结尾
 *   - 功能执行完立即清屏，用户看不到输出结果
 *   - 必须在功能函数内部调用 pauseForUser()，增加耦合
 *
 * - ❌ 错误做法 2：放在各功能函数内
 *   - 清屏逻辑分散在各模块，难以维护
 *   - 用户体验不一致，有些函数清屏有些不清屏
 *
 * 异常处理：
 * - 如果 std::cin 读取失败（如输入非数字），跳出循环退出程序
 *
 * @note 本函数是整个应用的主控制器，负责协调所有功能模块
 * @see clearScreen() 清屏函数（Utils 模块）
 * @see showMenu() 显示菜单
 * @see pauseForUser() 暂停等待用户（Utils 模块）
 * @see randomPractice() 随机刷题
 * @see wrongBookMode() 错题本练习
 * @see aiRecommendMode() AI 智能推荐（Recommender 模块）
 * @see showStatistics() 统计查看（Stats 模块）
 * @see examMode() 模拟考试
 * @see recommendReviewPath() 知识图谱推荐（KnowledgeGraph 模块）
 * @see exportLearningReport() 导出报告（Report 模块）
 * @see switchUser() 切换用户
 */
void runMenuLoop() {
    while (true) {
        // 【清屏设计】每次循环开始时清屏，确保菜单显示清爽
        // 用户看完上一功能的输出后，按回车才清屏显示菜单
        clearScreen();

        showMenu();
        int choice;
        // 使用健壮输入函数读取菜单选项
        if (!readIntSafely("", choice, 0, 8, false)) {
            // 如果读取失败，继续循环
            continue;
        }
        if (choice == 0) {
            std::cout << "再见！\n";
            break;
        } else if (choice == 1) {
            randomPractice();
        } else if (choice == 2) {
            wrongBookMode();
        } else if (choice == 3) {
            aiRecommendMode();  // AI 智能推荐练习（Recommender 模块）
        } else if (choice == 4) {
            showStatistics();   // 做题统计查看（Stats 模块）
        } else if (choice == 5) {
            examMode();
        } else if (choice == 6) {
            recommendReviewPath();  // 知识点复习路径推荐（KnowledgeGraph 模块）
        } else if (choice == 7) {
            exportLearningReport(); // 导出学习报告（Report 模块）
        } else if (choice == 8) {
            switchUser();
        } else {
            std::cout << "无效选项，请重新输入。\n";
            pauseForUser();
        }
    }
}
