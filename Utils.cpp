#include "Utils.h"
#include <iostream>
#include <limits>
#include <cstdlib>

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
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // 等待用户按回车
    std::string dummy;
    std::getline(std::cin, dummy);
}
