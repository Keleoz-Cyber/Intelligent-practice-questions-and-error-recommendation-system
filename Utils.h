/**
 * @file Utils.h
 * @brief 通用工具函数模块
 *
 * 提供清屏、暂停、路径定位等跨平台实用功能。
 *
 * @author DS_AI_Quiz Team
 * @date 2025
 */

#pragma once

#include <string>
#include <filesystem>
#include <random>

/**
 * @brief 清屏函数
 *
 * 根据不同操作系统使用不同的清屏方法：
 * - Windows：调用 system("cls")
 * - Linux/macOS：使用 ANSI 转义序列 "\033[2J\033[H"
 *   - \033[2J：清空整个屏幕
 *   - \033[H：将光标移动到左上角
 *
 * @note 已知问题：
 * - 在某些 IDE 集成控制台（如 CLion、Visual Studio 内置终端）中可能清屏不稳定
 * - 推荐在独立的终端窗口中运行程序以获得最佳体验
 * - ANSI 转义序列在老旧的 Windows 控制台中可能不支持（Windows 10 之前）
 *
 * @see runMenuLoop() 本函数在主菜单循环开头被调用
 */
void clearScreen();

/**
 * @brief 暂停等待用户按回车键继续
 *
 * 显示提示信息，清空输入缓冲区，等待用户按回车键后返回。
 *
 * 实现细节：
 * 1. 输出提示信息（默认："按回车键继续..."）
 * 2. 调用 cin.ignore() 清空输入缓冲区中的残留字符（包括换行符）
 *    - 参数 1：numeric_limits<streamsize>::max() 表示忽略的最大字符数
 *    - 参数 2：'\n' 表示遇到换行符停止
 * 3. 调用 getline() 等待用户按回车
 *
 * @param message 提示信息，默认为 "按回车键继续..."
 *
 * @note 关键技术点：
 * - 使用括号包裹 numeric_limits<streamsize>::max()，避免 Windows.h 的 max 宏污染
 * - Windows.h 定义了 max 宏，会与 std::numeric_limits::max() 冲突
 * - 解决方法 1：使用 NOMINMAX 宏禁止 min/max 宏定义（已在 Utils.cpp 中定义）
 * - 解决方法 2：使用括号包裹 (std::numeric_limits<streamsize>::max)()
 *
 * @warning 如果不清空缓冲区，可能会导致程序不等待用户输入就直接继续执行
 *
 * @see switchUser() 切换用户时也需要调用 cin.ignore() 清空缓冲区
 */
void pauseForUser(const std::string& message = "按回车键继续...");

/**
 * @brief 获取可执行文件所在目录
 *
 * 自动定位程序可执行文件的所在目录，实现跨平台路径定位。
 *
 * 实现策略：
 * - Windows：使用 GetModuleFileNameW() 获取可执行文件完整路径
 *   - 返回 Unicode 宽字符路径（wchar_t），最大长度 MAX_PATH
 *   - 成功后调用 parent_path() 获取父目录
 *   - 失败则 fallback 到当前工作目录（current_path）
 *
 * - Linux/macOS：读取符号链接 /proc/self/exe（Linux）
 *   - 使用 readlink() 读取符号链接目标
 *   - 成功后调用 parent_path() 获取父目录
 *   - 失败则 fallback 到当前工作目录（current_path）
 *   - 注意：macOS 不支持 /proc/self/exe，需要使用 _NSGetExecutablePath()（未实现）
 *
 * 跨平台处理：
 * - 使用 std::filesystem::path 统一处理路径分隔符（Windows 使用 \，Unix 使用 /）
 * - filesystem 库自动处理路径规范化
 *
 * @return std::filesystem::path 可执行文件所在目录的绝对路径
 *
 * @note 优点：
 * - 无论从哪个目录运行程序，都能正确定位数据文件
 * - 避免依赖当前工作目录（current_path 会随运行位置变化）
 * - 支持 CI/CD 环境和不同部署场景
 *
 * @warning 如果获取失败，会 fallback 到当前工作目录并输出警告信息
 *
 * @see getDataDir() 基于本函数获取数据目录
 * @see getReportsDir() 基于本函数获取报告目录
 */
