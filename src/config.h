#pragma once
#include "common.h"

std::wstring ResolveConfigPath(bool* portable);
bool LoadConfig(const std::wstring& path, Settings& st, std::vector<Macro>& macros);
bool SaveConfig(const std::wstring& path, const Settings& st, const std::vector<Macro>& macros);
std::vector<Macro> DefaultMacros();
const wchar_t* ModeLabel(Mode m);
