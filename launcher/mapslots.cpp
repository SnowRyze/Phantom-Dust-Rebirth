// New map slots (Conceptual): see mapslots.h. Formats (docs\CONCEPTUAL.md "New map slots"):
//   stageset_<lang>.dat: u32 file size, u32 count, u32 text offset, u32 0, count x 0x14 (u32 name, short name, internal
//     name offsets into the text; s16 map number, s16 layouts, s16 multiplayer flag, s16 0), then the text.
//   .alr: u32 0x11, u32 header size (0x20 + 4 x roots), u32 ?, u32 resource offset, u16 root count, u16 ?, u32 resource
//     size, ..., u32 root offsets at 0x20; then the 0x15 texture table (u32 0x15, u32 size, u32 count, count x 0x1C:
//     u32 flags, u32 offset into the resources, u32 0, u8 flags, u8 format, u8 width log2 << 4 | mips, u8 height log2,
//     u32 0, two base-40 name words); the root chunk chains; zeros up to the resource offset; the resources.
//   MNet's second root chunk (type 0x10): u32 textures, u32 sprites, 16-byte set name, textures x 0x20 (16-byte .dds
//     name + 16 bytes), textures x 0x14 (u16 width, u16 height, u32 ?, u32 sprites on it, u32 4 = DXT1, u32 0),
//     sprites x 60 (u32 texture, 32-byte name, 4 floats uv rectangle, u32 width, u32 height).
#include "mapslots.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace mapslots
{
    namespace
    {
        LogFn g_log = nullptr;
        constexpr int kStripW = 254, kStripH = 112, kStripPitch = 114, kColumnW = 256, kSheetH = 1024, kPerSheet = 9;

        uint32_t U32(const std::string& b, size_t o) { uint32_t v = 0; if (o + 4 <= b.size()) memcpy(&v, b.data() + o, 4); return v; }
        uint16_t U16(const std::string& b, size_t o) { uint16_t v = 0; if (o + 2 <= b.size()) memcpy(&v, b.data() + o, 2); return v; }
        void Add32(std::string& b, uint32_t v) { b.append(reinterpret_cast<const char*>(&v), 4); }
        void Add16(std::string& b, uint16_t v) { b.append(reinterpret_cast<const char*>(&v), 2); }

        std::string ReadAll(const std::wstring& p)
        {
            std::string data;
            if (FILE* f = nullptr; !_wfopen_s(&f, p.c_str(), L"rb") && f)
            {
                char buf[65536];
                for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) data.append(buf, n);
                fclose(f);
            }
            return data;
        }

        void MakeDirs(const std::wstring& dir)
        {
            for (size_t i = 3; i <= dir.size(); ++i)
                if (i == dir.size() || dir[i] == L'\\') CreateDirectoryW(dir.substr(0, i).c_str(), nullptr);
        }

        // Writes `data` to `path` unless it already holds exactly that. True if the file is right afterwards.
        bool WriteIfChanged(const std::wstring& path, const std::string& data)
        {
            WIN32_FILE_ATTRIBUTE_DATA a;
            if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a) &&
                ((static_cast<uint64_t>(a.nFileSizeHigh) << 32) | a.nFileSizeLow) == data.size() && ReadAll(path) == data)
                return true;
            MakeDirs(path.substr(0, path.find_last_of(L'\\')));
            std::wstring tmp = path + L".tmp";
            FILE* f = nullptr;
            if (_wfopen_s(&f, tmp.c_str(), L"wb") || !f) return false;
            bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
            fclose(f);
            return ok && MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        // ---- stageset ----

        struct Stage { std::string name, shortName, internal; int16_t map = 0, layouts = 0, flag = 0, pad = 0; };

        std::string CString(const std::string& b, size_t o)
        {
            if (o >= b.size()) return {};
            size_t e = b.find('\0', o);
            return b.substr(o, (e == std::string::npos ? b.size() : e) - o);
        }

        bool ParseStageset(const std::string& f, std::vector<Stage>& out)
        {
            if (f.size() < 0x10) return false;
            uint32_t count = U32(f, 4), pool = U32(f, 8);
            if (count > 200 || 0x10 + count * 0x14 > f.size() || pool > f.size()) return false;
            for (uint32_t i = 0; i < count; ++i)
            {
                size_t e = 0x10 + i * 0x14;
                Stage s;
                s.name = CString(f, pool + U32(f, e));
                s.shortName = CString(f, pool + U32(f, e + 4));
                s.internal = CString(f, pool + U32(f, e + 8));
                s.map = static_cast<int16_t>(U16(f, e + 12));
                s.layouts = static_cast<int16_t>(U16(f, e + 14));
                s.flag = static_cast<int16_t>(U16(f, e + 16));
                s.pad = static_cast<int16_t>(U16(f, e + 18));
                out.push_back(s);
            }
            return true;
        }

        std::string BuildStageset(const std::vector<Stage>& stages)
        {
            std::string head, pool;
            auto text = [&](const std::string& s) { uint32_t at = static_cast<uint32_t>(pool.size()); pool += s; pool += '\0'; return at; };
            std::string entries;
            for (const auto& s : stages)
            {
                Add32(entries, text(s.name));
                Add32(entries, text(s.shortName));
                Add32(entries, text(s.internal));
                Add16(entries, static_cast<uint16_t>(s.map));
                Add16(entries, static_cast<uint16_t>(s.layouts));
                Add16(entries, static_cast<uint16_t>(s.flag));
                Add16(entries, static_cast<uint16_t>(s.pad));
            }
            uint32_t poolAt = 0x10 + static_cast<uint32_t>(entries.size());
            Add32(head, poolAt + static_cast<uint32_t>(pool.size()));
            Add32(head, static_cast<uint32_t>(stages.size()));
            Add32(head, poolAt);
            Add32(head, 0);
            return head + entries + pool;
        }

        // ---- images ----

        // Any image WIC reads, scaled to w x h, as BGRA rows (top first). False if it can't be read.
        bool DecodeImage(const std::string& bytes, UINT w, UINT h, std::vector<uint8_t>& bgra)
        {
            if (bytes.empty()) return false;
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IWICImagingFactory* factory = nullptr;
            IWICStream* stream = nullptr;
            IWICBitmapDecoder* decoder = nullptr;
            IWICBitmapFrameDecode* frame = nullptr;
            IWICFormatConverter* converter = nullptr;
            IWICBitmapScaler* scaler = nullptr;
            bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
                      SUCCEEDED(factory->CreateStream(&stream)) &&
                      SUCCEEDED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())), static_cast<DWORD>(bytes.size()))) &&
                      SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
                      SUCCEEDED(decoder->GetFrame(0, &frame)) &&
                      SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
                      SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
                      SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
                      SUCCEEDED(scaler->Initialize(converter, w, h, WICBitmapInterpolationModeFant));
            if (ok)
            {
                bgra.assign(static_cast<size_t>(w) * h * 4, 0);
                ok = SUCCEEDED(scaler->CopyPixels(nullptr, w * 4, static_cast<UINT>(bgra.size()), bgra.data()));
            }
            for (IUnknown* u : { static_cast<IUnknown*>(scaler), static_cast<IUnknown*>(converter), static_cast<IUnknown*>(frame),
                                 static_cast<IUnknown*>(decoder), static_cast<IUnknown*>(stream), static_cast<IUnknown*>(factory) })
                if (u) u->Release();
            return ok;
        }

        // A plain stand-in preview: dark grey with a lighter frame.
        std::vector<uint8_t> PlainPreview()
        {
            std::vector<uint8_t> p(static_cast<size_t>(kStripW) * kStripH * 4);
            for (int y = 0; y < kStripH; ++y)
                for (int x = 0; x < kStripW; ++x)
                {
                    bool edge = x < 2 || y < 2 || x >= kStripW - 2 || y >= kStripH - 2;
                    uint8_t v = edge ? 0x90 : 0x30;
                    uint8_t* px = &p[(static_cast<size_t>(y) * kStripW + x) * 4];
                    px[0] = px[1] = px[2] = v;
                    px[3] = 0xff;
                }
            return p;
        }

        uint16_t To565(const uint8_t* bgra) { return static_cast<uint16_t>(((bgra[2] >> 3) << 11) | ((bgra[1] >> 2) << 5) | (bgra[0] >> 3)); }
        void From565(uint16_t c, int rgb[3]) { rgb[0] = ((c >> 11) & 31) * 255 / 31; rgb[1] = ((c >> 5) & 63) * 255 / 63; rgb[2] = (c & 31) * 255 / 31; }

        // DXT1 (BC1), four-colour blocks from each block's colour range: good enough for menu previews.
        std::string EncodeDxt1(const std::vector<uint8_t>& bgra, int w, int h)
        {
            std::string out;
            out.reserve(static_cast<size_t>(w / 4) * (h / 4) * 8);
            for (int by = 0; by < h / 4; ++by)
                for (int bx = 0; bx < w / 4; ++bx)
                {
                    uint8_t px[16][4];
                    int lo[3] = { 255, 255, 255 }, hi[3] = { 0, 0, 0 };
                    for (int i = 0; i < 16; ++i)
                    {
                        memcpy(px[i], &bgra[(static_cast<size_t>(by * 4 + i / 4) * w + bx * 4 + i % 4) * 4], 4);
                        for (int c = 0; c < 3; ++c) { lo[c] = (std::min)(lo[c], static_cast<int>(px[i][c])); hi[c] = (std::max)(hi[c], static_cast<int>(px[i][c])); }
                    }
                    uint8_t hiPx[4] = { static_cast<uint8_t>(hi[0]), static_cast<uint8_t>(hi[1]), static_cast<uint8_t>(hi[2]), 255 };
                    uint8_t loPx[4] = { static_cast<uint8_t>(lo[0]), static_cast<uint8_t>(lo[1]), static_cast<uint8_t>(lo[2]), 255 };
                    uint16_t c0 = To565(hiPx), c1 = To565(loPx);
                    uint32_t indices = 0;
                    if (c0 < c1) std::swap(c0, c1);
                    if (c0 != c1)
                    {
                        int pal[4][3];
                        From565(c0, pal[0]);
                        From565(c1, pal[1]);
                        for (int c = 0; c < 3; ++c) { pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3; pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3; }
                        for (int i = 0; i < 16; ++i)
                        {
                            int rgb[3] = { px[i][2], px[i][1], px[i][0] }, best = 0, bestD = INT_MAX;
                            for (int k = 0; k < 4; ++k)
                            {
                                int d = 0;
                                for (int c = 0; c < 3; ++c) d += (rgb[c] - pal[k][c]) * (rgb[c] - pal[k][c]);
                                if (d < bestD) { bestD = d; best = k; }
                            }
                            indices |= static_cast<uint32_t>(best) << (2 * i);
                        }
                    }
                    Add16(out, c0);
                    Add16(out, c1);
                    Add32(out, indices);
                }
            return out;
        }

        // ---- the preview sheet (pdnet puts it into its MNet.alr copy) ----

        // map-previews.bin: "PDMP", u32 1, u16 sheet width, u16 sheet height, u32 strips, u32 data size, then the sheet as
        // DXT1. Strip k (254 x 112) is at x = (k / 9) * 256, y = (k % 9) * 114; the sheet is 256, 512 or 1024 wide (9, 18
        // or 36 strips) and 1024 high.
        std::string PreviewSheet(const std::vector<std::vector<uint8_t>>& strips)
        {
            int columns = static_cast<int>((strips.size() + kPerSheet - 1) / kPerSheet);
            int sheetW = columns <= 1 ? 256 : columns <= 2 ? 512 : 1024;
            std::vector<uint8_t> sheet(static_cast<size_t>(sheetW) * kSheetH * 4, 0);
            for (size_t k = 0; k < strips.size() && k < static_cast<size_t>(sheetW / kColumnW * kPerSheet); ++k)
            {
                int x = static_cast<int>(k / kPerSheet) * kColumnW, y = static_cast<int>(k % kPerSheet) * kStripPitch;
                for (int row = 0; row < kStripH; ++row)
                    memcpy(&sheet[(static_cast<size_t>(y + row) * sheetW + x) * 4], &strips[k][static_cast<size_t>(row) * kStripW * 4], kStripW * 4);
            }
            std::string dxt = EncodeDxt1(sheet, sheetW, kSheetH), out = "PDMP";
            Add32(out, 1);
            Add16(out, static_cast<uint16_t>(sheetW));
            Add16(out, static_cast<uint16_t>(kSheetH));
            Add32(out, static_cast<uint32_t>(strips.size()));
            Add32(out, static_cast<uint32_t>(dxt.size()));
            return out + dxt;
        }
    }

    const std::vector<int>& SlotNumbers()
    {
        static std::vector<int> numbers = []
        {
            std::vector<int> n;
            for (int i = 30; i <= 64; ++i)
                if (i % 10 != 1 && i % 10 != 9 && i != 32) n.push_back(i); // st32 is the game's own (in its map file table)
            return n;
        }();
        return numbers;
    }

    void Generate(const std::wstring& gameDir, const std::wstring& pdplDir, const std::wstring& pdnetDir, const std::vector<MapMod>& mapsIn, LogFn log)
    {
        g_log = log;
        std::vector<MapMod> maps(mapsIn.begin(), mapsIn.begin() + (std::min)(mapsIn.size(), SlotNumbers().size()));
        if (mapsIn.size() > maps.size()) log(L"maps: %zu map mods, only %zu slots - the rest are left out", mapsIn.size(), maps.size());
        std::wstring data = pdplDir + L"\\Assets\\Data", listFile = pdnetDir + L"\\map-generated.txt", slotsFile = pdnetDir + L"\\map-slots.txt", previewFile = pdnetDir + L"\\map-previews.bin";
        std::set<std::wstring> written; // paths relative to pdplDir

        auto write = [&](const std::wstring& rel, const std::string& bytes)
        {
            if (WriteIfChanged(pdplDir + L"\\" + rel, bytes)) written.insert(rel);
            else log(L"maps: couldn't write %s", rel.c_str());
        };

        std::string slots = "# map slots (pdlaunch, mapslots.cpp): slot number = stage entry = map number; preview (strip in map-previews.bin); mod id; name\n";
        if (!maps.empty())
        {
            // Stage list: the game's, then entries up to the last slot (blank fillers where there's no map).
            int last = SlotNumbers()[maps.size() - 1];
            for (const wchar_t* file : { L"stageset_en.dat", L"stageset.dat" }) // English, Japanese (the game's file names)
            {
                std::wstring rel = std::wstring(L"Assets\\Data\\setup\\stageset\\") + file;
                std::vector<Stage> stages;
                if (!ParseStageset(ReadAll(gameDir + L"\\" + rel), stages)) { log(L"maps: the game's %s couldn't be read", rel.c_str()); continue; }
                stages.resize((std::min<size_t>)(stages.size(), 30));
                for (int n = static_cast<int>(stages.size()); n <= last; ++n)
                {
                    Stage s;
                    char filler[16];
                    sprintf_s(filler, "%02d Dummy", n);
                    s.name = s.shortName = s.internal = filler;
                    s.map = static_cast<int16_t>(n);
                    auto slot = std::find(SlotNumbers().begin(), SlotNumbers().end(), n);
                    size_t k = static_cast<size_t>(slot - SlotNumbers().begin());
                    if (slot != SlotNumbers().end() && k < maps.size())
                    {
                        s.name = s.shortName = maps[k].name;
                        s.internal = "st" + std::to_string(n) + "-" + maps[k].id;
                        s.layouts = 1;
                        s.flag = 1;
                    }
                    stages.push_back(s);
                }
                write(rel, BuildStageset(stages));
            }

            // Map files under their slot numbers, and the previews.
            std::vector<std::vector<uint8_t>> strips;
            for (size_t k = 0; k < maps.size(); ++k)
            {
                wchar_t name[32];
                swprintf_s(name, L"st%02d", SlotNumbers()[k]);
                write(std::wstring(L"Assets\\Data\\Map\\") + name + L".alr", maps[k].alr);
                write(std::wstring(L"Assets\\Data\\Map\\") + name + L".dat", maps[k].dat);
                std::vector<uint8_t> strip;
                if (!DecodeImage(maps[k].preview, kStripW, kStripH, strip))
                {
                    if (!maps[k].preview.empty()) log(L"maps: %S: preview couldn't be read - a plain one is used", maps[k].id.c_str());
                    strip = PlainPreview();
                }
                strips.push_back(std::move(strip));
            }
            // The previews go to pdnet (LocalState\pdnet\map-previews.bin), which adds them to its own copy of MNet.alr
            // with the character select's portraits: one MNet.alr for both (a generated MNet.alr here kept the
            // portraits out, Experimental 191).
            if (!WriteIfChanged(previewFile, PreviewSheet(strips))) log(L"maps: couldn't write %s", previewFile.c_str());
            for (size_t k = 0; k < maps.size(); ++k)
            {
                slots += "slot\t" + std::to_string(SlotNumbers()[k]) + "\t" + std::to_string(k) + "\t" + maps[k].id + "\t" + maps[k].name + "\n";
                log(L"maps: %S = st%02d (\"%S\", preview %zu)", maps[k].id.c_str(), SlotNumbers()[k], maps[k].name.c_str(), k);
            }
        }

        // Files from earlier starts that aren't wanted now go; then the list of what this start made.
        std::string listText = ReadAll(listFile), newList;
        for (size_t at = 0; at < listText.size();)
        {
            size_t e = listText.find('\n', at);
            std::string line = listText.substr(at, (e == std::string::npos ? listText.size() : e) - at);
            at = e == std::string::npos ? listText.size() : e + 1;
            if (line.empty()) continue;
            int n = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0);
            std::wstring rel(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), rel.data(), n);
            if (!written.count(rel) && DeleteFileW((pdplDir + L"\\" + rel).c_str())) log(L"maps: %s removed (no longer wanted)", rel.c_str());
        }
        for (const auto& rel : written)
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, rel.c_str(), static_cast<int>(rel.size()), nullptr, 0, nullptr, nullptr);
            std::string u(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, rel.c_str(), static_cast<int>(rel.size()), u.data(), n, nullptr, nullptr);
            newList += u + "\n";
        }
        WriteIfChanged(listFile, newList);
        if (maps.empty()) { DeleteFileW(slotsFile.c_str()); DeleteFileW(previewFile.c_str()); }
        else WriteIfChanged(slotsFile, slots);
    }
}
