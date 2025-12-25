/**
 * @file Record.cpp
 * @brief 做题记录管理模块 - 核心实现
 *
 * 【模块功能概述】
 * 本模块实现做题记录的全生命周期管理：
 * 1. 多用户隔离：基于 g_currentUserId 动态路由到不同的 CSV 文件
 * 2. 记录持久化：CSV 格式存储（追加写入模式，防止数据丢失）
 * 3. 内存索引：维护三个全局容器（时间序列、题号索引、错题集）
 * 4. 做题流程：交互式答题 + 计时 + 判分 + 记录 + 自动持久化
 *
 * 【数据流转图】
 * 启动 -> loadRecordsFromFile() -> 从 CSV 重建内存索引
 *      -> doQuestion() -> 用户答题 -> 更新内存索引 + appendRecordToFile()
 *      -> 错题集动态维护（最后一次作答结果决定是否在错题集中）
 *
 * 【关键算法与数据结构】
 * 1. 错题集维护算法（doQuestion 中）：
 *    - 规则：最后一次答对 -> 移除；最后一次答错 -> 加入
 *    - 复杂度：O(1)（unordered_set 的 insert/erase）
 *    - 设计意图：反映用户当前未掌握的知识点
 *
 * 2. 按题号索引（g_recordsByQuestion）：
 *    - 结构：unordered_map<int, vector<Record>>（题号 -> 时间序列）
 *    - 用途：快速查询某题的历史作答记录（用于统计、错题判定）
 *    - 插入：O(1) 平均（map 查找 + vector push_back）
 *
 * 3. 全局记录列表（g_records）：
 *    - 结构：vector<Record>（纯时间序列）
 *    - 用途：总体统计、学习报告、考试模式统计
 *    - 追加：O(1) 摊销
 *
 * 【文件 I/O 策略】
 * - 读取：loadRecordsFromFile() 一次性加载全部历史记录
 * - 写入：appendRecordToFile() 每次答题后立即追加（防止异常退出导致数据丢失）
 * - 编码：UTF-8（跨平台兼容）
 * - 格式：CSV（易于数据分析、人工检查、Excel 打开）
 *
 * 【多用户隔离机制】
 * - 实现：g_currentUserId 全局变量 + 动态文件名（records_<userId>.csv）
 * - 切换用户：loginUser() -> clearUserRecords() -> loadRecordsFromFile()
 * - 隔离范围：记录、统计、错题集全部隔离
 *
 * 【性能指标】
 * - 加载记录：O(M)，M 为记录总数（逐行解析 + 索引构建）
 * - 答题记录：O(1) 内存更新 + O(1) 文件追加
 * - 错题集查询：O(1)（unordered_set 查找）
 *
 * 【异常处理】
 * - CSV 解析错误：跳过该行，继续解析（鲁棒性优先）
 * - 文件不存在：不报错，视为首次使用（用户友好）
 * - 写入失败：输出警告，不影响内存记录（保证程序继续运行）
 *
 * 【与其他模块协作】
 * - Stats.cpp：依赖本模录的 g_records、g_recordsByQuestion 构建统计
 * - Recommender.cpp：根据统计信息（正确率、用时）计算推荐分数
 * - App.cpp：调用 doQuestion() 实现各种练习模式
 * - Utils.cpp：调用 getDataDir() 获取 data 目录路径
 */

#include "Record.h"
#include "Stats.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>

// ============================================================
// 全局变量定义（对应 Record.h 中的 extern 声明）
// ============================================================

/**
 * @brief 全局做题记录列表（时间序列）
 *
 * 存储所有历史作答记录，按时间顺序追加。
 * 加载时机：loadRecordsFromFile()
 * 更新时机：doQuestion() 中每次答题后 push_back
 */
std::vector<Record> g_records;

/**
 * @brief 按题号分组的做题记录索引
 *
 * 键：题号（questionId）
 * 值：该题的所有作答记录（按时间顺序）
 *
 * 用途：
 * - 快速查询某题的历史记录（统计模块使用）
 * - 判断最后一次作答结果（错题集构建）
 */
std::unordered_map<int, std::vector<Record>> g_recordsByQuestion;

/**
 * @brief 当前错题集合（最后一次答错的题号）
 *
 * 维护规则：
 * - 最后一次答对：从集合中移除
 * - 最后一次答错：加入集合
 *
 * 更新时机：
 * - loadRecordsFromFile() 加载后根据历史记录重建
 * - doQuestion() 每次答题后动态更新
 */
std::unordered_set<int> g_wrongQuestions;

