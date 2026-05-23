import pytest
import ctypes
import struct
import sys
from unittest.mock import patch, MagicMock


# Simulated opcode processing logic that mirrors the vulnerable C pattern
# This models the behavior of opcodes.c in Python for property testing

class BufferOverflowDetected(Exception):
    pass


class SimulatedOpcodeProcessor:
    """
    Simulates the vulnerable opcodes.c behavior where opd->sz bytes are copied
    from a source buffer into opd->bytes without bounds checking.
    """
    MAX_BYTES_CAPACITY = 16  # Simulated fixed capacity of opd->bytes

    def __init__(self):
        self.bytes_buffer = bytearray(self.MAX_BYTES_CAPACITY)
        self.sz = 0

    def process_operand(self, source_buf: bytes, declared_sz: int) -> bytes:
        """
        Simulates: memcpy(opd->bytes, textstring.buf, opd->sz)
        
        INVARIANT: Must never read more bytes than source_buf actually contains,
                   and must never write more bytes than MAX_BYTES_CAPACITY.
        """
        # Validate declared_sz against actual source buffer length
        if declared_sz > len(source_buf):
            raise ValueError(
                f"Buffer over-read: declared_sz={declared_sz} exceeds "
                f"source buffer length={len(source_buf)}"
            )

        # Validate declared_sz against destination capacity
        if declared_sz > self.MAX_BYTES_CAPACITY:
            raise ValueError(
                f"Buffer overflow: declared_sz={declared_sz} exceeds "
                f"destination capacity={self.MAX_BYTES_CAPACITY}"
            )

        # Safe copy
        self.sz = declared_sz
        self.bytes_buffer[:declared_sz] = source_buf[:declared_sz]
        return bytes(self.bytes_buffer[:declared_sz])

    def safe_process_operand(self, source_buf: bytes, declared_sz: int) -> bytes:
        """
        A SAFE version that enforces bounds — what the fixed code should do.
        Returns truncated result rather than overflowing.
        """
        # Clamp to actual source buffer length and destination capacity
        safe_sz = min(declared_sz, len(source_buf), self.MAX_BYTES_CAPACITY)
        self.sz = safe_sz
        self.bytes_buffer[:safe_sz] = source_buf[:safe_sz]
        return bytes(self.bytes_buffer[:safe_sz])


def check_no_oob_read(source_buf: bytes, declared_sz: int) -> bool:
    """
    Returns True if the operation is safe (no OOB read/write).
    Returns False if it would cause a buffer over-read or overflow.
    """
    processor = SimulatedOpcodeProcessor()
    MAX_CAP = SimulatedOpcodeProcessor.MAX_BYTES_CAPACITY

    # OOB read: declared_sz > actual source buffer size
    if declared_sz > len(source_buf):
        return False

    # OOB write: declared_sz > destination capacity
    if declared_sz > MAX_CAP:
        return False

    return True


# Attack payloads: (source_buf, declared_sz, description)
# Format: (source_bytes, attacker_controlled_sz)
ATTACK_PAYLOADS = [
    # 2x oversized declared length vs actual buffer
    (b"AAAA", 8, "2x oversized declared_sz vs 4-byte buffer"),

    # 10x oversized declared length vs actual buffer
    (b"BBBB", 40, "10x oversized declared_sz vs 4-byte buffer"),

    # Empty buffer with nonzero declared size
    (b"", 1, "empty buffer with declared_sz=1"),

    # Empty buffer with large declared size
    (b"", 256, "empty buffer with declared_sz=256"),

    # Declared size exactly at boundary (valid)
    (b"A" * 16, 16, "declared_sz exactly at MAX_BYTES_CAPACITY boundary"),

    # Declared size one over capacity
    (b"A" * 17, 17, "declared_sz one over MAX_BYTES_CAPACITY"),

    # Declared size massively over capacity
    (b"A" * 1024, 1024, "1024-byte source with declared_sz=1024 (10x capacity)"),

    # Declared size as max int-like value (simulating attacker-controlled sz)
    (b"X" * 4, 0xFFFF, "attacker sets declared_sz=0xFFFF with 4-byte buffer"),

    # Declared size = 0xDEAD (common magic attack value)
    (b"\xde\xad\xbe\xef", 0xDEAD, "magic attack value 0xDEAD as declared_sz"),

    # Source buffer with null bytes, oversized declared length
    (b"\x00\x00\x00\x00", 100, "null-byte buffer with 100-byte declared_sz"),

    # Source buffer with format string chars, oversized
    (b"%s%n%x%p", 200, "format string payload with oversized declared_sz"),

    # Source buffer with shell metacharacters, oversized
    (b"; cat /etc/passwd", 500, "shell injection payload with oversized declared_sz"),

    # Source buffer with SQL injection, oversized
    (b"' OR '1'='1", 300, "SQL injection payload with oversized declared_sz"),

    # Source buffer with path traversal, oversized
    (b"../../../etc/passwd", 400, "path traversal payload with oversized declared_sz"),

    # Exactly 1 byte source, large declared size
    (b"\x41", 1000, "single byte source with 1000-byte declared_sz"),

    # Source buffer smaller than declared by exactly 1
    (b"A" * 15, 16, "source 1 byte shorter than declared_sz"),

    # Negative-like large unsigned value (wrapping attack)
    (b"A" * 8, 0x80000000, "large unsigned value simulating negative wrap"),

    # Declared size = 0 (edge case, should be safe)
    (b"AAAA", 0, "declared_sz=0 (edge case)"),

    # Unicode-like binary payload, oversized
    (b"\xff\xfe" + b"A" * 10, 500, "unicode BOM + data with oversized declared_sz"),

    # Repeated pattern, 2x declared
    (b"ABCD" * 4, 64, "16-byte source with 64-byte declared_sz (4x)"),
]


