// Scenarios of spec mod-pack-storage: model, naming rules, operations, applying.
#include "doctest/doctest.h"
#include "packs/pack_store.h"

using namespace modpacks;

TEST_CASE("pack model: creating from enabled mods drops base") {
    PackStore store;
    REQUIRE(store.Create("Krastorio", {"base", "space-age", "flib", "Krastorio2"}) == NameError::None);
    const Pack* p = store.Find("Krastorio");
    REQUIRE(p);
    CHECK(p->mods == std::vector<std::string>{"Krastorio2", "flib", "space-age"});
    CHECK(store.selected() == std::optional<std::string>("Krastorio"));
}

TEST_CASE("pack model: duplicates and empty names in mod lists are normalized") {
    CHECK(NormalizeMods({"b", "a", "b", "", "base"}) == std::vector<std::string>{"a", "b"});
}

TEST_CASE("first run: no packs, nothing selected") {
    PackStore store;
    CHECK(store.packs().empty());
    CHECK_FALSE(store.selected().has_value());
}

TEST_CASE("naming rules: empty name") {
    PackStore store;
    CHECK(store.Create("", {}) == NameError::Empty);
    CHECK(store.Create("   \t ", {}) == NameError::Empty);
    CHECK(store.Create("\xC2\xA0", {}) == NameError::Empty);  // no-break space only
    CHECK(store.packs().empty());
}

TEST_CASE("naming rules: names are trimmed") {
    PackStore store;
    REQUIRE(store.Create("  Space Age  ", {}) == NameError::None);
    CHECK(store.packs()[0].name == "Space Age");
}

TEST_CASE("naming rules: duplicates ignore case, including Cyrillic") {
    PackStore store;
    REQUIRE(store.Create("Krastorio", {}) == NameError::None);
    CHECK(store.Create("krastorio", {}) == NameError::Duplicate);
    REQUIRE(store.Create("Пакет", {}) == NameError::None);
    CHECK(store.Create("ПАКЕТ", {}) == NameError::Duplicate);
    REQUIRE(store.Create("Other", {}) == NameError::None);
    CHECK(store.Rename("Other", "KRASTORIO") == NameError::Duplicate);
    CHECK(store.packs().size() == 3);
}

TEST_CASE("packs are ordered by name ignoring case") {
    PackStore store;
    store.Create("space Age", {});
    store.Create("Krastorio", {});
    store.Create("bob's", {});
    REQUIRE(store.packs().size() == 3);
    CHECK(store.packs()[0].name == "bob's");
    CHECK(store.packs()[1].name == "Krastorio");
    CHECK(store.packs()[2].name == "space Age");
}

TEST_CASE("rename keeps contents and selection") {
    PackStore store;
    store.Create("Old", {"flib"});
    REQUIRE(store.Rename("Old", "New") == NameError::None);
    CHECK(store.Find("Old") == nullptr);
    REQUIRE(store.Find("New"));
    CHECK(store.Find("New")->mods == std::vector<std::string>{"flib"});
    CHECK(store.selected() == std::optional<std::string>("New"));
}

TEST_CASE("rename to the same name with another case is allowed") {
    PackStore store;
    store.Create("pack", {});
    CHECK(store.Rename("pack", "Pack") == NameError::None);
    CHECK(store.packs()[0].name == "Pack");
}

TEST_CASE("rename rejects an empty name") {
    PackStore store;
    store.Create("pack", {});
    CHECK(store.Rename("pack", "  ") == NameError::Empty);
    CHECK(store.packs()[0].name == "pack");
}

TEST_CASE("delete removes the pack and clears the selection") {
    PackStore store;
    store.Create("A", {});
    store.Create("B", {});
    REQUIRE(store.selected() == std::optional<std::string>("B"));
    CHECK(store.Delete("b"));
    CHECK(store.Find("B") == nullptr);
    CHECK_FALSE(store.selected().has_value());
    CHECK_FALSE(store.Delete("missing"));
}

TEST_CASE("select: existing, unknown and none") {
    PackStore store;
    store.Create("A", {});
    store.Create("B", {});
    store.Select("a");
    CHECK(store.selected() == std::optional<std::string>("A"));
    store.Select("nope");
    CHECK_FALSE(store.selected().has_value());
    store.Select("B");
    store.Select(std::nullopt);
    CHECK_FALSE(store.selected().has_value());
}

TEST_CASE("add and remove mods") {
    PackStore store;
    store.Create("P", {"b"});
    CHECK(store.AddMod("P", "a"));
    CHECK_FALSE(store.AddMod("P", "a"));
    CHECK_FALSE(store.AddMod("P", "base"));
    CHECK(store.Find("P")->mods == std::vector<std::string>{"a", "b"});
    CHECK(store.RemoveMod("P", "b"));
    CHECK_FALSE(store.RemoveMod("P", "b"));
    CHECK(store.Find("P")->mods == std::vector<std::string>{"a"});
}

TEST_CASE("revision changes on every modification") {
    PackStore store;
    unsigned r0 = store.revision();
    store.Create("P", {});
    unsigned r1 = store.revision();
    CHECK(r1 != r0);
    store.Select("P");  // already selected: no change
    CHECK(store.revision() == r1);
    store.AddMod("P", "x");
    CHECK(store.revision() != r1);
}

