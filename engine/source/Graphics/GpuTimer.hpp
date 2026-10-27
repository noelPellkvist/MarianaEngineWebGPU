#pragma once
#include <webgpu/webgpu_cpp.h>
#include <cstdint>
#include <string>

// Times render passes on the GPU with timestamp queries, when the device can (TimestampQuery): each
// pass of a frame writes a timestamp as it begins and ends into the frame's query set, and EndFrame
// copies them out and reads them back; a few frames later the times reach the profiler
// (Profiler::SetGpuPasses). Without the feature (or without a device) nothing is timed.
namespace GpuTimer
{
    // After the device is made: whether it times, and why not.
    void Setup(bool timestamps, const std::string& why);
    bool Enabled();
    // Fills `writes` for a pass about to begin, named `label` in the profiler; false when this frame
    // isn't timed (its readback is still pending, or its passes ran out).
    bool Pass(const std::string& label, wgpu::PassTimestampWrites& writes);
    // After the frame's last pass: resolves its timestamps and reads them back.
    void EndFrame(uint64_t frame);
}
