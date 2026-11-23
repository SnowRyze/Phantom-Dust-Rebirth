#pragma once
// The format of P2P channel 4 (p2p.cpp): game packets with a number, sent straight to a player or through a third one,
// and the hellos that show who hears whom. Header only, nothing of Windows or EOS in it.
//
//   "PDDT", u8 type, then:
//   0 hello:  u8 version, u8 count, count * { 16-byte player id, u16 ms since the sender last heard that player }
//   1 direct: u32 epoch, u32 number, the game packet
//   2 detour: 16-byte receiver, 16-byte sender, u32 epoch, u32 number, the game packet
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace pdnet::detour
{
    constexpr uint8_t kHello = 0, kDirect = 1, kDetour = 2, kVersion = 1;
    constexpr size_t kDirectHeader = 13, kDetourHeader = 45;

    using Id = std::vector<uint8_t>; // 16 bytes, or empty

    inline Id ToId(const std::string& hex)
    {
        Id id;
        if (hex.size() != 32) return id;
        auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        for (size_t i = 0; i < 32; i += 2)
        {
            int hi = digit(hex[i]), lo = digit(hex[i + 1]);
            if (hi < 0 || lo < 0) return Id();
            id.push_back(static_cast<uint8_t>(hi << 4 | lo));
        }
        return id;
    }

    inline std::string ToHex(const uint8_t* id)
    {
        static const char digits[] = "0123456789abcdef";
        std::string hex;
        for (int i = 0; i < 16; ++i) { hex += digits[id[i] >> 4]; hex += digits[id[i] & 15]; }
        return hex;
    }

    inline void Put(std::vector<uint8_t>& v, uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; ++i) v.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
    inline uint32_t Get(const uint8_t* p, int bytes)
    {
        uint32_t value = 0;
        for (int i = 0; i < bytes; ++i) value |= static_cast<uint32_t>(p[i]) << (8 * i);
        return value;
    }

    inline std::vector<uint8_t> Start(uint8_t type) { return { 'P', 'D', 'D', 'T', type }; }

    inline std::vector<uint8_t> MakeHello(const std::vector<std::pair<Id, uint32_t>>& heard)
    {
        auto v = Start(kHello);
        v.push_back(kVersion);
        v.push_back(0);
        for (auto& [id, age] : heard)
        {
            if (id.size() != 16 || v[6] == 255) continue;
            ++v[6];
            v.insert(v.end(), id.begin(), id.end());
            Put(v, age > 65535 ? 65535 : age, 2);
        }
        return v;
    }

    inline std::vector<uint8_t> MakeDirect(uint32_t epoch, uint32_t number, const uint8_t* data, size_t size)
    {
        auto v = Start(kDirect);
        Put(v, epoch, 4);
        Put(v, number, 4);
        v.insert(v.end(), data, data + size);
        return v;
    }

    // `direct` is a MakeDirect packet.
    inline std::vector<uint8_t> MakeDetour(const Id& to, const Id& from, const std::vector<uint8_t>& direct)
    {
        auto v = Start(kDetour);
        v.insert(v.end(), to.begin(), to.end());
        v.insert(v.end(), from.begin(), from.end());
        v.insert(v.end(), direct.begin() + 5, direct.end());
        return v;
    }

    struct Packet
    {
        uint8_t type = 255;
        std::string to, from;                        // detour
        uint32_t epoch = 0, number = 0;              // direct, detour
        const uint8_t* data = nullptr;
        size_t size = 0;
        std::map<std::string, uint32_t> heard;       // hello
    };

    inline bool Read(const uint8_t* p, size_t size, Packet& out)
    {
        if (size < 5 || memcmp(p, "PDDT", 4) != 0) return false;
        out.type = p[4];
        if (out.type == kHello)
        {
            if (size < 7 || size < 7 + static_cast<size_t>(p[6]) * 18) return false;
            for (int i = 0; i < p[6]; ++i) out.heard[ToHex(p + 7 + i * 18)] = Get(p + 23 + i * 18, 2);
            return true;
        }
        size_t header = out.type == kDirect ? kDirectHeader : out.type == kDetour ? kDetourHeader : 0;
        if (!header || size <= header) return false;
        if (out.type == kDetour)
        {
            out.to = ToHex(p + 5);
            out.from = ToHex(p + 21);
        }
        out.epoch = Get(p + header - 8, 4);
        out.number = Get(p + header - 4, 4);
        out.data = p + header;
        out.size = size - header;
        return true;
    }

    // Which numbered packets have arrived: each is let through once, whichever way its copies come. An epoch is one
    // opening of the sender's link; two are kept (a new link's first packets can overtake the old one's last).
    class Seen
    {
    public:
        bool First(uint32_t epoch, uint32_t number)
        {
            Epoch* e = nullptr;
            for (auto& known : m_epochs) if (known.used && known.id == epoch) e = &known;
            if (!e)
            {
                e = &m_epochs[m_next];
                m_next ^= 1;
                e->used = true;
                e->id = epoch;
                e->top = number;
                e->bits.assign(kSize, false);
                e->bits[number % kSize] = true;
                return true;
            }
            if (number > e->top)
            {
                if (number - e->top >= kSize) e->bits.assign(kSize, false);
                else for (uint32_t n = e->top + 1; n < number; ++n) e->bits[n % kSize] = false;
                e->top = number;
                e->bits[number % kSize] = true;
                return true;
            }
            if (e->top - number >= kSize) return false; // far too old
            if (e->bits[number % kSize]) return false;
            e->bits[number % kSize] = true;
            return true;
        }

    private:
        static constexpr uint32_t kSize = 8192; // about 9 minutes of packets
        struct Epoch { bool used = false; uint32_t id = 0, top = 0; std::vector<bool> bits; };
        Epoch m_epochs[2];
        int m_next = 0;
    };
}