/**
 * @brief 当前登录用户的 ID（学号或用户名）
 *
 * 用于多用户隔离：
 * - 每个用户的记录存储于独立文件（records_<userId>.csv）
 * - 统计、错题集全部隔离
 *
 * 默认值：空字符串（使用默认文件 records.csv）
 */
std::string g_currentUserId;

// ============================================================
// 多用户管理函数
// ============================================================

/**
 * @brief 获取当前用户的记录文件路径
 *
 * 【实现逻辑】
 * 1. 调用 Utils.cpp 的 getDataDir() 获取 data 目录（自动创建）
 * 2. 根据 g_currentUserId 是否为空，返回不同的文件路径：
 *    - 空：返回默认文件 "data/records.csv"（单用户模式）
 *    - 非空：返回 "data/records_<userId>.csv"（多用户模式）
 *
 * 【设计意图】
 * - 多用户隔离：每个用户拥有独立的记录文件
 * - 向后兼容：默认路径保持不变，不影响旧数据
 *
 * 【路径拼接】
 * 使用 std::filesystem::path 的 operator/ 自动处理平台差异：
 * - Windows: data\records_202001.csv
 * - Linux/Mac: data/records_202001.csv
 *
 * @return std::string 绝对路径或相对路径（取决于 getDataDir() 的返回值）
 * @complexity O(1)
 *
 * @note 该函数不创建文件，仅返回路径字符串
 * @note 调用时机：loadRecordsFromFile()、appendRecordToFile() 前
 */
std::string getRecordFilePath() {
    std::filesystem::path dataDir = getDataDir();  // 获取 data 目录（自动创建）

    // 单用户模式：使用默认文件名
    if (g_currentUserId.empty()) {
        return (dataDir / "records.csv").string();
    }

    // 多用户模式：文件名包含用户 ID
    return (dataDir / ("records_" + g_currentUserId + ".csv")).string();
}

/**
 * @brief 用户登录：设置当前用户 ID
 *
 * 【功能】
 * 设置全局变量 g_currentUserId，后续所有操作将使用该用户的记录文件。
 *
 * 【调用时机】
 * - 程序启动时（App.cpp）
 * - 用户切换时（需先调用 clearUserRecords() 清空旧数据）
 *
 * 【典型流程】
 * ```cpp
 * loginUser("202001");              // 设置用户 ID
 * clearUserRecords();               // 清空旧用户的内存数据
 * loadRecordsFromFile(getRecordFilePath());  // 加载新用户的记录
 * ```
 *
 * @param userId 用户标识（学号或用户名，非空）
 * @complexity O(1)
 *
 * @note 该函数仅设置 ID，不加载记录文件（需手动调用 loadRecordsFromFile）
 * @note 会在控制台输出"当前用户：<userId>"（用于调试和用户确认）
 */
void loginUser(const std::string& userId) {
    g_currentUserId = userId;
    std::cout << "当前用户：" << g_currentUserId << std::endl;
}

/**
 * @brief 清空当前用户的所有记录数据（用于切换用户）
 *
 * 【功能】
 * 清空三个全局容器，释放内存。
 * 清空内容：
 * - g_records：所有历史记录
 * - g_recordsByQuestion：按题号索引
 * - g_wrongQuestions：错题集
 *
 * 【调用时机】
 * 1. 切换用户前（避免数据混淆）
 * 2. loadRecordsFromFile() 函数开头（防止重复加载）
 *
 * 【为什么需要显式清空】
 * - 避免多次加载同一文件导致记录重复
 * - 避免切换用户后数据混淆（A 用户的错题集混入 B 用户）
 *
 * @complexity O(M)，M 为当前记录数量（vector/map/set 的 clear 操作）
 *
 * @note 该函数仅清空内存数据，不删除文件
 * @note 清空后需重新调用 loadRecordsFromFile() 加载新用户的记录
 */
void clearUserRecords() {
    g_records.clear();              // 清空时间序列记录
    g_recordsByQuestion.clear();    // 清空题号索引
    g_wrongQuestions.clear();       // 清空错题集
}

// ============================================================
// 记录持久化函数
// ============================================================

