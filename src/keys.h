#pragma once
#include "common.h"

std::wstring VkName(UINT vk);
bool VkFromName(const std::wstring& name, UINT& vk);
bool ParseCombo(const std::wstring& s, Combo& out);
std::wstring ComboToString(const Combo& c);
bool IsModifierVk(UINT vk);
bool IsMouseVk(UINT vk);
