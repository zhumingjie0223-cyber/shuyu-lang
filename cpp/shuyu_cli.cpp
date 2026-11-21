/**
 * 枢语纯 C++17 原生命令行解释器 (shuyu-cpp)
 * © 阿权/路飞
 * 用法：
 *   shuyu-cpp decode <编号>
 *   shuyu-cpp encode <枢语词>
 *   shuyu-cpp coin <种子>
 *   shuyu-cpp interpret <代码文本或 - 从标准输入读取>
 *   shuyu-cpp info
 */

#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "枢语纯 C++17 解释器 v1.0 | 词库容量: "
                  << shuyu::LexiconEngine::CAPACITY << "\n"
                  << "用法:\n"
                  << "  shuyu-cpp decode <编号>\n"
                  << "  shuyu-cpp encode <枢语词>\n"
                  << "  shuyu-cpp coin <种子>\n"
                  << "  shuyu-cpp interpret <枢语代码或 ->\n"
                  << "  shuyu-cpp info\n";
        return 0;
    }

    const std::string cmd = argv[1];
    const auto& eng = shuyu::LexiconEngine::instance();

    if (cmd == "info") {
        std::cout << "{\"engine\":\"shuyu-cpp17\",\"capacity\":" << shuyu::LexiconEngine::CAPACITY << "}\n";
        return 0;
    }

    if (cmd == "decode" && argc >= 3) {
        uint64_t id = std::stoull(argv[2]);
        auto w = eng.decode(id);
        if (!w) {
            std::cerr << "编号越界: " << id << "\n";
            return 1;
        }
        std::cout << "{\"id\":" << w->id
                  << ",\"word\":\"" << w->word
                  << "\",\"han\":\"" << w->han
                  << "\",\"layer\":\"" << w->layer
                  << "\",\"sem\":\"" << w->sem << "\"}\n";
        return 0;
    }

    if (cmd == "encode" && argc >= 3) {
        int64_t id = eng.encode(argv[2]);
        std::cout << id << "\n";
        return id < 0 ? 1 : 0;
    }

    if (cmd == "coin" && argc >= 3) {
        auto w = eng.autoCoin(argv[2]);
        std::cout << "{\"id\":" << w.id
                  << ",\"word\":\"" << w.word
                  << "\",\"han\":\"" << w.han
                  << "\",\"layer\":\"" << w.layer << "\"}\n";
        return 0;
    }

    if (cmd == "interpret") {
        std::string code;
        if (argc >= 3 && std::string(argv[2]) != "-") {
            code = argv[2];
        } else {
            std::ostringstream ss;
            ss << std::cin.rdbuf();
            code = ss.str();
        }
        shuyu::SoulState soul;
        auto res = shuyu::interpret(code, soul);
        shuyu::applyToSoul(res, soul);
        std::cout << shuyu::resultToJson(res, soul) << "\n";
        return 0;
    }

    std::cerr << "未知命令: " << cmd << "\n";
    return 1;
}
