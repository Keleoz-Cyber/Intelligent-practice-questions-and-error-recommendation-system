/**
 * @file Utils.cpp
 * @brief 通用工具函数模块实现
 *
 * 实现清屏、暂停、路径定位等跨平台实用功能。
 *
 * @author DS_AI_Quiz Team
 * @date 2025
 */

#include "Utils.h"
#include <iostream>
#include <sstream>
#include <limits>
#include <cstdlib>

// 跨平台头文件处理
#ifdef _WIN32
#define NOMINMAX  // 防止 Windows.h 定义 min/max 宏，避免与 std::numeric_limits 冲突
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

/**
 * @brief 清屏函数实现
 *
 * Windows 平台：
 * - 使用 system("cls") 调用 Windows 控制台清屏命令
 * - 优点：简单直接，兼容性好
 * - 缺点：有一定性能开销（启动子进程）
 *
 * Linux/macOS 平台：
 * - 使用 ANSI 转义序列 "\033[2J\033[H"
 *   - \033[2J：清空整个屏幕缓冲区
 *   - \033[H：将光标移动到屏幕左上角 (1,1) 位置
 * - 优点：性能好，无需启动子进程
 * - 缺点：老旧终端可能不支持 ANSI 转义序列
 *
 * 已知问题：
 * - 某些 IDE 集成控制台（CLion、Visual Studio、VS Code 内置终端）可能清屏不稳定
 * - 原因：IDE 控制台可能不完全模拟标准终端行为
 * - 解决方案：推荐在独立的终端窗口中运行程序
 */
void clearScreen() {
#ifdef _WIN32
    system("cls");  // Windows 控制台清屏命令
#else
    // 使用 ANSI 转义序列清屏（Linux/macOS）
    std::cout << "\033[2J\033[H";
#endif
}

/**
 * @brief 暂停等待用户按回车键继续
 *
 * 实现细节：
 * 1. 输出提示信息（默认："按回车键继续..."）
 * 2. 清空输入缓冲区，避免残留字符干扰
 * 3. 等待用户按回车键
 *
 * 关键技术点 - cin.ignore() 的使用：
 * - 功能：清空输入缓冲区中的残留字符（如上一次输入留下的换行符）
 * - 参数 1：最大忽略字符数，这里使用 numeric_limits<streamsize>::max()
 * - 参数 2：停止字符，这里使用 '\n'（遇到换行符停止）
 * - 效果：忽略缓冲区中所有字符，直到遇到换行符或达到最大字符数
 *
 * 关键技术点 - NOMINMAX 宏污染问题：
 * - 问题：Windows.h 定义了 max 和 min 宏，会与 std::numeric_limits::max() 冲突
 * - 错误示例：std::numeric_limits<std::streamsize>::max() 被宏展开为：
 *   std::numeric_limits<std::streamsize>::(((a) > (b)) ? (a) : (b))() // 编译错误！
 *
 * - 解决方案 1（本项目采用）：在包含 Windows.h 之前定义 NOMINMAX 宏
 *   #define NOMINMAX
 *   #include <windows.h>
 *   效果：禁止 Windows.h 定义 min/max 宏
 *
 * - 解决方案 2：使用括号包裹 max()，避免宏展开
 *   std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
 *   效果：括号阻止宏替换，但代码可读性降低
 *
 * 为什么需要清空缓冲区：
 * - 场景：用户在菜单输入数字 "1" 后按回车，cin >> choice 读取 "1"，但换行符残留在缓冲区
 * - 如果不调用 cin.ignore()，getline() 会立即读取到残留的换行符，程序不等待用户输入就继续执行
 * - 调用 cin.ignore() 后，换行符被清除，getline() 才能正确等待用户按回车
 *
 * @param message 提示信息，默认为 "按回车键继续..."
 *
 * @see switchUser() 切换用户时也需要调用 cin.ignore() 清空缓冲区
 */
void pauseForUser(const std::string& message) {
    std::cout << "\n" << message;

    // 移除 cin.ignore()，因为项目已统一使用 getline 读取输入
    // 缓冲区中不会残留换行符，无需清空
    // std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    // 等待用户按回车键，读取的内容无需使用，直接丢弃
    std::string dummy;
    std::getline(std::cin, dummy);
}

