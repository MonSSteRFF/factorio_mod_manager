#include "packs/pack_store.h"

#include <windows.h>

#include <algorithm>
#include <set>

#include "common/utf.h"

namespace modpacks {

namespace {

bool IsSpace(wchar_t c) {
    // ASCII whitespace plus the Unicode spaces a user can realistically type or paste.
    return c == L' ' || (c >= L'\t' && c <= L'\r') || c == 0x00A0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200B) || c == 0x2028 || c == 0x2029 || c == 0x202F ||
           c == 0x205F || c == 0x3000 || c == 0xFEFF;
}

int CompareIgnoreCase(std::string_view a, std::string_view b) {
    std::wstring wa = Utf8ToWide(a);
    std::wstring wb = Utf8ToWide(b);
    int r = CompareStringOrdinal(wa.data(), static_cast<int>(wa.size()), wb.data(),
                                 static_cast<int>(wb.size()), TRUE);
    return r - CSTR_EQUAL;
}

}  // namespace

std::string TrimName(std::string_view name) {
    std::wstring w = Utf8ToWide(name);
    size_t begin = 0;
    size_t end = w.size();
    while (begin < end && IsSpace(w[begin])) ++begin;
    while (end > begin && IsSpace(w[end - 1])) --end;
    return WideToUtf8(std::wstring_view(w).substr(begin, end - begin));
}

bool NamesEqual(std::string_view a, std::string_view b) { return CompareIgnoreCase(a, b) == 0; }

bool NameLess(std::string_view a, std::string_view b) {
    int r = CompareIgnoreCase(a, b);
    return r != 0 ? r < 0 : a < b;
}

std::vector<std::string> NormalizeMods(std::vector<std::string> mods) {
    std::erase_if(mods, [](const std::string& m) { return m.empty() || m == kBaseMod; });
    std::sort(mods.begin(), mods.end());
    mods.erase(std::unique(mods.begin(), mods.end()), mods.end());
    return mods;
}

ApplyPlan PlanApply(const Pack& pack, const std::vector<std::string>& installed) {
    ApplyPlan plan;
    std::set<std::string, std::less<>> inPack(pack.mods.begin(), pack.mods.end());
    std::set<std::string, std::less<>> seen;
    for (const std::string& mod : installed) {
        if (mod == kBaseMod || !seen.insert(mod).second) continue;
        plan.enabled.emplace_back(mod, inPack.contains(mod));
    }
    for (const std::string& mod : pack.mods) {
        if (!seen.contains(mod)) plan.missing.push_back(mod);
    }
    return plan;
}

const Pack* PackStore::Find(std::string_view name) const {
    for (const Pack& p : packs_) {
        if (NamesEqual(p.name, name)) return &p;
    }
    return nullptr;
}

Pack* PackStore::FindMutable(std::string_view name) {
    return const_cast<Pack*>(static_cast<const PackStore*>(this)->Find(name));
}

NameError PackStore::ValidateName(std::string_view name, std::string_view renaming) const {
    if (TrimName(name).empty()) return NameError::Empty;
    const Pack* existing = Find(TrimName(name));
    if (existing && !(renaming.size() && NamesEqual(existing->name, renaming))) return NameError::Duplicate;
    return NameError::None;
}

NameError PackStore::Create(std::string_view name, std::vector<std::string> enabledMods) {
    std::string trimmed = TrimName(name);
    if (NameError e = ValidateName(trimmed); e != NameError::None) return e;
    packs_.push_back(Pack{trimmed, NormalizeMods(std::move(enabledMods))});
    selected_ = trimmed;
    Sort();
    Touch();
    return NameError::None;
}

NameError PackStore::Rename(std::string_view from, std::string_view to) {
    Pack* pack = FindMutable(from);
    if (!pack) return NameError::None;
    std::string trimmed = TrimName(to);
    if (NameError e = ValidateName(trimmed, pack->name); e != NameError::None) return e;
    bool wasSelected = selected_ && NamesEqual(*selected_, pack->name);
    pack->name = trimmed;
    if (wasSelected) selected_ = trimmed;
    Sort();
    Touch();
    return NameError::None;
}

bool PackStore::Delete(std::string_view name) {
    auto it = std::find_if(packs_.begin(), packs_.end(),
                           [&](const Pack& p) { return NamesEqual(p.name, name); });
    if (it == packs_.end()) return false;
    if (selected_ && NamesEqual(*selected_, it->name)) selected_.reset();
    packs_.erase(it);
    Touch();
    return true;
}

void PackStore::Select(std::optional<std::string_view> name) {
    std::optional<std::string> next;
    if (name) {
        if (const Pack* p = Find(*name)) next = p->name;
    }
    if (next != selected_) {
        selected_ = std::move(next);
        Touch();
    }
}

bool PackStore::AddMod(std::string_view pack, std::string_view mod) {
    Pack* p = FindMutable(pack);
    if (!p || mod.empty() || mod == kBaseMod) return false;
    auto it = std::lower_bound(p->mods.begin(), p->mods.end(), mod);
    if (it != p->mods.end() && *it == mod) return false;
    p->mods.insert(it, std::string(mod));
    Touch();
    return true;
}

bool PackStore::RemoveMod(std::string_view pack, std::string_view mod) {
    Pack* p = FindMutable(pack);
    if (!p) return false;
    auto it = std::lower_bound(p->mods.begin(), p->mods.end(), mod);
    if (it == p->mods.end() || *it != mod) return false;
    p->mods.erase(it);
    Touch();
    return true;
}

void PackStore::Assign(std::vector<Pack> packs, std::optional<std::string> selected) {
    packs_.clear();
    for (Pack& p : packs) {
        std::string name = TrimName(p.name);
        if (name.empty() || Find(name)) continue;
        packs_.push_back(Pack{std::move(name), NormalizeMods(std::move(p.mods))});
    }
    Sort();
    selected_.reset();
    if (selected) {
        if (const Pack* p = Find(*selected)) selected_ = p->name;
    }
    Touch();
}

void PackStore::Sort() {
    std::sort(packs_.begin(), packs_.end(),
              [](const Pack& a, const Pack& b) { return NameLess(a.name, b.name); });
}

}  // namespace modpacks
