cmake_minimum_required(VERSION 3.10)
project(DS_AI_Quiz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DS_AI_Quiz
        main.cpp
        Question.cpp
        Record.cpp
        Stats.cpp
        Recommender.cpp
        KnowledgeGraph.cpp
        App.cpp
        Utils.cpp
)