/**
 * @brief 获取可执行文件所在目录
 *
 * 自动定位程序可执行文件的所在目录，支持跨平台。
 *
 * Windows 平台实现：
 * - API：GetModuleFileNameW(NULL, buffer, MAX_PATH)
 *   - NULL：获取当前进程的可执行文件路径
 *   - buffer：宽字符（wchar_t）缓冲区，存储路径
 *   - MAX_PATH：Windows 定义的最大路径长度（通常为 260）
 * - 返回值：实际写入的字符数（不包括 null 终止符）
 * - 成功判断：length > 0 且 length < MAX_PATH
 * - 后处理：使用 filesystem::path 构造路径对象，调用 parent_path() 获取父目录
 * - 失败处理：输出警告，fallback 到当前工作目录（current_path）
 *
 * Linux 平台实现：
 * - 原理：读取符号链接 /proc/self/exe，它指向当前进程的可执行文件
 * - API：readlink("/proc/self/exe", buffer, sizeof(buffer) - 1)
 *   - 返回值：读取的字节数（-1 表示失败）
 *   - 注意：readlink 不自动添加 null 终止符，需要手动添加
 * - 后处理：手动添加 '\0'，使用 filesystem::path 构造路径对象，调用 parent_path() 获取父目录
 * - 失败处理：输出警告，fallback 到当前工作目录（current_path）
 *
 * macOS 平台注意事项：
 * - macOS 不支持 /proc/self/exe
 * - 可选方案：使用 _NSGetExecutablePath() 函数（需要包含 <mach-o/dyld.h>）
 * - 当前实现：fallback 到当前工作目录（未实现 macOS 专用代码）
 *
 * 跨平台处理优势：
 * - 使用 std::filesystem::path 统一处理路径分隔符（Windows: \，Unix: /）
 * - 自动路径规范化，避免手动处理路径字符串
 * - 支持 Unicode 路径（Windows 使用 wchar_t）
 *
 * 应用场景：
 * - 无论从哪个目录运行程序（如 ./build/app 或 cd build && ./app），都能正确定位数据文件
 * - 避免依赖当前工作目录（current_path 会随运行位置变化）
 * - 支持 CI/CD 环境、打包部署等场景
 *
 * @return std::filesystem::path 可执行文件所在目录的绝对路径
 *
 * @warning 如果获取失败，会输出警告信息并 fallback 到当前工作目录
 *
 * @see getDataDir() 基于本函数获取数据目录
 * @see getReportsDir() 基于本函数获取报告目录
 */
std::filesystem::path getExeDir() {
#ifdef _WIN32
    // Windows 平台：使用 GetModuleFileNameW 获取可执行文件路径（Unicode）
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH);

    if (length > 0 && length < MAX_PATH) {
        // 成功获取路径，构造 path 对象并返回父目录
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    } else {
        // 获取失败，fallback 到当前工作目录
        std::cerr << "警告：无法获取可执行文件路径，使用当前工作目录作为 fallback。\n";
        return std::filesystem::current_path();
    }
#else
    // 非 Windows 平台：尝试读取 /proc/self/exe (Linux) 或其他方法
    char buffer[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    if (length != -1) {
        // 成功读取符号链接，手动添加 null 终止符
        buffer[length] = '\0';
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    } else {
        // 获取失败（可能是 macOS 或其他不支持 /proc/self/exe 的系统）
        // fallback 到当前工作目录
        std::cerr << "警告：无法获取可执行文件路径，使用当前工作目录作为 fallback。\n";
        return std::filesystem::current_path();
    }
#endif
}

/**
 * @brief 获取数据目录（exeDir/data）
 *
 * 返回题库数据文件所在的目录路径。
 *
 * 路径构造：
 * - 使用 getExeDir() 获取可执行文件目录
 * - 使用 / 运算符拼接子目录 "data"（filesystem::path 重载了 / 运算符）
 * - filesystem 自动处理路径分隔符（Windows: \，Unix: /）
 *
 * 目录结构示例：
 * - Windows：C:\Program Files\DS_AI_Quiz\data\
 * - Linux：/home/user/DS_AI_Quiz/data/
 *
 * 存储内容：
 * - questions.txt：题库文件
 * - knowledge_graph.txt：知识图谱文件
 * - user_xxx.txt：各用户的做题记录文件
 *
 * @return std::filesystem::path 数据目录的绝对路径
 *
 * @note 本函数只返回路径，不检查目录是否存在
 * @note 目录创建由 main.cpp 初始化时处理（使用 create_directories）
 *
 * @see getExeDir() 获取可执行文件目录
 * @see loadQuestionsFromFile() 加载题库文件（Question 模块）
 * @see loadKnowledgeGraphFromFile() 加载知识图谱（KnowledgeGraph 模块）
 * @see loadRecordsFromFile() 加载用户做题记录（Record 模块）
 */
