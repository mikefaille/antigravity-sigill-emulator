#!/usr/bin/env python3
from elftools.elf.elffile import ELFFile
from capstone import *
import sys

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <path_to_binary>")
    sys.exit(1)

path = sys.argv[1]

bad_mnems = {
    "aesenc", "aesenclast", "aesdec", "aesdeclast", "aesimc", "aeskeygenassist",
    "pclmulqdq",
    "vaesenc", "vaesenclast", "vaesdec", "vaesdeclast",
    "vpclmulqdq",
}

with open(path, "rb") as f:
    elf = ELFFile(f)
    text = elf.get_section_by_name(".text")
    if not text:
        raise SystemExit("Error: No .text section found in binary.")

    code = text.data()
    text_va = text["sh_addr"]
    text_off = text["sh_offset"]

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

chunk_size = 1024 * 1024  # 1MB chunk size to keep memory low
total_hits = 0

print(f"[*] Scanning binary: {path}")
print(f"[*] Text section address: 0x{text_va:X}, file offset: 0x{text_off:X}, size: {len(code)} bytes")

for offset_in_text in range(0, len(code), chunk_size):
    chunk = code[offset_in_text : offset_in_text + chunk_size]
    chunk_va = text_va + offset_in_text
    # Disassemble chunk and search for unsupported instructions
    for insn in md.disasm(chunk, chunk_va):
        m = insn.mnemonic.lower()
        if (
            m in bad_mnems
            or m.startswith("v") and ("xmm" in insn.op_str or "ymm" in insn.op_str or "zmm" in insn.op_str)
        ):
            file_off = text_off + (insn.address - text_va)
            print(f"file_off=0x{file_off:x} va=0x{insn.address:x} {insn.mnemonic:16} {insn.op_str:40} bytes={insn.bytes.hex()}")
            total_hits += 1

print(f"\n[*] Scan complete. Total suspect instructions: {total_hits}")
