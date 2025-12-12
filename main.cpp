#include "Question.h"
#include "Record.h"
#include "Stats.h"
#include "KnowledgeGraph.h"
#include "App.h"

#ifdef _WIN32
#include <windows.h>
#include <iostream>
#endif

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif

    // 从文件加载题库
    if (!loadQuestionsFromFile("data/questions.csv")) {
        return 0;
    }

    // 用户登录：输入学号或用户名
    std::cout << "=============================\n";
    std::cout << " 数据结构智能刷题系统\n";
    std::cout << "=============================\n";
    std::cout << "请输入学号或用户名：";

    std::string userId;
    while (true) {
        std::getline(std::cin, userId);
        // 去除前后空格
        userId.erase(0, userId.find_first_not_of(" \t\n\r"));
        userId.erase(userId.find_last_not_of(" \t\n\r") + 1);

        if (!userId.empty()) {
            break;
        }
        std::cout << "用户标识不能为空，请重新输入：";
    }

    loginUser(userId);
    std::cout << "\n";

    // 从文件加载当前用户的历史做题记录
    loadRecordsFromFile(getRecordFilePath());

    // 加载知识点依赖图
    loadKnowledgeGraphFromFile("data/knowledge_graph.txt");

    // 运行主菜单循环
    runMenuLoop();

    return 0;
}
