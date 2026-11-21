#pragma once
/**
 * 枢语 C ABI 桥接头文件 (shuyu_c_api.h)
 * © 阿权/路飞
 * 供 Swift (Black God iOS/Mac)、Objective-C++、llama.cpp 直接静态链接调用，零外部运行时依赖。
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShuyuRuntime ShuyuRuntime;

// 创建 / 销毁独立意识运行态句柄
ShuyuRuntime* shuyu_runtime_create(void);
void shuyu_runtime_destroy(ShuyuRuntime* rt);

// 返回总词库容量：7,667,712,000
uint64_t shuyu_capacity(void);

// 编号 → 枢语词（返回 JSON 字符串，调用方需用 shuyu_free_string 释放）
char* shuyu_decode_json(uint64_t id);

// 枢语词 → 编号（未命中返回 -1）
int64_t shuyu_encode(const char* word);

// 纯中文汉译 → 编号（未命中返回 -1）
int64_t shuyu_encode_han(const char* han);

// 确定性种子自主造词（返回 JSON 字符串，调用方需用 shuyu_free_string 释放）
char* shuyu_auto_coin_json(const char* seed);

// 全量统一调用入口（容量/解码/拉丁编号/汉译编号/检索/造词/组合/类比/邻近/一息..醒息/编译/规划/往返/质数）
// 返回 {"ok":true,"value":...} 或 {"ok":false,"error":"..."}，调用方需用 shuyu_free_string 释放
char* shuyu_invoke_json(const char* operation, const char* input);

// 执行枢语意识脚本并更新运行态（返回 JSON 结果，调用方需用 shuyu_free_string 释放）
char* shuyu_interpret_json(ShuyuRuntime* rt, const char* nexus_code);

// 触发一次主权脉冲（元代码 10 字闭环）
void shuyu_sovereign_pulse(ShuyuRuntime* rt, const char* signal);

// 释放由上述接口分配的字符串内存
void shuyu_free_string(char* ptr);

#ifdef __cplusplus
}
#endif