/**
 * @brief 从 CSV 文件加载历史做题记录
 *
 * 【核心流程】
 * 1. 清空旧数据（调用 clearUserRecords）
 * 2. 打开 CSV 文件（不存在则提示首次使用）
 * 3. 逐行解析 CSV（跳过空行、格式错误行）
 * 4. 重建三个全局容器：
 *    - g_records：按时间顺序追加
 *    - g_recordsByQuestion：按题号分组
 *    - g_wrongQuestions：根据最后一次作答结果构建
 * 5. 调用 buildQuestionStats() 构建统计信息（Stats.cpp）
 * 6. 输出加载摘要信息
 *
 * 【CSV 解析策略】
 * - 分隔符：逗号（,）
 * - 列数要求：至少 4 列（questionId, correct, usedSeconds, timestamp）
 * - 空行：跳过（防止文件末尾多余换行）
 * - 解析失败：跳过该行（try-catch 捕获所有异常）
 *
 * 【错题集构建算法】
 * 遍历 g_recordsByQuestion（题号 -> 记录列表）：
 * - 取每题的最后一条记录（vec.back()）
 * - 若最后一次答错（!correct），加入 g_wrongQuestions
 *
 * 【为什么用"最后一次"而非"任意一次答错"】
 * - 设计意图：错题集反映当前未掌握的知识点
 * - 如果后来答对了，说明已掌握，应从错题集移除
 *
 * 【异常处理】
 * - 文件不存在：不报错，返回 true（首次使用场景）
 * - 字段解析失败：跳过该行，继续解析（鲁棒性优先）
 * - 格式错误不影响整体加载
 *
 * 【边界条件】
 * - 空文件：g_records 为空，正常返回
 * - 单条记录：正常加载
 * - 同一题多次作答：按时间顺序全部加载
 *
 * @param filename 记录文件路径（通常由 getRecordFilePath() 返回）
 * @return true  总是返回 true（文件不存在也视为成功）
 * @complexity O(M + N)，M 为记录数，N 为题目数（解析 + 错题集构建）
 *
 * @note 该函数会清空当前内存中的所有记录（调用 clearUserRecords）
 * @note 加载后会自动调用 buildQuestionStats() 更新统计信息
 */
bool loadRecordsFromFile(const std::string& filename) {
    // 步骤 1：清空旧数据（避免重复加载）
    clearUserRecords();

    // 步骤 2：打开 CSV 文件
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        // 文件不存在不算错误，可能是首次使用
        std::cout << "未找到当前用户的做题记录文件，将从空记录开始。\n";
        return true;  // 返回 true 表示"加载成功"（空记录）
    }

    // 步骤 3：逐行解析 CSV
    std::string line;
    while (std::getline(fin, line)) {
        // 跳过空行（文件末尾可能有多余换行）
        if (line.empty()) continue;

        // 使用 stringstream 按逗号分割字段
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        // 列数检查：至少需要 4 列
        if (fields.size() < 4) continue;

        // 步骤 4：解析字段并构建 Record 对象
        Record r;
        try {
            r.questionId = std::stoi(fields[0]);    // 题号（整数）
            r.correct = (fields[1] == "1");         // 正误（1 表示正确，0 表示错误）
            r.usedSeconds = std::stoi(fields[2]);   // 用时（秒）
            r.timestamp = std::stoll(fields[3]);    // 时间戳（64 位整数）
        } catch (...) {
            // 捕获所有异常：stoi/stoll 解析失败、数值越界等
            // 跳过该行，继续解析下一行（鲁棒性优先）
            continue;
        }

        // 步骤 5：更新全局容器
        g_records.push_back(r);                         // 追加到时间序列
        g_recordsByQuestion[r.questionId].push_back(r); // 添加到题号索引
    }

    // 步骤 6：根据"最后一次作答结果"构建错题集
    for (auto& p : g_recordsByQuestion) {
        int qid = p.first;                // 题号
        auto& vec = p.second;             // 该题的所有记录（按时间顺序）

        if (!vec.empty()) {
            const Record& last = vec.back();  // 取最后一次作答记录
            if (!last.correct) {
                // 最后一次答错 -> 加入错题集
                g_wrongQuestions.insert(qid);
            }
            // 最后一次答对 -> 不在错题集中（隐式逻辑）
        }
    }

    // 步骤 7：输出加载摘要
    std::cout << "历史做题记录加载完成，共 " << g_records.size() << " 条，当前错题数 "
              << g_wrongQuestions.size() << " 道。\n";

    // 步骤 8：构建统计信息（调用 Stats.cpp 的函数）
    buildQuestionStats();

    return true;
}

