#!/usr/bin/env python3
"""Compile real WAV/worker code, then render the native SC sample graph.

Developer test: requires g++, scsynth and sclang. Does not rebuild SC/UGens and
does not claim to validate the user's Windows host.
"""
import array
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import wave
from importlib.util import module_from_spec, spec_from_file_location

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
spec = spec_from_file_location("audio_probe", root / "tools/test-core-synth-audio.py")
audio = module_from_spec(spec)
spec.loader.exec_module(audio)
message, bundle = audio.message, audio.bundle

cpp = r'''
#include "portable_samples.h"
#include "python_runtime.h"
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
using namespace PortableSamples;
std::filesystem::path fakeBinary;
std::filesystem::path pluginBinaryPath() { return fakeBinary; }
struct SndBuf {
    double samplerate=0, sampledur=0; float* data=nullptr;
    int channels=0, samples=0, frames=0;
};
struct Update { int reads=0, writes=0; };
struct World {
    unsigned mNumSndBufs=16;
    SndBuf rt[16]{}, nrt[16]{};
    Update updates[16]{};
    SndBuf* mSndBufs=rt;
    SndBuf* mSndBufsNonRealTimeMirror=nrt;
    Update* mSndBufUpdates=updates;
};
struct Harness {
    static constexpr std::size_t kPlayerCount=8;
    struct TimeVarControl {
        bool active=false;
        std::size_t count=0;
        std::array<double,8> values{};
    };
    struct PatternState {
        bool active=false; int synthIndex=0; std::size_t eventCount=0;
        std::uint64_t sampleWorld=0;
        TimeVarControl sampleControl;
        std::array<PythonNoteEvent,256> events{};
    };
    World ownedWorld;
    World* world_=&ownedWorld;
    std::mutex worldAccessMutex_;
    std::uint64_t sampleWorldGeneration_=1;
    std::string worldInitializationError_;
    bool initializeWorld() { return world_ != nullptr; }
    void setEditorStatus(const std::string&) {}
'''
processor = (plugin / "processor.cpp").read_text()
start = processor.index("    struct CachedSample {")
end = processor.index("    bool prepareSynthGraphs(", start)
cpp += processor[start:end]
cpp += r'''
    std::vector<std::unique_ptr<CachedSample>> sampleCache_;
    std::size_t sampleCacheBytes_=0;
};
int main(int argc, char** argv) {
    if (argc != 3) return 3;
    if (std::string(argv[1]) == "--dump") {
        Wave w; std::string error;
        if (!readWave(argv[2], w, error)) { std::cerr << error; return 1; }
        std::uint32_t frames=w.frames, rate=static_cast<std::uint32_t>(w.sampleRate);
        std::cout.write(reinterpret_cast<const char*>(&frames),4);
        std::cout.write(reinterpret_cast<const char*>(&rate),4);
        std::cout.write(reinterpret_cast<const char*>(w.mono.data()), frames*4);
        return 0;
    }
    std::filesystem::path directory=argv[1], samples=argv[2];
    std::string error;
    for (auto& f : std::filesystem::directory_iterator(directory)) {
        Wave w; bool good=readWave(f.path(),w,error);
        if (f.path().filename().u8string().find("bad-")==0) assert(!good && !error.empty());
        else {
            assert(good && error.empty() && w.frames==4);
            assert(w.mono.size()==8 && w.sourceChannels>=1);
            assert(std::abs(w.mono[0]) < 0.00001);
            assert(w.mono[1] > 0.24 && w.mono[1] < 0.26);
            assert(w.mono[2] < -0.24 && w.mono[2] > -0.26);
            const float last=f.path().filename()=="float-over-one.wav" ? 2.0f : .5f;
            assert(std::abs(w.mono[3]-last)<.01f);
        }
    }
    std::filesystem::path selected;
    assert(resolveWave(samples,'x',-1,selected,error));
    assert(selected.filename()=="01-test-kick-tight.wav");
    assert(resolveWave(samples,'x',2,selected,error));
    assert(selected.filename()=="00-test-kick.wav");
    assert(!resolveWave(samples,'Z',0,selected,error));
    assert(!resolveWave(samples,']',0,selected,error));
    fakeBinary=samples.parent_path().parent_path()/"Contents"/"x86_64-win"/"dexter_sc_tone.vst3";
    Harness h;
    std::array<Harness::PatternState,8> updates{};
    updates[0].active=true; updates[0].synthIndex=57; updates[0].eventCount=2;
    for (int i=0;i<2;++i) {
        updates[0].events[i].sampleSymbol='x';
        updates[0].events[i].sampleIndex=i;
    }
    assert(h.prepareSampleBuffers(updates,1,error));
    assert(h.sampleCache_.size()==2);
    assert(updates[0].sampleWorld==1);
    assert(updates[0].events[0].sampleBuffer==1 && updates[0].events[1].sampleBuffer==2);
    assert(std::abs(updates[0].events[0].sampleDurationSeconds-.45)<0.0001);
    auto* data=h.ownedWorld.rt[1].data;
    assert(data==h.ownedWorld.nrt[1].data && data!=nullptr);
    assert(h.ownedWorld.rt[1].samplerate==44100 && h.ownedWorld.rt[1].channels==1);
    assert(h.prepareSampleBuffers(updates,1,error));
    assert(h.sampleCache_.size()==2 && data==h.ownedWorld.rt[1].data);
    updates[0].sampleControl.active=true;
    updates[0].sampleControl.count=2;
    updates[0].sampleControl.values[0]=0;
    updates[0].sampleControl.values[1]=1;
    assert(h.prepareSampleBuffers(updates,1,error));
    assert(updates[0].events[0].sampleVariantCount==2);
    assert(updates[0].events[0].sampleVariantBuffers[0]==1);
    assert(updates[0].events[0].sampleVariantBuffers[1]==2);
    updates[0].sampleControl.active=false;
    updates[0].events[0].sampleSymbol='a'; updates[0].eventCount=1;
    auto oldBytes=h.sampleCacheBytes_;
    h.sampleCacheBytes_=kCacheBudgetBytes;
    assert(!h.prepareSampleBuffers(updates,1,error));
    assert(error.find("128 MiB")!=std::string::npos && h.sampleCache_.size()==2);
    h.sampleCacheBytes_=oldBytes; h.ownedWorld.mNumSndBufs=3;
    assert(!h.prepareSampleBuffers(updates,1,error));
    assert(error.find("buffers are full")!=std::string::npos);
    h.ownedWorld.mNumSndBufs=16;
    assert(h.prepareSampleBuffers(updates,1,error));
    assert(h.sampleCache_.size()==3);
    updates[0].events[0].sampleIndex=1;
    assert(h.prepareSampleBuffers(updates,1,error));
    assert(h.sampleCache_.size()==4 && h.ownedWorld.rt[4].samplerate==48000);
    assert(h.ownedWorld.rt[4].channels==1 && h.ownedWorld.rt[4].frames==33600);
    h.detachSampleBuffers();
    for (int i=1;i<=4;++i) assert(!h.ownedWorld.rt[i].data && !h.ownedWorld.nrt[i].data);
    assert(data==h.sampleCache_[0]->wave.mono.data());
    h.sampleCache_.clear();
    std::cout << "PASS: real decoder, format rejection, selection, actual worker/cache code, budgets and ownership detachment\n";
}
'''

