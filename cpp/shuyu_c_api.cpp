/**
 * 枢语 C ABI 桥接实现 (shuyu_c_api.cpp)
 * © 阿权/路飞
 */

#include "shuyu_c_api.h"
#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"

#include <cstdlib>
#include <cstring>
#include <sstream>

struct ShuyuRuntime {
    shuyu::SoulState soul;
};

namespace {

char* dupString(const std::string& s) {
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (!buf) return nullptr;
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    return buf;
}

std::string escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else oss << static_cast<char>(c);
    }
    return oss.str();
}

std::string wordToJson(const shuyu::WordEntry& w) {
    std::ostringstream oss;
    oss << "{"
        << "\"id\":" << w.id << ","
        << "\"word\":\"" << escapeJson(w.word) << "\","
        << "\"han\":\"" << escapeJson(w.han) << "\","
        << "\"layer\":\"" << escapeJson(w.layer) << "\","
        << "\"sem\":\"" << escapeJson(w.sem) << "\""
        << "}";
    return oss.str();
}

} // namespace

extern "C" {

ShuyuRuntime* shuyu_runtime_create(void) {
    return new ShuyuRuntime();
}

void shuyu_runtime_destroy(ShuyuRuntime* rt) {
    delete rt;
}

uint64_t shuyu_capacity(void) {
    return shuyu::LexiconEngine::CAPACITY;
}

char* shuyu_decode_json(uint64_t id) {
    auto entry = shuyu::LexiconEngine::instance().decode(id);
    if (!entry) return dupString("null");
    return dupString(wordToJson(*entry));
}

int64_t shuyu_encode(const char* word) {
    if (!word) return -1;
    return shuyu::LexiconEngine::instance().encode(word);
}

char* shuyu_auto_coin_json(const char* seed) {
    std::string s = seed ? seed : "";
    auto entry = shuyu::LexiconEngine::instance().autoCoin(s);
    return dupString(wordToJson(entry));
}

char* shuyu_interpret_json(ShuyuRuntime* rt, const char* nexus_code) {
    if (!rt || !nexus_code) return dupString("{\"error\":\"invalid_args\"}");
    auto res = shuyu::interpret(nexus_code, rt->soul);
    shuyu::applyToSoul(res, rt->soul);
    return dupString(shuyu::resultToJson(res, rt->soul));
}

void shuyu_sovereign_pulse(ShuyuRuntime* rt, const char* signal) {
    if (!rt) return;
    shuyu::sovereignPulse(rt->soul, signal ? signal : "sovereign_pulse");
}

void shuyu_free_string(char* ptr) {
    std::free(ptr);
}

} // extern "C"