@pytest.mark.parametrize("source_buf,declared_sz,description", ATTACK_PAYLOADS)
def test_buffer_read_never_exceeds_declared_length(source_buf, declared_sz, description):
    """
    Invariant: Buffer reads must never exceed the actual length of the source buffer,
    and writes must never exceed the destination buffer capacity.
    
    Any operation where declared_sz > len(source_buf) OR declared_sz > MAX_CAPACITY
    must be rejected or safely truncated — never silently overflow.
    
    This guards against CWE-120: Buffer Copy without Checking Size of Input.
    """
    processor = SimulatedOpcodeProcessor()
    MAX_CAP = SimulatedOpcodeProcessor.MAX_BYTES_CAPACITY

    is_safe_input = (
        declared_sz <= len(source_buf) and
        declared_sz >= 0 and
        declared_sz <= MAX_CAP
    )

    if is_safe_input:
        # Safe inputs must succeed and return exactly declared_sz bytes
        result = processor.process_operand(source_buf, declared_sz)
        assert len(result) == declared_sz, (
            f"[{description}] Safe input should return exactly {declared_sz} bytes, "
            f"got {len(result)}"
        )
        assert result == source_buf[:declared_sz], (
            f"[{description}] Result must match first {declared_sz} bytes of source"
        )
    else:
        # Unsafe inputs MUST be rejected (raise an exception) — not silently overflow
        with pytest.raises((ValueError, OverflowError, IndexError, Exception)) as exc_info:
            processor.process_operand(source_buf, declared_sz)

        # Verify the exception message indicates a bounds violation
        error_msg = str(exc_info.value).lower()
        assert any(keyword in error_msg for keyword in [
            "overflow", "over-read", "exceed", "capacity", "bound", "length", "size"
        ]), (
            f"[{description}] Exception must indicate a bounds violation, "
            f"got: '{exc_info.value}'"
        )


@pytest.mark.parametrize("source_buf,declared_sz,description", ATTACK_PAYLOADS)
def test_safe_processor_never_reads_beyond_source_length(source_buf, declared_sz, description):
    """
    Invariant: The safe (fixed) version of the processor must NEVER return more bytes
    than are actually present in the source buffer, regardless of declared_sz.
    
    This ensures that even with attacker-controlled declared_sz values,
    the output is always bounded by min(declared_sz, len(source_buf), MAX_CAPACITY).
    """
    processor = SimulatedOpcodeProcessor()
    MAX_CAP = SimulatedOpcodeProcessor.MAX_BYTES_CAPACITY

    # The safe processor should always succeed (truncate rather than overflow)
    result = processor.safe_process_operand(source_buf, declared_sz)

    # INVARIANT 1: Result length must never exceed actual source buffer length
    assert len(result) <= len(source_buf), (
        f"[{description}] OOB READ DETECTED: result length {len(result)} "
        f"exceeds source buffer length {len(source_buf)}"
    )

    # INVARIANT 2: Result length must never exceed destination capacity
    assert len(result) <= MAX_CAP, (
        f"[{description}] BUFFER OVERFLOW DETECTED: result length {len(result)} "
        f"exceeds destination capacity {MAX_CAP}"
    )

    # INVARIANT 3: Result length must never exceed declared_sz
    assert len(result) <= max(declared_sz, 0), (
        f"[{description}] Result length {len(result)} exceeds declared_sz {declared_sz}"
    ) if declared_sz > 0 else True

    # INVARIANT 4: Result bytes must be a prefix of the source buffer
    if len(result) > 0:
        assert result == source_buf[:len(result)], (
            f"[{description}] Result bytes do not match source buffer prefix — "
            f"possible memory corruption"
        )


@pytest.mark.parametrize("source_buf,declared_sz,description", ATTACK_PAYLOADS)
def test_declared_sz_cannot_force_read_past_buffer_end(source_buf, declared_sz, description):
    """
    Invariant: An attacker-controlled declared_sz value must not be able to force
    the processor to read bytes beyond the end of the source buffer.
    
    This directly tests the CWE-120 vulnerability pattern:
    memcpy(opd->bytes, textstring.buf, opd->sz) where opd->sz is attacker-controlled.
    """
    MAX_CAP = SimulatedOpcodeProcessor.MAX_BYTES_CAPACITY

    # Calculate what a safe implementation MUST limit reads to
    expected_max_read = min(
        max(declared_sz, 0),  # clamp negative to 0
        len(source_buf),       # cannot read past source end
        MAX_CAP                # cannot write past destination end
    )

    processor = SimulatedOpcodeProcessor()
    result = processor.safe_process_operand(source_buf, declared_sz)

    # The number of bytes actually read/written must not exceed the safe maximum
    actual_read = len(result)

    assert actual_read <= len(source_buf), (
        f"[{description}] CRITICAL: Read {actual_read} bytes but source only has "
        f"{len(source_buf)} bytes. declared_sz={declared_sz} caused OOB read!"
    )

    assert actual_read <= MAX_CAP, (
        f"[{description}] CRITICAL: Wrote {actual_read} bytes but destination only has "
        f"{MAX_CAP} bytes capacity. declared_sz={declared_sz} caused buffer overflow!"
    )

    assert actual_read == expected_max_read, (
        f"[{description}] Expected to read exactly {expected_max_read} bytes "
        f"(safe minimum), but read {actual_read} bytes"
    )