#pragma once

#include <string>

// 清屏函数
void clearScreen();

// 暂停等待用户按回车键继续
void pauseForUser(const std::string& message = "按回车键继续...");
