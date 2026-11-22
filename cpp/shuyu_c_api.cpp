/**
 * 枢语 C ABI 桥接实现 (shuyu_c_api.cpp)
 * © 阿权/路飞
 * 使用 std::shared_mutex 保护 ShuyuRuntime 并发读写安全。
 */

#include "shuyu_c_api.h"
#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"
#include "shuyu_ops.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string_view>

struct ShuyuRuntime {
    mutable std::shared_mutex mtx;
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

int shuyu_runtime_save(const ShuyuRuntime* rt, const char* filepath) {
    if (!rt || !filepath) return -1;
    std::shared_lock<std::shared_mutex> lock(rt->mtx);
    return shuyu::saveSoulState(rt->soul, filepath);
}

int shuyu_runtime_load(ShuyuRuntime* rt, const char* filepath) {
    if (!rt || !filepath) return -1;
    std::unique_lock<std::shared_mutex> lock(rt->mtx);
    return shuyu::loadSoulState(rt->soul, filepath);
}

char* shuyu_runtime_state_json(const ShuyuRuntime* rt) {
    if (!rt) return dupString("{\"error\":\"invalid_args\"}");
    std::shared_lock<std::shared_mutex> lock(rt->mtx);
    return dupString(shuyu::soulStateToJson(rt->soul));
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
    return shuyu::LexiconEngine::instance().encode(std::string_view(word));
}

int64_t shuyu_encode_han(const char* han) {
    if (!han) return -1;
    return shuyu::LexiconEngine::instance().encodeHan(std::string_view(han));
}

char* shuyu_auto_coin_json(const char* seed) {
    std::string s = seed ? seed : "";
    auto entry = shuyu::LexiconEngine::instance().autoCoin(s);
    return dupString(wordToJson(entry));
}

char* shuyu_invoke_json(const char* operation, const char* input) {
    std::string op = operation ? operation : "";
    std::string in = input ? input : "";
    return dupString(shuyu::invokeOperationJson(op, in));
}

char* shuyu_interpret_json(ShuyuRuntime* rt, const char* nexus_code) {
    if (!rt || !nexus_code) return dupString("{\"error\":\"invalid_args\"}");
    std::unique_lock<std::shared_mutex> lock(rt->mtx);
    auto res = shuyu::interpret(nexus_code, rt->soul);
    shuyu::applyToSoul(res, rt->soul);
    return dupString(shuyu::resultToJson(res, rt->soul));
}

void shuyu_sovereign_pulse(ShuyuRuntime* rt, const char* signal) {
    if (!rt) return;
    std::unique_lock<std::shared_mutex> lock(rt->mtx);
    shuyu::sovereignPulse(rt->soul, signal ? signal : "sovereign_pulse");
}

void shuyu_free_string(char* ptr) {
    std::free(ptr);
}

} // extern "C"
