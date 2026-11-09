#!/usr/bin/env python3
"""Apply a small, hash-guarded host-I/O adaptation to pinned upstream DiskIO.

The DiskIn, VDiskIn and DiskOut DSP functions remain upstream code. Changes:
asynchronous reads in a host-driven World; multi-producer queue; close barrier;
finite short-file loop handling; optional bridge-managed recording duration.
"""
import hashlib
import pathlib
import sys

EXPECTED = "37b5f27a58f48950a6ccdd8ce6c1ec6cbaa32aa36eea71ad90a3cecfe655ffe1"


def adapt(source):
    # Windows Git may check out CRLF; guard the canonical LF source, not EOLs.
    source = source.replace(b"\r\n", b"\n")
    if hashlib.sha256(source).hexdigest() != EXPECTED:
        raise ValueError("DiskIO source hash differs from the pinned SuperCollider revision")
    text = source.decode("utf-8")

    def replace(old, new, count=1):
        nonlocal text
        if text.count(old) != count:
            raise ValueError(f"Upstream patch anchor changed: {old[:80]!r}")
        text = text.replace(old, new)

    replace('#include "SC_SyncCondition.h"\n', "")
    replace("#include <sndfile.h>", '#include <sndfile.h>\n#include <algorithm>\n'
            '#include "DiskIOQueueTypes.h"')
    replace("    void Perform();", "    dexter_disk::Barrier* mBarrier = nullptr;\n    void Perform();")
    replace("void DiskIOMsg::Perform() {\n    NRTLock(mWorld);",
            "void DiskIOMsg::Perform() {\n"
            "    if (mBarrier) { mBarrier->signal(); return; }\n    NRTLock(mWorld);")
    start = text.index("struct DiskIOThread {")
    end = text.index("\nDiskIOThread* gDiskIO;", start)
    text = text[:start] + '#include "DiskIOThread.inc"\n' + text[end:]
    replace("if (unit->mWorld->mRealTime)", "if (true /* host callback: always asynchronous */)", 2)
    replace("    case kDiskCmd_ReadLoop:\n", "    case kDiskCmd_ReadLoop:\n")
    loop_start = text.index("        count = sf_readf_float", text.index("case kDiskCmd_ReadLoop:"))
    loop_end = text.index("        break;", loop_start)
    text = (text[:loop_start] +
            "        dexter_disk::readLoop(GETSNDFILE(buf), buf->data + mPos * buf->channels,\n"
            "                              mFrames, buf->channels);\n" + text[loop_end:])
    replace("count = buf->sndfile ? sf_readf_float(GETSNDFILE(buf), buf->data + mPos * buf->channels, mFrames) : 0;",
            "count = buf->sndfile ? std::max<sf_count_t>(0, sf_readf_float(GETSNDFILE(buf), "
            "buf->data + mPos * buf->channels, mFrames)) : 0;")
    replace("count = sf_writef_float(GETSNDFILE(buf), buf->data + mPos * buf->channels, mFrames);",
            "dexter_disk::writeFrames(mWorld, mBufNum, buf->data + mPos * buf->channels, mFrames);")
    # SC_Unit.h replaces Windows IN/OUT annotation macros. Parse the complete
    # Windows SDK first, before those DSP macros can affect winuser/RPC headers.
    prefix = '#ifdef _WIN32\n#include <windows.h>\n#endif\n'
    return prefix + text + '\n#include "DiskIOBridgeImpl.inc"\n'


if __name__ == "__main__":
    if sys.argv[1] == "--self-test":
        original = pathlib.Path(sys.argv[2]).read_bytes().replace(b"\r\n", b"\n")
        assert adapt(original) == adapt(original.replace(b"\n", b"\r\n"))
        try:
            adapt(original + b"\n// unexpected source change\n")
        except ValueError:
            pass
        else:
            raise AssertionError("Unpinned source was accepted")
        print("PASS pinned source/hash/patch anchors, LF and Windows CRLF")
    else:
        pathlib.Path(sys.argv[2]).write_text(
            adapt(pathlib.Path(sys.argv[1]).read_bytes()), encoding="utf-8")
