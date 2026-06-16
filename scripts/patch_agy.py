#!/usr/bin/env python3
import struct
import sys
import os
import re
import shutil
import hashlib
from datetime import datetime

# ---------------------------------------------------------------------------
# Signature table: (description, regex, jmp_offset_from_match_start, insn_len)
# Tried in order; first unpatched match wins.
# jmp_offset: byte distance from regex match start to the instruction being replaced.
# insn_len: length of the instruction being replaced (must be >= 5 for the JMP).
# ---------------------------------------------------------------------------
SIGNATURES = [
    # Primary — stable across all known builds (May22–Jun4 2026):
    # push rbp; mov rbp,rsp; push rbx; push rax; call; mov eax,[rip+x]; test eax,0x40000; je
    (
        "primary-aes-bit18",
        re.compile(rb'\x55\x48\x89\xe5\x53\x50\xe8.{4}\x8b\x05.{4}\xa9\x00\x00\x04\x00\x0f\x84', re.DOTALL),
        11, 6,
    ),
    # Wildcard TEST mask — survives future Go feature bit reassignment
    (
        "generic-feature-check",
        re.compile(rb'\x55\x48\x89\xe5\x53\x50\xe8.{4}\x8b\x05.{4}\xa9.{4}\x0f\x84', re.DOTALL),
        11, 6,
    ),
    # No push-rax variant (smaller frame, jmp_offset = 10)
    (
        "no-push-rax",
        re.compile(rb'\x55\x48\x89\xe5\x53\xe8.{4}\x8b\x05.{4}\xa9.{4}\x0f\x84', re.DOTALL),
        10, 6,
    ),
    # TEST AL form — seen in pclmulqdq checks (compact 2-byte TEST AL,imm8).
    # insn_len=6: the preceding MOV EAX,[RIP+disp32] (\x8b\x05 + 4-byte disp) is 6 bytes,
    # so we write E9+4+NOP (6 bytes), which the already-patched detector (\xe9.{4}\x90) can match.
    (
        "test-al-byte-check",
        re.compile(rb'\x55\x48\x89\xe5\x53\x50\xe8.{4}\x8b\x05.{4}\xa8.\x0f\x84', re.DOTALL),
        11, 6,
    ),
]

# ---------------------------------------------------------------------------
# Patterns that confirm the function epilogue — tried in order within 1500 bytes.
# ---------------------------------------------------------------------------
EPILOGUES = [
    b'\x48\x83\xc4\x08\x5b\x5d\xc3',   # add rsp,8; pop rbx; pop rbp; ret  (observed in all versions)
    b'\x48\x83\xc4\x10\x5b\x5d\xc3',   # add rsp,16; pop rbx; pop rbp; ret
    b'\x48\x83\xc4\x18\x5b\x5d\xc3',   # add rsp,24; pop rbx; pop rbp; ret
    b'\x5b\x5d\xc3',                     # pop rbx; pop rbp; ret
    b'\x5d\xc3',                          # pop rbp; ret
]

# ---------------------------------------------------------------------------
# Already-patched detector: same function prologue but with our JMP+NOP in place
# of the MOV instruction.  Pattern: prologue + call + e9 [4-byte offset] 90 + test + je
# ---------------------------------------------------------------------------
ALREADY_PATCHED_SIGS = [
    re.compile(rb'\x55\x48\x89\xe5\x53\x50\xe8.{4}\xe9.{4}\x90\xa9.{4}\x0f\x84', re.DOTALL),
    re.compile(rb'\x55\x48\x89\xe5\x53\xe8.{4}\xe9.{4}\x90\xa9.{4}\x0f\x84', re.DOTALL),
]