def wav_bytes(payload, encoding=1, bits=16, channels=1, extensible=False):
    alignment = channels * bits // 8
    fmt = struct.pack("<HHIIHH", 0xfffe if extensible else encoding, channels, 44100,
                      44100 * alignment, alignment, bits)
    if extensible:
        fmt += struct.pack("<HHI", 22, bits, 0) + struct.pack("<I",encoding)
        fmt += bytes.fromhex("00001000800000aa00389b71")
    body = b"WAVEfmt " + struct.pack("<I",len(fmt)) + fmt
    body += b"data" + struct.pack("<I",len(payload)) + payload + b"\0"*(len(payload)%2)
    return b"RIFF"+struct.pack("<I",len(body))+body

with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    samples = directory / "dexter_sc_tone.vst3/Portable/Samples"
    shutil.copytree(plugin/"Portable/Samples",samples)
    cases = directory/"cases"
    cases.mkdir()
    for bits in (8,16,24,32):
        payload=bytearray()
        for value in (0,.25,-.25,.5):
            if bits==8: payload.append(round(value*128+128))
            else: payload.extend(round(value*(1<<(bits-1))).to_bytes(bits//8,"little",signed=True))
        (cases/f"pcm{bits}.wav").write_bytes(wav_bytes(payload,bits=bits))
    stereo=struct.pack("<8h",0,0,8192,8192,-8192,-8192,16384,16384)
    (cases/"stereo.wav").write_bytes(wav_bytes(stereo,channels=2))
    floating=struct.pack("<4f",0,.25,-.25,.5)
    (cases/"float.wav").write_bytes(wav_bytes(floating,3,32))
    (cases/"float-over-one.wav").write_bytes(wav_bytes(struct.pack("<4f",0,.25,-.25,2),3,32))
    (cases/"extended-float.wav").write_bytes(wav_bytes(floating,3,32,extensible=True))
    (cases/"extended-pcm.wav").write_bytes(wav_bytes(struct.pack("<4h",0,8192,-8192,16384),extensible=True))
    bad_cases={
        "truncated":wav_bytes(stereo)[:-1], "not-wav":b"x"*100,
        "compressed":wav_bytes(floating,6,32), "nan":wav_bytes(struct.pack("<f",math.nan),3,32),
        "alignment":wav_bytes(b"123",bits=16), "over-range":wav_bytes(struct.pack("<f",17),3,32),
        "empty":wav_bytes(b""),
    }
    for name,contents in bad_cases.items(): (cases/f"bad-{name}.wav").write_bytes(contents)
    unit, binary = directory/"samples.cpp",directory/"samples-test"
    unit.write_text(cpp)
    subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-I",str(plugin),str(unit),"-o",str(binary)],check=True)
    subprocess.run([str(binary),str(cases),str(samples)],check=True)

    # Fill an SC buffer from the actual C++ decoder output, not b_allocRead.
    dumped=subprocess.run([str(binary),"--dump",str(samples/"a/lower/01-test-stereo-chirp.wav")],
                          check=True,capture_output=True).stdout
    frames, sample_rate = struct.unpack_from("<II",dumped)
    assert sample_rate==48000
    source=array.array("f"); source.frombytes(dumped[8:])
    blob=bytes(int(v,16) for v in re.findall(r"0x([0-9a-fA-F]{2})",(plugin/"sample_synthdef.h").read_text()))
    probe_code=f'SynthDef("DexterSampleProbe", {{Out.ar(0,In.ar(16,1))}}).writeDefFile("{directory}"); 0.exit;\n'
    env=dict(os.environ,QT_QPA_PLATFORM="minimal",QT_QUICK_BACKEND="software",
             QT_OPENGL="software",QTWEBENGINE_DISABLE_SANDBOX="1",
             QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu")
    compiled=subprocess.run(["sclang"],input=probe_code,text=True,capture_output=True,env=env,timeout=30)
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    score=[bundle(0,message("/d_recv",blob),message("/d_recv",(directory/"DexterSampleProbe.scsyndef").read_bytes()),
                  message("/b_alloc",1,frames,1))]
    for i in range(0,frames,1024):
        score.append(bundle(.05,message("/b_setn",1,i,min(1024,frames-i),*source[i:i+1024])))
    score.append(bundle(.1,message("/s_new","DexterSampleProbe",900,1,0)))
    starts=[]
    for index,rate in enumerate((1.,-1.,2.,-2.,1.)):
        start_time=.25+index*1.25
        starts.append((start_time,rate))
        score.append(bundle(start_time,message("/s_new","DexterSample",100+index,0,0),
                            message("/n_set",100+index,0,16.,2,rate,6,64.,7,0.,10,.4,22,1.)))
        score.append(bundle(start_time+1.,message("/n_free",100+index)))
    score.append(bundle(6.5,message("/c_set",0,0)))
    score_path,wav_path=directory/"score.osc",directory/"render.wav"
    score_path.write_bytes(b"".join(score))
    rendered=subprocess.run(["scsynth","-D","0","-R","0","-i","0","-o","2","-z","64",
                             "-N",str(score_path),"_",str(wav_path),"48000","WAV","float"],
                            capture_output=True,text=True,timeout=30)
    assert rendered.returncode==0 and "FAILURE" not in rendered.stdout,rendered.stdout+rendered.stderr
    output,channels,rate=audio.read_float_wav(wav_path)
    mono=output[::channels]
    assert all(math.isfinite(value) for value in mono)
    for start_time,rate in starts:
        start=int(start_time*48000)
        expected=list(source[::int(abs(rate))]) if rate>0 else list(source[::-int(abs(rate))])
        # SC block scheduling plus the native LPF can shift the onset slightly.
        best=-1.
        for lag in range(-64,65):
            actual=mono[start+lag:start+lag+len(expected)]
            if len(actual)!=len(expected): continue
            dot=sum(a*b for a,b in zip(actual,expected))
            energy=sum(a*a for a in actual)*sum(b*b for b in expected)
            if energy: best=max(best,dot/math.sqrt(energy))
        assert best>.97,(rate,"sample direction/speed mismatch",best)
        assert max(abs(v) for v in mono[start+100:start+int(.2*48000)])>.02
        end=start+round(frames/abs(rate))
        assert max(abs(v) for v in mono[end+128:end+512])<1e-5,(rate,"did not stop at natural length")
        print(f"PASS: native PlayBuf rate={rate:+g}, decoded-buffer correlation={best:.5f}, natural stop")
print("PASS: sample buffer playback, reverse, pitch/rate, repeated fresh nodes; no disk UGen or libsndfile reader used")
