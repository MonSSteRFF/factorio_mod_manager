// Tiny x64 pattern scans over engine code (no full disassembler needed).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace modpacks {

// Finds `test dword|byte [reg+disp], imm` on the field at `fieldOffset` within
// the first `length` bytes of `code` and returns the tested bit mask (as a
// dword mask). Used to read the "owned by parent" bit of
// agui::Widget::usageBitMask from agui::Widget::clearParentWidget, because the
// constant has no name in the PDB.
inline std::optional<uint32_t> FindTestedMask(const uint8_t* code, size_t length, uint32_t fieldOffset) {
    for (size_t i = 0; i + 8 < length; ++i) {
        size_t p = i;
        if ((code[p] & 0xF0) == 0x40) ++p;  // REX prefix
        uint8_t opcode = code[p];
        if (opcode != 0xF7 && opcode != 0xF6) continue;
        uint8_t modrm = code[p + 1];
        uint8_t mod = modrm >> 6;
        uint8_t reg = (modrm >> 3) & 7;
        uint8_t rm = modrm & 7;
        if (reg != 0 || mod == 0 || mod == 3 || rm == 4) continue;  // TEST r/m, imm with [reg+disp], no SIB
        size_t q = p + 2;
        int32_t disp;
        if (mod == 1) {
            disp = static_cast<int8_t>(code[q]);
            q += 1;
        } else {
            std::memcpy(&disp, code + q, 4);
            q += 4;
        }
        if (q + 4 > length) break;
        if (opcode == 0xF7) {
            if (static_cast<uint32_t>(disp) != fieldOffset) continue;
            uint32_t imm;
            std::memcpy(&imm, code + q, 4);
            return imm;
        }
        // Byte test of one of the four bytes of the field.
        uint32_t byteIndex = static_cast<uint32_t>(disp) - fieldOffset;
        if (static_cast<uint32_t>(disp) < fieldOffset || byteIndex > 3) continue;
        return static_cast<uint32_t>(code[q]) << (8 * byteIndex);
    }
    return std::nullopt;
}

}  // namespace modpacks
