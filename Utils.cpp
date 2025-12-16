#include "Utils.h"
#include <iostream>
#include <limits>
#include <cstdlib>

#ifdef _WIN32
#define NOMINMAX  // 防止 Windows.h 定义 min/max 宏
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    // 使用 ANSI 转义序列清屏
    std::cout << "\033[2J\033[H";
#endif
}

void pauseForUser(const std::string& message) {
    std::cout << "\n" << message;

    // 清空输入缓冲区中的残留字符（包括换行符）
    // 使用括号避免 Windows.h 的 max 宏污染
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    // 等待用户按回车
    std::string dummy;
    std::getline(std::cin, dummy);
}

// 获取可执行文件所在目录
std::filesystem::path getExeDir() {
#ifdef _WIN32
    // Windows 平台：使用 GetModuleFileNameW 获取可执行文件路径
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH);

    if (length > 0 && length < MAX_PATH) {
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
        buffer[length] = '\0';
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    } else {
        // 获取失败，fallback 到当前工作目录
        std::cerr << "警告：无法获取可执行文件路径，使用当前工作目录作为 fallback。\n";
        return std::filesystem::current_path();
    }
#endif
}

// 获取数据目录（exeDir/data）
std::filesystem::path getDataDir() {
    return getExeDir() / "data";
}

// 获取报告目录（exeDir/reports）
std::filesystem::path getReportsDir() {
    return getExeDir() / "reports";
}
