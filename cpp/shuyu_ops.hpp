#pragma once
/**
 * 枢语全量扩展操作引擎 — 纯 C++17 原生实现 (shuyu_ops.hpp)
 * © 阿权/路飞
 * 100% 替代原 JavaScriptCore / shuyu.js 运行时，零脚本引擎依赖。
 * 支持全部 27 个操作：容量、解码、拉丁编号、汉译编号、检索、造词、组合、类比、邻近、
 * 一息/余息/回息/摇息/落息/起息/栖息/转息/顾息/倾息/贴息/含息/温息/醒息、编译、规划、往返、质数。
 */

#include <string>

namespace shuyu {

// 统一调度接口，返回标准 JSON：{"ok":true,"value":...} 或 {"ok":false,"error":"..."}
std::string invokeOperationJson(const std::string& operation, const std::string& input);

} // namespace shuyu