/**
 * @brief 追加单条记录到 CSV 文件
 *
 * 【核心流程】
 * 1. 以追加模式（std::ios::app）打开文件
 * 2. 写入一行 CSV 数据：questionId,correct,usedSeconds,timestamp
 * 3. 自动换行（末尾 \n）
 * 4. 文件流析构时自动 flush 和关闭
 *
 * 【为什么用追加模式】
 * - 避免覆盖原有记录（std::ios::out 会清空文件）
 * - 每次答题后立即写入，防止程序异常退出导致数据丢失
 * - 性能：追加写入比重写整个文件高效
 *
 * 【CSV 格式】
 * 字段顺序：questionId,correct,usedSeconds,timestamp
 * 示例：1001,1,45,1703145600
 * 解释：
 * - 题号 1001
 * - 答对（1 表示 true，0 表示 false）
 * - 用时 45 秒
 * - 时间戳 1703145600（Unix epoch 秒数）
 *
 * 【异常处理】
 * - 文件打开失败：输出警告，不抛异常（保证程序继续运行）
 * - 不影响内存中的记录（g_records 已更新）
 * - 场景：磁盘空间不足、权限不足、路径无效
 *
 * 【为什么不检查写入是否成功】
 * - fout << ... 写入失败时 failbit 会被设置，但不影响程序继续运行
 * - 设计权衡：记录丢失（极少见）vs 程序崩溃（用户体验差）
 * - 内存中的记录仍然有效，用户可继续刷题
 *
 * @param r 做题记录（来自 doQuestion）
 * @param filename 记录文件路径（通常由 getRecordFilePath() 返回）
 * @complexity O(1) - 单次文件追加操作
 *
 * @note 该函数不会读取文件，仅追加一行
 * @note 文件流会在函数结束时自动关闭（RAII 机制）
 * @note 追加模式下，若文件不存在会自动创建
 */
void appendRecordToFile(const Record& r, const std::string& filename) {
    // 以追加模式打开文件（不存在则创建）
    std::ofstream fout(filename, std::ios::app);

    if (!fout.is_open()) {
        // 文件打开失败：输出警告，不影响程序继续运行
        std::cout << "警告：无法写入做题记录文件。\n";
        return;
    }

    // 写入一行 CSV 数据
    fout << r.questionId << ','              // 题号
         << (r.correct ? 1 : 0) << ','       // 正误（1/0）
         << r.usedSeconds << ','             // 用时（秒）
         << r.timestamp << '\n';             // 时间戳 + 换行

    // fout 析构时自动调用 close()，flush 缓冲区
}

// ============================================================
// 答题流程函数
// ============================================================

/**
 * @brief 完成一道题的完整答题流程
 *
 * 【核心流程】
 * 1. 展示题目信息：题号、知识点、难度、题目文本、选项列表
 * 2. 等待用户输入答案（整数，对应选项下标）
 * 3. 计时：使用 std::chrono::steady_clock 记录思考时间
 * 4. 判分：比较用户答案与正确答案（q.answer）
 * 5. 输出反馈：正确或错误（错误时显示正确答案）
 * 6. 构造 Record 对象（题号、正误、用时、时间戳）
 * 7. 更新三个全局容器：
 *    - g_records：追加到时间序列
 *    - g_recordsByQuestion[qid]：追加到题号索引
 *    - g_wrongQuestions：动态维护错题集
 * 8. 持久化：调用 appendRecordToFile() 追加到 CSV
 *
 * 【计时机制】
 * - 时钟类型：steady_clock（单调时钟，不受系统时间调整影响）
 * - 精度：秒级（duration_cast<seconds>）
 * - 开始：输出"请输入你的答案编号："后
 * - 结束：用户按回车键后
 * - 边界：用时为 0 时设为 1 秒（避免统计异常）
 *
 * 【错题集维护算法】
 * 规则：基于"最后一次作答结果"
 * - 答对（correct == true）：从错题集移除（erase）
 * - 答错（correct == false）：加入错题集（insert）
 * 设计意图：
 * - 错题集反映当前未掌握的知识点
 * - 如果后来答对了，说明已掌握，不再是"错题"
 *
 * 【为什么先更新内存再写文件】
 * 1. 内存更新失败概率极低（除非内存耗尽）
 * 2. 文件写入可能失败（磁盘满、权限不足）
 * 3. 优先保证内存数据一致性（统计、推荐功能依赖内存数据）
 * 4. 文件写入失败不影响程序继续运行
 *
 * 【边界条件处理】
 * - 用时为 0：设为 1 秒（极短思考时间，避免除零、异常统计）
 * - 非法输入（非整数）：std::cin 读取失败，userAns 保持未初始化值
 *   → 当前实现视为错误答案（下标越界或不等于正确答案）
 *   → 改进建议：检查 std::cin.fail()，提示用户重新输入
 * - 选项下标越界：不检查，由判分逻辑处理（userAns != q.answer）
 *
 * 【时间戳的用途】
 * - 统计分析：学习曲线、时间分布
 * - 间隔计算：艾宾浩斯遗忘曲线（推荐系统）
 * - 学习报告：按日期汇总
 *
 * @param q 题目对象（来自 g_questionById 或其他题库容器）
 * @complexity O(1) - 内存更新 + 文件追加均为常数时间
 *
 * @note 该函数会阻塞等待用户输入（同步 I/O）
 * @note 计时精度为秒级，不适合毫秒级统计
 * @note 文件写入失败不会抛异常，仅输出警告
 */
