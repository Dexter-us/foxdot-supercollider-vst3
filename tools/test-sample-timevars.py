#!/usr/bin/env python3
"""Compile and exercise the sample/rate control evaluators from the real processor."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "spikes/vst3-callback/processor.cpp").read_text()
start = source.index("    static bool parseTimeVarControl(")
end = source.index("    void setEditorStatus(", start)
functions = source[start:end]
cpp = r"""
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
class SampleControls {
public:
    struct TimeVarControl {
        bool active=false, linear=false;
        std::size_t count=0;
        std::array<double,8> values{}, durations{};
    };
"""
cpp += functions
cpp += r"""
};
int main() {
    SampleControls::TimeVarControl control;
    assert(SampleControls::parseTimeVarControl("v:0,1:2,2",-1000000,1000000,control));
    assert(SampleControls::validSampleSelectionControl(control));
    assert(SampleControls::validSampleRateControl(SampleControls::TimeVarControl{}));
    assert(SampleControls::evaluateSampleSelection(control,0,9)==0);
    assert(SampleControls::evaluateSampleSelection(control,2,9)==1);
    assert(SampleControls::evaluateSampleSelection(control,4,9)==0);
    assert(SampleControls::parseTimeVarControl("l:0,1:4,4",-1000000,1000000,control));
    assert(SampleControls::evaluateSampleSelection(control,1.99,9)==0);
    assert(SampleControls::evaluateSampleSelection(control,2,9)==1);
    assert(SampleControls::evaluateSampleSelection(control,4,9)==1);
    assert(SampleControls::evaluateSampleSelection(control,6,9)==0);
    assert(SampleControls::parseTimeVarControl("l:0.75,1.5:4,4",-8,8,control));
    assert(std::abs(SampleControls::evaluateTimeVar(control,2,0)-1.125f)<1e-6f);
    assert(SampleControls::parseTimeVarControl("v:1,-1:4,4",-8,8,control));
    assert(SampleControls::validSampleRateControl(control));
    assert(SampleControls::evaluateTimeVar(control,0,0)==1);
    assert(SampleControls::evaluateTimeVar(control,4,0)==-1);
    assert(SampleControls::parseTimeVarControl("l:0.75,0:4,4",-8,8,control));
    assert(!SampleControls::validSampleRateControl(control));
    assert(SampleControls::parseTimeVarControl("v:0,1.5:4,4",-1000000,1000000,control));
    assert(!SampleControls::validSampleSelectionControl(control));
    std::cout << "PASS: production time-var parsing, categorical sample switching, and playback-rate glide/reversal controls\n";
}
"""
with tempfile.TemporaryDirectory() as directory:
    cpp_file = Path(directory) / "sample-timevars.cpp"
    binary = Path(directory) / "sample-timevars"
    cpp_file.write_text(cpp)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    str(cpp_file), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