std::filesystem::path getExeDir();

/**
 * @brief 获取数据目录（exeDir/data）
 *
 * 返回题库数据文件所在的目录路径（相对于可执行文件）。
 *
 * 路径结构：
 * - 可执行文件目录/data/
 *   - questions.txt（题库文件）
 *   - knowledge_graph.txt（知识图谱）
 *   - user_xxx.txt（用户做题记录）
 *
 * @return std::filesystem::path 数据目录的绝对路径
 *
 * @note 本函数只返回路径，不检查目录是否存在
 * @note 目录由 main.cpp 初始化时自动创建（如果不存在）
 *
 * @see getExeDir() 获取可执行文件目录
 */
std::filesystem::path getDataDir();

/**
 * @brief 获取报告目录（exeDir/reports）
 *
 * 返回学习报告输出目录的路径（相对于可执行文件）。
 *
 * 路径结构：
 * - 可执行文件目录/reports/
 *   - report_用户名_时间戳.txt（学习报告文件）
 *
 * @return std::filesystem::path 报告目录的绝对路径
 *
 * @note 本函数只返回路径，不检查目录是否存在
 * @note 目录由 main.cpp 初始化时自动创建（如果不存在）
 *
 * @see getExeDir() 获取可执行文件目录
 * @see exportLearningReport() 导出学习报告（Report 模块）
 */
std::filesystem::path getReportsDir();

/**
 * @brief 安全读取整数（健壮的输入处理）
 *
 * 【功能】
 * 提供健壮的整数输入功能，避免 std::cin >> int 的 failbit 问题。
 *
 * 【处理流程】
 * 1. 打印提示信息（prompt）
 * 2. 使用 std::getline 读取整行输入（避免 cin fail state）
 * 3. 使用 std::stringstream 尝试解析为整数
 * 4. 验证输入是否为合法整数（完整解析，无多余字符）
 * 5. 验证范围：[minVal, maxVal]
 * 6. 输入非法时循环提示重新输入
 *
 * 【输入验证】
 * - 非数字字符：提示"请输入有效的数字"
 * - 范围外：提示"请输入 [minVal, maxVal] 范围内的数字"
 * - 空行处理：根据 allowEmpty 参数决定是否允许
 *
 * @param prompt 提示信息（例如："请选择："）
 * @param out 输出参数，存储成功读取的整数
 * @param minVal 允许的最小值
 * @param maxVal 允许的最大值
 * @param allowEmpty 是否允许空输入（默认 false，空输入会提示重新输入）
 * @return true 成功读取到合法整数
 * @return false 用户输入空行且 allowEmpty=true（用于取消操作）
 *
 * @note 使用 getline + stringstream 避免 cin 进入 fail state
 * @note 循环直到输入合法，不会因非法输入而崩溃或退出
 *
 * @complexity O(1) 单次输入解析
 */
bool readIntSafely(const std::string& prompt, int& out, int minVal, int maxVal, bool allowEmpty = false);

/**
 * @brief 获取全局随机数生成器（线程安全）
 *
 * 【功能】
 * 提供一个全局唯一的 std::mt19937 随机数生成器，用于所有随机数需求。
 *
 * 【初始化策略】
 * - 使用静态局部变量（函数内 static）实现单例模式
 * - 首次调用时使用 std::random_device 初始化种子（高质量随机种子）
 * - 线程安全（C++11 保证静态局部变量初始化的线程安全性）
 *
 * 【设计目的】
 * - 统一随机数风格：避免混用 rand()/mt19937/random_device
 * - 避免重复播种：不再每次调用 srand(time(nullptr))
 * - 提高随机质量：mt19937 优于传统 rand()
 *
 * @return std::mt19937& 全局随机数生成器的引用
 *
 * @note 返回引用避免拷贝（mt19937 状态较大）
 * @note 所有模块（App/Record/Recommender）应使用此函数获取 RNG
 * @note 配合 std::uniform_int_distribution 使用
 *
 * @example
 * std::uniform_int_distribution<int> dist(0, 9);
 * int randomNum = dist(globalRng());
 *
 * @complexity O(1) 首次调用初始化，后续直接返回引用
 */
std::mt19937& globalRng();