void doQuestion(const Question& q) {
    using namespace std::chrono;

    // ======== 步骤 1：展示题目信息 ========
    std::cout << "题号: " << q.id << std::endl;
    std::cout << "[知识点] " << q.knowledge << "    [难度] " << q.difficulty << std::endl;
    std::cout << q.text << std::endl;

    // 遍历选项列表，显示"下标. 选项内容"
    for (size_t i = 0; i < q.options.size(); ++i) {
        std::cout << i << ". " << q.options[i] << std::endl;
    }

    // ======== 步骤 2-3：计时 + 等待输入（使用健壮输入函数）========
    auto start = steady_clock::now();  // 记录开始时间（单调时钟）

    int userAns = -1;  // 初始化为非法值
    std::string line;
    bool inputValid = false;

    // 循环读取，直到获取有效输入
    while (!inputValid) {
        std::cout << "请输入你的答案编号：";
        
        // 【关键修复】检查 getline 是否成功，防止 EOF/错误状态导致无限循环
        if (!std::getline(std::cin, line)) {
            // 输入流已关闭（EOF）或出错
            if (std::cin.eof()) {
                std::cerr << "\n检测到输入结束（EOF），本题判错并退出。\n";
            } else {
                std::cerr << "\n输入流异常，本题判错。\n";
                std::cin.clear();  // 尝试恢复输入流状态
            }
            // 强制跳出循环，使用默认错误答案
            userAns = -1;
            break;
        }

        // 【安全的空白字符去除】先检查是否为空或全空白
        size_t startPos = line.find_first_not_of(" \t\n\r");
        if (startPos == std::string::npos) {
            // 空行或全空白字符
            std::cout << "输入为空，请重新输入。\n";
            continue;
        }
        
        // 去除前导空白
        line.erase(0, startPos);
        
        // 去除尾部空白（此时 line 至少有一个非空白字符，所以 find_last_not_of 不会返回 npos）
        size_t endPos = line.find_last_not_of(" \t\n\r");
        if (endPos != std::string::npos) {
            line.erase(endPos + 1);
        }

        // 尝试解析整数
        std::stringstream ss(line);
        if (ss >> userAns && ss.eof()) {
            // 解析成功且无多余字符
            if (userAns >= 0 && userAns < (int)q.options.size()) {
                inputValid = true;
            } else {
                std::cout << "选项超出范围（请输入 0-" << (q.options.size() - 1) << "），请重新输入。\n";
            }
        } else {
            std::cout << "输入无效（请输入数字），请重新输入。\n";
        }
    }

    auto end = steady_clock::now();  // 记录结束时间

    // 计算用时（秒）
    int usedSeconds = (int)duration_cast<seconds>(end - start).count();
    // 边界处理：用时为 0 时设为 1 秒（避免统计异常、除零错误）
    if (usedSeconds <= 0) usedSeconds = 1;

    // ======== 步骤 4：判分 ========
    bool correct = (userAns == q.answer);

    // ======== 步骤 5：输出反馈 ========
    if (correct) {
        std::cout << "回答正确！\n";
    } else {
        // 显示正确答案：下标 + 选项内容
        std::cout << "回答错误，正确答案是：" << q.answer
                  << "，" << q.options[q.answer] << std::endl;
    }

    // ======== 步骤 6：构建 Record 对象 ========
    Record r;
    r.questionId = q.id;                          // 题号
    r.correct = correct;                          // 正误
    r.usedSeconds = usedSeconds;                  // 用时（秒）
    r.timestamp = (long long)std::time(nullptr);  // 当前时间戳（Unix epoch 秒数）

    // ======== 步骤 7：更新内存结构 ========
    g_records.push_back(r);                       // 追加到全局时间序列
    g_recordsByQuestion[q.id].push_back(r);       // 追加到题号索引

    // 动态维护错题集：最后一次答对移除，答错加入
    if (correct) {
        g_wrongQuestions.erase(q.id);   // O(1) 平均复杂度
    } else {
        g_wrongQuestions.insert(q.id);  // O(1) 平均复杂度
    }

    // ======== 步骤 8：持久化到文件 ========
    // 使用当前用户的记录路径（多用户隔离）
    appendRecordToFile(r, getRecordFilePath());
}