TEST_CASE("apply: pack mods on, everything else off, base untouched") {
    Pack pack{"P", {"Krastorio2", "flib"}};
    ApplyPlan plan = PlanApply(pack, {"base", "flib", "Krastorio2", "alien-biomes", "space-age"});
    std::vector<std::pair<std::string, bool>> expected = {
        {"flib", true}, {"Krastorio2", true}, {"alien-biomes", false}, {"space-age", false}};
    CHECK(plan.enabled == expected);
    CHECK(plan.missing.empty());
}

TEST_CASE("apply: mod of the pack is not installed") {
    Pack pack{"P", {"cybersyn", "flib"}};
    ApplyPlan plan = PlanApply(pack, {"base", "flib"});
    CHECK(plan.enabled == std::vector<std::pair<std::string, bool>>{{"flib", true}});
    CHECK(plan.missing == std::vector<std::string>{"cybersyn"});
}

TEST_CASE("assign drops invalid and duplicate packs and unknown selection") {
    PackStore store;
    store.Assign({{"A", {"x", "base"}}, {" ", {}}, {"a", {}}, {"B", {}}}, std::string("missing"));
    REQUIRE(store.packs().size() == 2);
    CHECK(store.Find("A")->mods == std::vector<std::string>{"x"});
    CHECK_FALSE(store.selected().has_value());
    store.Assign({{"A", {}}}, std::string("a"));
    CHECK(store.selected() == std::optional<std::string>("A"));
}

// Scenarios of spec mod-pack-manage-tab: the draft of checkbox changes and the
// pack selected when the window opens.

TEST_CASE("draft: only changed checkboxes, base ignored") {
    ModStates baseline{{"base", true}, {"a", true}, {"b", false}, {"c", true}};
    ModStates current{{"base", false}, {"a", false}, {"b", true}, {"c", true}};
    ModDelta delta = DraftDelta(baseline, current);
    CHECK(delta.added == std::vector<std::string>{"b"});
    CHECK(delta.removed == std::vector<std::string>{"a"});
}

TEST_CASE("draft: switched off and back on is no change") {
    ModStates states{{"a", true}, {"b", false}};
    CHECK(DraftDelta(states, states).empty());
}

TEST_CASE("draft: mods missing from one snapshot are ignored") {
    CHECK(DraftDelta({{"a", false}}, {{"a", false}, {"new", true}}).empty());
    CHECK(DraftDelta({{"gone", true}}, {}).empty());
}

TEST_CASE("draft: applying keeps untouched and not installed mods") {
    PackStore store;
    store.Create("QOL", {"flare-stack", "foo-not-installed", "flib"});
    unsigned before = store.revision();
    ModDelta delta{{"krastorio2"}, {"flare-stack"}};
    CHECK(DeltaChangesPack(*store.Find("QOL"), delta));
    CHECK(store.ApplyDelta("QOL", delta));
    CHECK(store.Find("QOL")->mods == std::vector<std::string>{"flib", "foo-not-installed", "krastorio2"});
    CHECK(store.revision() != before);
}

TEST_CASE("draft: a delta the pack already has changes nothing") {
    PackStore store;
    store.Create("QOL", {"flib"});
    ModDelta delta{{"flib"}, {"absent"}};
    CHECK_FALSE(DeltaChangesPack(*store.Find("QOL"), delta));
    unsigned before = store.revision();
    CHECK_FALSE(store.ApplyDelta("QOL", delta));
    CHECK(store.revision() == before);
    CHECK_FALSE(store.ApplyDelta("unknown", {{"x"}, {}}));
}

TEST_CASE("open: the pack matching the enabled mods is selected") {
    std::vector<Pack> packs{{"A", {"a"}}, {"B", {"a", "b"}}};
    ModStates current{{"base", true}, {"a", true}, {"b", true}, {"c", false}};
    CHECK(MatchPack(packs, current, std::nullopt) == std::optional<std::string>("B"));
    CHECK(MatchPack(packs, {{"a", false}, {"b", true}}, std::nullopt) == std::nullopt);
}

TEST_CASE("open: mods of a pack that are not installed do not prevent a match") {
    std::vector<Pack> packs{{"QOL", {"a", "missing"}}};
    CHECK(MatchPack(packs, {{"a", true}, {"b", false}}, std::nullopt) == std::optional<std::string>("QOL"));
}

TEST_CASE("open: several matches prefer the saved selection, else the first") {
    std::vector<Pack> packs{{"A", {"a"}}, {"B", {"a"}}, {"C", {"c"}}};
    ModStates current{{"a", true}, {"c", false}};
    CHECK(MatchPack(packs, current, std::string_view("b")) == std::optional<std::string>("B"));
    CHECK(MatchPack(packs, current, std::string_view("C")) == std::optional<std::string>("A"));
    CHECK(MatchPack(packs, current, std::nullopt) == std::optional<std::string>("A"));
}

TEST_CASE("open: an empty pack matches when nothing but base is enabled") {
    std::vector<Pack> packs{{"Vanilla", {}}};
    CHECK(MatchPack(packs, {{"base", true}, {"a", false}}, std::nullopt) == std::optional<std::string>("Vanilla"));
}
