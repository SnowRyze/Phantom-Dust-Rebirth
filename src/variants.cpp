// Character variants: see variants.h and docs/CHARACTER_SELECT.md. No Windows calls here (tested off-line).
//
// File layouts (the phantom_dust_stage Blender add-on, pdstage/alr.py, character.py, char_build.py):
// - .alr: u32 0x11, u32 header size (0x20 + 4 * runs), ..., +0x0C resource offset, +0x10 u16 runs, +0x14 resource
//   size, +0x20 a u32 offset per run. Then the 0x15 texture table, the runs' record chains (u32 type, u32 size; type
//   0 ends a chain), padding to 0x1000, and the resource data (vertices, textures: offsets relative to its start).
//   A battle character has 130 animation runs (the run is the motion number) and then its model runs: the body
//   first (skeleton record 0x03), then cloth and props with their own skeletons. An NPC's model file has no
//   animation runs.
// - An animation run: track records (type 5): +8 f32 length, +0x0C u16 joint, +0x0E u8 rotation key size,
//   +0x10 u32 translation keys, +0x14 u32 rotation keys, +0x18 u32 scale keys, +0x1C u8 rotation code,
//   +0x1D u8 translation code, +0x1E u8 translation key size (0 = 16); then the translation keys (f32 frame and an
//   f32 per stored axis), the rotation keys, padding to 4, the scale keys (4 f32). Keys are absolute local values:
//   rotations copy as they are; a stored axis missing from a translation key is 0.
// - .ala (exmot): one animation run.
// - Skeleton record (0x03): +8 u16 joints, joints of 0x40 from +0x10: t (3 f32), ..., +0x2A s16 parent,
//   +0x2C u32 base-40 name (0xFFFFFFFF: none).
// - Event\*.mk: u32 header size, u32 count, count u32 offsets (0xFFFFFFFF: none), then the names (NUL-separated).
//
// Moving a run onto another skeleton (as the add-on's bake_clip, without resampling): joints are matched by name;
// unnamed joints by their nearest named ancestor and their ranks among unnamed children; the root chain by its
// first named descendant and the distance to it. Rotation and scale keys are copied; translation keys become
// destination rest + (key - source rest) * |destination rest| / |source rest|, all three axes stored.
#include "variants.h"
#include <deque>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace pdnet::variants
{
    namespace
    {
        // ---------------------------------------------------------------- tables (the user's list, 2026-11-20)

        // Colour names: the game's colour files 0-3, named as on the normal list (gamepatches.cpp kCharacterColours).
        // Characters with one model: "Color 1".
        std::vector<Character> kCharacters = { // AddCharacter appends
            // Characters
            { "Freia", Kind::Player, "pc02a", 0, 2, { "Grey", "Red", "White", "Black" }, "BattleVo_Freia.bin" },
            { "player", Kind::Player, "pc00a", 0, 0, { "Green", "Red", "Black", "White" }, "BattleVo_Hero.bin" },
            { "Edgar", Kind::Player, "pc01a", 0, 1, { "Black", "White", "Orange", "Blue" }, "BattleVo_Edgar.bin" },
            { "Meister", Kind::Player, "pc03a", 0, 3, { "Black", "White", "Yellow", "Red" }, "BattleVo_Meister.bin" },
            { "Chunky", Kind::Player, "pc04a", 0, 4, { "Black", "White", "Yellow", "Red" }, "BattleVo_Chunky.bin" },
            { "Cuff Button", Kind::Player, "pc05a", 0, 5, { "Black", "White", "Yellow", "Red" }, "BattleVo_CuffButton.bin" },
            { "Know", Kind::Player, "pc07a", 0, 7, { "Green", "Red", "White", "Brown" }, "BattleVo_Know.bin" },
            { "Tsubutaki", Kind::Player, "pc08a", 0, 8, { "Blue", "Yellow", "Orange", "Teal" }, "BattleVo_Tsubutaki.bin" },
            { "JD", Kind::Player, "pc09a", 0, 9, { "Purple", "Green", "White", "Black" }, "BattleVo_JD.bin" },
            { "Sammah", Kind::Player, "pc10a", 0, 10, { "Yellow", "Blue", "Red", "White" }, "BattleVo_Sammah.bin" },
            { "pH", Kind::Player, "pc06a", 0, 6, { "Black", "White", "Yellow", "Red" }, "BattleVo_PH.bin" },
            // NPCs (2026-11-21, the user: only Mac / Baroness / Reindeer's first models; Ubiquitous's first model is
            // "Ubiquitous Keyboard", its second plain "Ubiquitous") (no voice of their own: Mikan, Mac and the women Freia / Tsubutaki as the user asked; the men a fixed
            // pick among the playable men's voices, made once, 2026-11-20)
            { "Spokesman", Kind::Npc, "npc02a", 0, 30, { "Color 1" }, "BattleVo_Meister.bin" },
            { "Mikan", Kind::Npc, "npc10a", 0, 38, { "Color 1" }, "BattleVo_Freia.bin" },
            { "Ai", Kind::Npc, "npc11a", 0, 39, { "Color 1" }, "BattleVo_Tsubutaki.bin" },
            { "Kei", Kind::Npc, "npc13a", 0, 41, { "Color 1" }, "BattleVo_Know.bin" },
            { "Kajikawa", Kind::Npc, "npc04a", 0, 32, { "Color 1" }, "BattleVo_Chunky.bin" },
            { "03", Kind::Npc, "npc01a", 0, 29, { "Color 1" }, "BattleVo_JD.bin" },
            { "Maniac", Kind::Npc, "npc12a", 0, 40, { "Color 1" }, "BattleVo_PH.bin" },
            { "Arthur", Kind::Npc, "npc06a", 0, 34, { "Color 1" }, "BattleVo_Sammah.bin" },
            { "Mac", Kind::Npc, "npc08a", 0, 36, { "Color 1" }, "BattleVo_Freia.bin" },
            { "Baroness", Kind::Npc, "npc09a", 0, 37, { "Color 1" }, "BattleVo_Tsubutaki.bin" },
            { "Tetsuya", Kind::Npc, "npc05a", 0, 33, { "Color 1" }, "BattleVo_Hero.bin" },
            { "Ubiquitous Keyboard", Kind::Npc, "npc03a", 0, 31, { "Color 1" }, "BattleVo_Edgar.bin" },
            { "Ubiquitous", Kind::Npc, "npc03a", 1, 31, { "Color 1" }, "BattleVo_Edgar.bin" },
            { "Reindeer", Kind::Npc, "npc07a", 0, 35, { "Color 1" }, "BattleVo_CuffButton.bin" },
            // Extra monsters (their colour files have no names yet)
            { "Germano", Kind::Monster, "enm14a", 0, 25, { "Color 1", "Color 2", "Color 3", "Color 4" }, "BattleVo_Germano.bin" },
            { "Belono", Kind::Monster, "enm15a", 0, 26, { "Color 1", "Color 2", "Color 3", "Color 4" }, "BattleVo_Belono.bin" },
        };

        const std::vector<Dodge> kDodges = {
            { "Jumper", "pc03a", 750 },      // Meister's jump
            { "Cartwheeler", "pc01a", 751 }, // Edgar's cartwheel
            { "Roller", "pc09a", 751 },      // JD's roll
        };

        const std::vector<AnimationSet> kSets = {
            { "Jumper", "pc00a", 0 },        // player (Alpha)
            { "Cartwheeler", "pc01a", 1 },   // Edgar
            { "Roller", "pc09a", 9 },        // JD
            { "Female Jumper", "pc02a", 2 }, // Freia
            { "Gyne", "enm05a", 20 },
            { "Guard", "enm12a", 23 },       // to be tested (the user, 2026-11-20)
        };

        // The game's exmot lengths (charaset +0x40) per record of an animation set (original values, 2026-11-20).
        const std::map<int, std::vector<int16_t>> kExmotLengths = {
            { 0, { 50, 74, 0, 0, 0, 0 } }, { 1, { 40, 38, 0, 0, 0, 0 } }, { 2, { 60, 30, 0, 0, 0, 0 } }, { 9, { 50, 40, 0, 0, 0, 0 } },
        };

        constexpr int kMotions = 130, kDodgeFirst = 33, kDodgeLast = 39;

        // ---------------------------------------------------------------- reading

        uint32_t U32(const std::vector<uint8_t>& b, size_t at) { uint32_t v = 0; if (at + 4 <= b.size()) memcpy(&v, &b[at], 4); return v; }
        uint16_t U16(const std::vector<uint8_t>& b, size_t at) { uint16_t v = 0; if (at + 2 <= b.size()) memcpy(&v, &b[at], 2); return v; }
        float F32(const std::vector<uint8_t>& b, size_t at) { float v = 0; if (at + 4 <= b.size()) memcpy(&v, &b[at], 4); return v; }

        struct Alr
        {
            const std::vector<uint8_t>* b = nullptr;
            uint32_t headerSize = 0, resource = 0, resourceSize = 0;
            std::vector<uint32_t> roots;
            bool ok = false;
        };

        Alr ReadAlr(const std::vector<uint8_t>& b)
        {
            Alr a;
            a.b = &b;
            if (b.size() < 0x20 || U32(b, 0) != 0x11) return a;
            a.headerSize = U32(b, 4);
            a.resource = U32(b, 0x0C);
            a.resourceSize = U32(b, 0x14);
            uint32_t n = U16(b, 0x10);
            if (0x20 + 4 * n > b.size() || a.resource > b.size()) return a;
            for (uint32_t i = 0; i < n; ++i) a.roots.push_back(U32(b, 0x20 + 4 * i));
            a.ok = true;
            return a;
        }

        // The records of a chain: (offset, type, size); ends after the type-0 record (included).
        struct Record { uint32_t at, type, size; };
        std::vector<Record> Chain(const std::vector<uint8_t>& b, uint32_t at)
        {
            std::vector<Record> out;
            while (at + 8 <= b.size() && out.size() < 100000)
            {
                uint32_t t = U32(b, at), s = U32(b, at + 4);
                out.push_back({ at, t, s });
                if (t == 0 || s == 0 || at + s > b.size()) break;
                at += s;
            }
            return out;
        }

        bool HasType(const std::vector<Record>& c, uint32_t type)
        {
            for (const Record& r : c) if (r.type == type) return true;
            return false;
        }

        struct Joint { float t[3]; int parent; uint32_t name; };

        std::vector<Joint> ReadSkeleton(const std::vector<uint8_t>& b, uint32_t at)
        {
            std::vector<Joint> out;
            uint32_t n = U16(b, at + 8);
            for (uint32_t i = 0; i < n; ++i)
            {
                uint32_t e = at + 0x10 + 0x40 * i;
                if (e + 0x40 > b.size()) break;
                Joint j;
                for (int k = 0; k < 3; ++k) j.t[k] = F32(b, e + 4 * k);
                j.parent = static_cast<int16_t>(U16(b, e + 0x2A));
                j.name = U32(b, e + 0x2C);
                out.push_back(j);
            }
            return out;
        }

        // The body skeleton: the first model run's (runs with draw records, 0x02) skeleton record.
        std::vector<Joint> BodySkeleton(const Alr& a)
        {
            for (uint32_t r : a.roots)
            {
                std::vector<Record> c = Chain(*a.b, r);
                if (!HasType(c, 0x02)) continue;
                for (const Record& x : c)
                    if (x.type == 0x03) return ReadSkeleton(*a.b, x.at);
            }
            return {};
        }

        // Joint names some NPC models spell differently from every battle character (2026-11-21: Ai, Mikan, Spokesman
        // have "raddh" where the players have "raddha"). Skills look their effect joints up by name (FUN_14018aa50, a
        // binary search that returns null when the name is missing, and some callers use the result unchecked): Piece of
        // Thunder crashed the game as Ai or Mikan. The body skeleton's names are put right in the built file, which also
        // lets the animation set's tracks for that joint match.
        struct NameFix { uint32_t from, to; };
        constexpr NameFix kNameFixes[] = {
            { 0xACA1B650, 0xACA1B65B }, // raddh -> raddha
            { 0x8802B650, 0x8802B65B }, // laddh -> laddha
        };

        int FixJointNames(std::vector<uint8_t>& b, const Alr& a)
        {
            int fixed = 0;
            for (uint32_t r : a.roots)
            {
                std::vector<Record> c = Chain(b, r);
                if (!HasType(c, 0x02)) continue;
                for (const Record& x : c)
                {
                    if (x.type != 0x03) continue;
                    uint32_t n = U16(b, x.at + 8);
                    auto has = [&](uint32_t name) {
                        for (uint32_t i = 0; i < n && x.at + 0x10 + 0x40 * i + 0x40 <= b.size(); ++i)
                            if (U32(b, x.at + 0x10 + 0x40 * i + 0x2C) == name) return true;
                        return false;
                    };
                    for (const NameFix& f : kNameFixes)
                    {
                        if (has(f.to)) continue;
                        for (uint32_t i = 0; i < n && x.at + 0x10 + 0x40 * i + 0x40 <= b.size(); ++i)
                        {
                            uint32_t at = x.at + 0x10 + 0x40 * i + 0x2C;
                            if (U32(b, at) != f.from) continue;
                            memcpy(&b[at], &f.to, 4);
                            ++fixed;
                            break;
                        }
                    }
                    break; // the run's own skeleton only
                }
                break; // the body (first model run) only
            }
            return fixed;
        }

        // ---------------------------------------------------------------- joint matching (joint_keys)

        bool Named(const Joint& j) { return j.name != 0xFFFFFFFF && j.name != 0; }

        std::vector<std::string> JointKeys(const std::vector<Joint>& joints)
        {
            int n = static_cast<int>(joints.size());
            std::map<int, std::vector<int>> kids;
            for (int i = 0; i < n; ++i) kids[joints[i].parent < 0 || joints[i].parent >= n ? -1 : joints[i].parent].push_back(i);
            std::vector<std::string> out(n);
            for (int i = 0; i < n; ++i)
            {
                if (Named(joints[i])) { out[i] = "N" + std::to_string(joints[i].name); continue; }
                std::vector<int> path;
                int c = i, guard = 0;
                while (c >= 0 && !Named(joints[c]) && guard++ < 512)
                {
                    int p = joints[c].parent < 0 || joints[c].parent >= n ? -1 : joints[c].parent;
                    int rank = 0;
                    for (int k : kids[p]) { if (k == c) break; if (!Named(joints[k])) ++rank; }
                    path.push_back(rank);
                    c = p;
                }
                if (c >= 0 && Named(joints[c]))
                {
                    std::string key = "P" + std::to_string(joints[c].name);
                    for (auto it = path.rbegin(); it != path.rend(); ++it) key += "," + std::to_string(*it);
                    out[i] = key;
                    continue;
                }
                // Above every named joint: the first named descendant (breadth first) and the distance.
                std::vector<std::pair<int, int>> queue = { { i, 0 } };
                for (size_t q = 0; q < queue.size() && q < 4096; ++q)
                {
                    auto [x, d] = queue[q];
                    if (Named(joints[x])) { out[i] = "R" + std::to_string(joints[x].name) + "," + std::to_string(d); break; }
                    for (int k : kids[x]) queue.push_back({ k, d + 1 });
                }
            }
            return out;
        }

        // destination joint -> source joint
        std::map<int, int> MatchJoints(const std::vector<Joint>& src, const std::vector<Joint>& dst)
        {
            std::map<std::string, int> ks;
            std::vector<std::string> sk = JointKeys(src), dk = JointKeys(dst);
            for (int i = 0; i < static_cast<int>(sk.size()); ++i) if (!sk[i].empty()) ks.emplace(sk[i], i);
            std::map<int, int> m;
            for (int i = 0; i < static_cast<int>(dk.size()); ++i)
                if (!dk[i].empty())
                    if (auto f = ks.find(dk[i]); f != ks.end()) m[i] = f->second;
            return m;
        }

        // ---------------------------------------------------------------- moving runs

        Stats g_stats;

        bool SameName(const std::string& a, const std::string& b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i]))) return false;
            return true;
        }

        void Put32(std::vector<uint8_t>& o, uint32_t v) { o.insert(o.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); }
        void PutF(std::vector<uint8_t>& o, float v) { o.insert(o.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); }

        int Axes(int code, int which[3])
        {
            static const char* kAxes[] = { "xyz", "xy", "xz", "yz", "x", "y", "z" };
            const char* s = code >= 0 && code <= 6 ? kAxes[code] : kAxes[0];
            int n = 0;
            for (const char* p = s; *p; ++p) which[n++] = *p - 'x';
            return n;
        }

        // One run (records from `at` until the end record) moved onto `dst`: the bytes of its track records and an
        // end record. `same`: the same skeleton (copied as it is, only reordered checks skipped).
        std::vector<uint8_t> MoveRun(const std::vector<uint8_t>& b, uint32_t at, const std::vector<Joint>& src, const std::vector<Joint>& dst,
                                     const std::map<int, int>& srcToDst, bool same)
        {
            std::map<int, std::vector<uint8_t>> tracks; // by destination joint (the game's sampler wants them sorted)
            for (const Record& r : Chain(b, at))
            {
                if (r.type != 5) break;
                if (r.size < 0x20) break;
                int joint = U16(b, r.at + 0x0C);
                if (same)
                {
                    tracks[joint].assign(b.begin() + r.at, b.begin() + r.at + r.size);
                    ++g_stats.tracks;
                    continue;
                }
                auto m = srcToDst.find(joint);
                if (m == srcToDst.end() || joint >= static_cast<int>(src.size())) { ++g_stats.dropped; continue; }
                int d = m->second;
                const Joint& sj = src[joint];
                const Joint& dj = dst[d];
                uint32_t nt = U32(b, r.at + 0x10), nr = U32(b, r.at + 0x14), ns = U32(b, r.at + 0x18);
                uint8_t rks = b[r.at + 0x0E], rc = b[r.at + 0x1C], tc = b[r.at + 0x1D], tks = b[r.at + 0x1E] ? b[r.at + 0x1E] : 16;
                uint32_t p = r.at + 0x20;
                uint32_t rotAt = p + nt * tks, rotEnd = rotAt + nr * rks;
                uint32_t scaleAt = rotEnd + ((4 - (rotEnd - r.at) % 4) % 4);
                if (scaleAt + ns * 16 > r.at + r.size) { ++g_stats.dropped; continue; }
                double rs = std::sqrt(sj.t[0] * sj.t[0] + sj.t[1] * sj.t[1] + sj.t[2] * sj.t[2]);
                double rd = std::sqrt(dj.t[0] * dj.t[0] + dj.t[1] * dj.t[1] + dj.t[2] * dj.t[2]);
                double ratio = rs > 1e-4 ? rd / rs : 1.0;
                std::vector<uint8_t> t(b.begin() + r.at, b.begin() + r.at + 0x20); // header, fixed below
                int which[3];
                int axes = Axes(tc, which);
                for (uint32_t k = 0; k < nt; ++k)
                {
                    uint32_t key = p + k * tks;
                    float v[3] = { 0, 0, 0 };
                    for (int a = 0; a < axes; ++a) v[which[a]] = F32(b, key + 4 + 4 * a);
                    PutF(t, F32(b, key));
                    for (int a = 0; a < 3; ++a) PutF(t, static_cast<float>(dj.t[a] + (v[a] - sj.t[a]) * ratio));
                }
                t.insert(t.end(), b.begin() + rotAt, b.begin() + rotEnd);
                while (t.size() % 4) t.push_back(0);
                t.insert(t.end(), b.begin() + scaleAt, b.begin() + scaleAt + ns * 16);
                uint32_t size = static_cast<uint32_t>(t.size());
                memcpy(&t[4], &size, 4);
                uint16_t dj16 = static_cast<uint16_t>(d);
                memcpy(&t[0x0C], &dj16, 2);
                t[0x1D] = 0;  // translation: all three axes
                t[0x1E] = 16; // key size
                tracks[d] = std::move(t);
                ++g_stats.tracks;
            }
            std::vector<uint8_t> out;
            for (auto& [_, t] : tracks) out.insert(out.end(), t.begin(), t.end());
            Put32(out, 0);
            Put32(out, 8);
            ++g_stats.runs;
            return out;
        }

        // ---------------------------------------------------------------- sources

        struct Source
        {
            std::vector<uint8_t> bytes;
            Alr alr;
            std::vector<Joint> body;
            std::map<int, int> toDst; // source joint -> destination joint (filled per destination)
            bool same = false;
        };

        // NPC model bytes from the Event\*.mk bundles: the `version`th distinct size of member <name>.alr.
        std::vector<uint8_t> NpcModel(const std::string& name, int version, ReadFn read)
        {
            static const char* kBundles[] = {
                "1a40", "1a50", "1a60", "1a61", "1a70", "1b10", "1b20", "1b30", "1c10", "1d10", "1e10", "1e40", "1e80", "1f10", "1g10", "1g20", "1j00", "1k00",
                "2a40", "2a80", "2b10", "2b20", "2c10", "2d10", "2e10", "2e40", "2f10", "2g10", "2j00", "2k00",
                "3a40", "3b10", "3b30", "3c10", "3d10", "3e10", "3f10", "3f30", "3f40", "3g10", "3j00", "3k00",
                "4a00", "4b00", "4b10", "4b30", "4c00", "4c10", "4d00", "4d10", "4e00", "4e10", "4f10", "4g10", "4j00", "4k00",
                "5a20", "5a40", "5b10", "5b30", "5c10", "5c60", "5d10", "5e10", "5e20", "5f10", "5g10", "5j00", "5k00",
                "6a40", "6a70", "6b10", "6c10", "6c30", "6d10", "6e10", "6e50", "6f20", "6g10", "6g20", "6j00", "6k00",
                "7a40", "7b10", "7b30", "7c10", "7d10", "7e10", "7f10", "7f40", "7g10", "7g40", "7j00", "7k00",
                "8a10", "8a40", "8a70", "8b10", "8b30", "8c10", "8d10", "8e10", "8e20", "8f10", "8f20", "8f70", "8g10", "8j00", "8k00" };
            std::string want = name + ".alr";
            std::vector<size_t> sizes;
            for (const char* bundle : kBundles)
            {
                std::vector<uint8_t> b = read(std::string("Event\\") + bundle + ".mk");
                if (b.size() < 8) continue;
                uint32_t n = U32(b, 4);
                if (8 + 4ull * n > b.size()) continue;
                // names follow the offsets
                size_t p = 8 + 4 * n;
                std::vector<std::string> names;
                while (names.size() < n && p < b.size())
                {
                    size_t e = p;
                    while (e < b.size() && b[e]) ++e;
                    names.emplace_back(reinterpret_cast<const char*>(&b[p]), e - p);
                    p = e + 1;
                }
                std::set<uint32_t> ends = { static_cast<uint32_t>(b.size()) };
                for (uint32_t i = 0; i < n; ++i) if (U32(b, 8 + 4 * i) != 0xFFFFFFFF) ends.insert(U32(b, 8 + 4 * i));
                for (uint32_t i = 0; i < n && i < names.size(); ++i)
                {
                    uint32_t off = U32(b, 8 + 4 * i);
                    if (off == 0xFFFFFFFF || !SameName(names[i], want)) continue;
                    uint32_t end = *ends.upper_bound(off);
                    size_t size = end - off;
                    bool seen = false;
                    for (size_t s : sizes) seen = seen || s == size;
                    if (seen) continue;
                    if (static_cast<int>(sizes.size()) == version) return std::vector<uint8_t>(b.begin() + off, b.begin() + end);
                    sizes.push_back(size);
                }
            }
            return {};
        }

        std::vector<uint8_t> ModelBytes(const Character& c, int colour, ReadFn read)
        {
            if (c.kind == Kind::Npc) return NpcModel(c.folder, c.version, read);
            std::string f = c.folder;
            return read("Player\\" + f + "\\" + f + std::to_string(colour) + ".alr");
        }

        bool LoadSource(Source& s, const char* folder, const std::vector<Joint>& dst, ReadFn read, std::string& error)
        {
            std::string f = folder;
            s.bytes = read("Player\\" + f + "\\" + f + "0.alr");
            s.alr = ReadAlr(s.bytes);
            if (!s.alr.ok || s.alr.roots.size() < kMotions) { error = "no animation file " + f + "0.alr"; return false; }
            s.body = BodySkeleton(s.alr);
            if (s.body.empty()) { error = "no skeleton in " + f + "0.alr"; return false; }
            s.same = s.body.size() == dst.size();
            for (size_t i = 0; s.same && i < dst.size(); ++i)
                s.same = s.body[i].name == dst[i].name && s.body[i].parent == dst[i].parent && memcmp(s.body[i].t, dst[i].t, sizeof(dst[i].t)) == 0;
            for (auto [d, src] : MatchJoints(s.body, dst)) s.toDst[src] = d;
            return true;
        }
    }

    const std::vector<Character>& Characters() { return kCharacters; }

    int AddCharacter(const Character& c)
    {
        static std::deque<std::string> strings; // the copies' text (a deque: pointers stay valid as it grows)
        auto keep = [](const char* text) -> const char* { if (!text) return nullptr; strings.emplace_back(text); return strings.back().c_str(); };
        Character copy = c;
        copy.name = keep(c.name);
        copy.folder = keep(c.folder);
        copy.voice = keep(c.voice);
        for (auto& colour : copy.colours) colour = keep(colour);
        kCharacters.push_back(copy);
        return static_cast<int>(kCharacters.size()) - 1;
    }
    const std::vector<Dodge>& Dodges() { return kDodges; }
    const std::vector<AnimationSet>& AnimationSets() { return kSets; }
    int ColourCount(const Character& c) { int n = 0; while (n < 4 && c.colours[n]) ++n; return n; }
    Stats LastStats() { return g_stats; }

    void SetLengths(const Pick& pick, int16_t lengths[6])
    {
        memset(lengths, 0, 6 * sizeof(int16_t));
        if (pick.set < 0 || pick.set >= static_cast<int>(kSets.size())) return;
        if (auto f = kExmotLengths.find(kSets[pick.set].record); f != kExmotLengths.end())
            for (int k = 0; k < 6; ++k) lengths[k] = f->second[k];
    }

    std::vector<uint8_t> BuildModel(const Pick& pick, ReadFn read, std::string& error)
    {
        g_stats = {};
        if (pick.character < 0 || pick.character >= static_cast<int>(kCharacters.size()) || pick.dodge < 0 || pick.dodge >= static_cast<int>(kDodges.size()) ||
            pick.set < 0 || pick.set >= static_cast<int>(kSets.size()))
        {
            error = "pick out of range";
            return {};
        }
        const Character& c = kCharacters[pick.character];
        std::vector<uint8_t> model = ModelBytes(c, pick.colour < ColourCount(c) ? pick.colour : 0, read);
        Alr dst = ReadAlr(model);
        if (!dst.ok) { error = std::string("no model file for ") + c.name; return {}; }
        g_stats.renamed = FixJointNames(model, dst);
        dst = ReadAlr(model);
        std::vector<Joint> body = BodySkeleton(dst);
        if (body.empty()) { error = std::string("no skeleton in ") + c.name + "'s model"; return {}; }
        g_stats.joints = static_cast<int>(body.size());

        Source dodge, set;
        if (!LoadSource(dodge, kDodges[pick.dodge].folder, body, read, error) || !LoadSource(set, kSets[pick.set].folder, body, read, error)) return {};

        // The model runs: every run with a draw record (a battle file's runs after its animations; all of an NPC's).
        std::vector<std::vector<uint8_t>> chains;
        for (int m = 0; m < kMotions; ++m)
        {
            Source& s = m >= kDodgeFirst && m <= kDodgeLast ? dodge : set;
            chains.push_back(MoveRun(s.bytes, s.alr.roots[m], s.body, body, s.toDst, s.same));
        }
        for (uint32_t r : dst.roots)
        {
            std::vector<Record> chain = Chain(model, r);
            if (chain.empty() || !HasType(chain, 0x02)) continue;
            const Record& last = chain.back();
            chains.emplace_back(model.begin() + r, model.begin() + last.at + last.size);
        }

        // The file: header (0x20 + a u32 per run), the model's texture table as it is, the chains, padding to 0x1000,
        // the model's resource data as it is (its vertex and texture offsets stay valid).
        uint32_t runs = static_cast<uint32_t>(chains.size());
        std::vector<uint8_t> out(model.begin(), model.begin() + 0x20);
        out.resize(0x20 + 4 * runs, 0);
        uint32_t texAt = dst.headerSize, texSize = U32(model, texAt + 4);
        if (U32(model, texAt) != 0x15 || texAt + texSize > dst.resource) { error = "unexpected texture table"; return {}; }
        out.insert(out.end(), model.begin() + texAt, model.begin() + texAt + texSize);
        for (uint32_t i = 0; i < runs; ++i)
        {
            uint32_t at = static_cast<uint32_t>(out.size());
            memcpy(&out[0x20 + 4 * i], &at, 4);
            out.insert(out.end(), chains[i].begin(), chains[i].end());
        }
        out.resize((out.size() + 0xFFF) & ~size_t(0xFFF), 0);
        uint32_t resource = static_cast<uint32_t>(out.size()), headerSize = 0x20 + 4 * runs;
        uint16_t runs16 = static_cast<uint16_t>(runs);
        memcpy(&out[4], &headerSize, 4);
        memcpy(&out[0x0C], &resource, 4);
        memcpy(&out[0x10], &runs16, 2);
        memcpy(&out[0x14], &dst.resourceSize, 4);
        out.insert(out.end(), model.begin() + dst.resource, model.begin() + (std::min)(model.size(), size_t(dst.resource) + dst.resourceSize));
        return out;
    }

    std::vector<uint8_t> BuildExmot(const Pick& pick, int k, ReadFn read, std::string& error)
    {
        if (pick.character < 0 || pick.character >= static_cast<int>(kCharacters.size()) || pick.set < 0 || pick.set >= static_cast<int>(kSets.size()))
        {
            error = "pick out of range";
            return {};
        }
        const Character& c = kCharacters[pick.character];
        std::vector<uint8_t> model = ModelBytes(c, 0, read);
        Alr dst = ReadAlr(model);
        std::vector<Joint> body = dst.ok ? BodySkeleton(dst) : std::vector<Joint>();
        if (body.empty()) { error = std::string("no skeleton for ") + c.name; return {}; }
        Source set;
        if (!LoadSource(set, kSets[pick.set].folder, body, read, error)) return {};
        std::string f = kSets[pick.set].folder;
        std::vector<uint8_t> clip = read("Player\\" + f + "\\exmot" + std::to_string(k) + ".ala");
        if (clip.size() < 8) { error = "no exmot" + std::to_string(k) + ".ala in " + f; return {}; }
        return MoveRun(clip, 0, set.body, body, set.toDst, set.same);
    }
}