class GoFeatureEnforcementPatcher:
    def __init__(self, target_path):
        self.target_path = target_path
        with open(target_path, 'rb') as f:
            self.binary_data = bytearray(f.read())
        self.segments = self._extract_elf_pt_load_segments()
        size = len(self.binary_data)
        digest = hashlib.sha256(self.binary_data).hexdigest()[:16]
        mtime = datetime.fromtimestamp(os.path.getmtime(target_path)).isoformat()
        print(f"[*] Target: {target_path}  size={size}  sha256={digest}  mtime={mtime}")

    def _extract_elf_pt_load_segments(self):
        if self.binary_data[:4] != b'\x7fELF':
            raise ValueError("Not a valid ELF binary.")
        if self.binary_data[4] != 2:
            raise ValueError("Only 64-bit ELF supported.")
        phoff = struct.unpack_from("<Q", self.binary_data, 32)[0]
        phentsize = struct.unpack_from("<H", self.binary_data, 54)[0]
        phnum = struct.unpack_from("<H", self.binary_data, 56)[0]
        segments = []
        for i in range(phnum):
            off = phoff + i * phentsize
            if struct.unpack_from("<I", self.binary_data, off)[0] == 1:
                segments.append({
                    'file_offset': struct.unpack_from("<Q", self.binary_data, off + 8)[0],
                    'virtual_address': struct.unpack_from("<Q", self.binary_data, off + 16)[0],
                    'file_size': struct.unpack_from("<Q", self.binary_data, off + 32)[0],
                })
        return segments

    def _va_to_file_offset(self, va):
        for seg in self.segments:
            if seg['virtual_address'] <= va < seg['virtual_address'] + seg['file_size']:
                return va - seg['virtual_address'] + seg['file_offset']
        return None

    def _find_epilogue(self, start_idx):
        for pattern in EPILOGUES:
            idx = self.binary_data.find(pattern, start_idx, start_idx + 1500)
            if idx != -1:
                print(f"[+] Epilogue '{pattern.hex()}' at 0x{idx:X}  (distance: {idx - start_idx} bytes)")
                return idx
        return -1

    def _is_already_patched(self):
        for sig in ALREADY_PATCHED_SIGS:
            m = sig.search(self.binary_data)
            if m:
                print(f"[=] Already-patched signature found at 0x{m.start():X}")
                return True
        return False

    def execute_signature_patch(self):
        print("[*] Scanning for Go cpu.Initialize enforcement pattern...")

        if self._is_already_patched():
            return "already_patched"

        for desc, sig, jmp_off, insn_len in SIGNATURES:
            match = sig.search(self.binary_data)
            if not match:
                print(f"[-] {desc}: no match")
                continue

            start_idx = match.start()
            jmp_idx = start_idx + jmp_off
            original = self.binary_data[jmp_idx:jmp_idx + insn_len]
            print(f"[+] {desc}: match at 0x{start_idx:X}  jmp_site=0x{jmp_idx:X}  original={original.hex()}")

            epi_idx = self._find_epilogue(start_idx)
            if epi_idx == -1:
                print(f"[-] Epilogue not found within 1500 bytes — trying next signature")
                continue

            rel_offset = epi_idx - (jmp_idx + 5)
            nops = bytearray([0x90] * (insn_len - 5))
            patch = bytearray([0xE9]) + bytearray(struct.pack('<i', rel_offset)) + nops
            print(f"[*] Patch: {patch.hex()}  (JMP rel=0x{rel_offset:X} → epilogue 0x{epi_idx:X})")

            self.binary_data[jmp_idx:jmp_idx + insn_len] = patch
            print(f"[+] Patch applied at 0x{jmp_idx:X}")
            return "patched"

        print("[-] All signature variants exhausted — no unpatched match found")
        return None

    def execute_legacy_patch(self, targeted_features=[b"aes", b"pclmulqdq"]):
        """Fallback: scan for Go option struct layout and zero the required flag."""
        print("[*] Attempting legacy structure-based scan...")
        modified = 0
        file_len = len(self.binary_data)
        for offset in range(0, file_len - 32, 8):
            string_ptr, string_len, feature_ptr = struct.unpack_from("<QQQ", self.binary_data, offset)
            if self.binary_data[offset + 24] != 1:
                continue
            if self.binary_data[offset + 25:offset + 32] != b"\x00" * 7:
                continue
            str_off = self._va_to_file_offset(string_ptr)
            if str_off is None or str_off + string_len > file_len:
                continue
            name = self.binary_data[str_off:str_off + string_len]
            if name in targeted_features and string_len == len(name):
                feat_off = self._va_to_file_offset(feature_ptr)
                if feat_off is not None:
                    print(f"[+] Legacy: '{name.decode()}' struct @0x{offset:X}  required 0x01→0x00")
                    self.binary_data[offset + 24] = 0
                    modified += 1
        return modified

    def run_patch(self):
        result = None
        try:
            result = self.execute_signature_patch()
        except Exception as e:
            print(f"[!] Signature patch error: {e}")

        if result == "already_patched":
            print("[=] Binary is already patched — nothing to do")
            return  # exit 0

        if not result:
            print("[*] Falling back to legacy structure scan...")
            try:
                count = self.execute_legacy_patch()
                if count > 0:
                    result = "patched"
                    print(f"[+] Legacy scan patched {count} enforcement flag(s)")
                else:
                    print("[-] Legacy scan found nothing — binary incompatible or already patched")
                    return  # exit 0 (nothing to do)
            except Exception as e:
                print(f"[!] Legacy patch error: {e}")
                sys.exit(1)

        if result == "patched":
            backup = self.target_path + ".bak"
            if not os.path.exists(backup):
                shutil.copy2(self.target_path, backup)
                print(f"[*] Pre-patch backup saved: {backup}")
            with open(self.target_path, 'wb') as f:
                f.write(self.binary_data)
            os.chmod(self.target_path, 0o755)
            print(f"[+] Done — patched binary written to: {self.target_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 patch_agy.py <path_to_binary>")
        sys.exit(1)
    try:
        GoFeatureEnforcementPatcher(sys.argv[1]).run_patch()
    except Exception as e:
        print(f"[!] Fatal: {e}")
        sys.exit(1)