std::filesystem::path getDataDir() {
    return getExeDir() / "data";
}

/**
 * @brief 获取报告目录（exeDir/reports）
 *
 * 返回学习报告输出目录的路径。
 *
 * 路径构造：
 * - 使用 getExeDir() 获取可执行文件目录
 * - 使用 / 运算符拼接子目录 "reports"（filesystem::path 重载了 / 运算符）
 * - filesystem 自动处理路径分隔符（Windows: \，Unix: /）
 *
 * 目录结构示例：
 * - Windows：C:\Program Files\DS_AI_Quiz\reports\
 * - Linux：/home/user/DS_AI_Quiz/reports/
 *
 * 存储内容：
 * - report_用户名_时间戳.txt：各用户的学习报告文件
 * - 文件名格式：report_<userId>_<timestamp>.txt
 *
 * @return std::filesystem::path 报告目录的绝对路径
 *
 * @note 本函数只返回路径，不检查目录是否存在
 * @note 目录创建由 main.cpp 初始化时处理（使用 create_directories）
 *
 * @see getExeDir() 获取可执行文件目录
 * @see exportLearningReport() 导出学习报告（Report 模块）
 */
std::filesystem::path getReportsDir() {
    return getExeDir() / "reports";
}

/**
 * @brief 安全读取整数实现
 *
 * 【核心算法】
 * 1. 使用 std::getline 读取整行（避免 cin >> 的 failbit）
 * 2. 使用 std::stringstream 解析整数
 * 3. 验证完整解析（ss >> out 成功且 ss.eof()）
 * 4. 验证范围 [minVal, maxVal]
 * 5. 失败时循环提示重新输入
 *
 * 【为什么用 getline + stringstream】
 * - cin >> int 在输入非数字时会进入 fail state
 * - fail state 导致后续所有 cin 操作失败（死循环/崩溃）
 * - getline 读取整行，即使输入非法也不影响 cin 状态
 * - stringstream 解析失败仅影响局部变量，不污染全局状态
 *
 * 【完整解析验证】
 * - ss >> out 成功：至少解析了一个整数
 * - ss.eof() 为真：字符串已完全解析，无多余字符
 * - 例如："123abc" 会被拒绝（123 后有多余字符 abc）
 *
 * @complexity O(N)，N 为输入字符串长度（通常很小）
 */
bool readIntSafely(const std::string& prompt, int& out, int minVal, int maxVal, bool allowEmpty) {
    std::string line;
    while (true) {
        std::cout << prompt;
        std::getline(std::cin, line);

        // 去除前后空白字符
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        // 允许空输入且输入为空，返回 false
        if (allowEmpty && line.empty()) {
            return false;
        }

        // 空输入且不允许，提示重新输入
        if (line.empty()) {
            std::cout << "输入不能为空，请重新输入。\n";
            continue;
        }

        // 尝试解析整数
        std::stringstream ss(line);
        int value;
        if (ss >> value && ss.eof()) {
            // 解析成功且无多余字符
            if (value >= minVal && value <= maxVal) {
                out = value;
                return true;
            } else {
                std::cout << "输入超出范围，请输入 [" << minVal << ", " << maxVal << "] 之间的数字。\n";
            }
        } else {
            // 解析失败或有多余字符
            std::cout << "输入无效，请输入有效的数字。\n";
        }
    }
}

/**
 * @brief 全局随机数生成器实现
 *
 * 【单例模式】
 * 使用函数内静态变量实现：
 * - 首次调用时初始化（C++11 保证线程安全）
 * - 后续调用直接返回引用（无额外开销）
 *
 * 【随机种子来源】
 * - 优先使用 std::random_device（真随机数，来自操作系统）
 * - Windows：CryptGenRandom API
 * - Linux：/dev/urandom
 * - 若 random_device 不可用，编译器会使用确定性种子（极少见）
 *
 * 【mt19937 特性】
 * - Mersenne Twister 算法（周期 2^19937-1）
 * - 高质量伪随机数（通过多项统计测试）
 * - 状态大小 2.5KB（相比 rand() 的几字节）
 *
 * @complexity O(1) 首次初始化 O(K)，K 为 random_device 初始化开销
 */
std::mt19937& globalRng() {
    static std::random_device rd;
    static std::mt19937 rng(rd());
    return rng;
}
