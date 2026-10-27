#include "Graphics/GpuTimer.hpp"
#include "Graphics/Init.hpp"

#include <Mariana/Foundation/Profiler.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace GpuTimer
{
    namespace
    {
        constexpr uint32_t kMaxPasses = 128;
        constexpr uint32_t kQueries = kMaxPasses * 2;
        constexpr size_t kSlots = 3;

        enum SlotState : int { kFree, kRecording, kReading };

        // One frame's timestamps: written by its passes, resolved, read back while later frames run.
        struct Slot
        {
            wgpu::QuerySet set;
            wgpu::Buffer resolve;
            wgpu::Buffer readback;
            std::vector<std::string> labels; // per pass
            uint32_t used = 0;               // timestamps written
            uint64_t frame = 0;
            std::atomic<int> state{ kFree };
        };

        struct Result
        {
            uint64_t frame = 0;
            std::vector<Profiler::GpuPass> passes;
        };

        struct State
        {
            bool enabled = false;
            std::array<std::shared_ptr<Slot>, kSlots> slots;
            Slot* current = nullptr;
            bool skipFrame = false; // no slot was free: this frame isn't timed
            std::mutex mutex;       // results, handed over by the readback callbacks
            std::vector<Result> results;
        };

        State& S()
        {
            static State state;
            return state;
        }

        // Runs when a slot's readback lands (natively while the instance processes events).
        void Read(const std::shared_ptr<Slot>& slot)
        {
            Result result;
            result.frame = slot->frame;
            const auto* stamps = static_cast<const uint64_t*>(slot->readback.GetConstMappedRange(0, slot->used * sizeof(uint64_t)));
            if (stamps)
            {
                uint64_t first = ~uint64_t(0);
                for (uint32_t i = 0; i < slot->used; i += 2)
                    if (stamps[i] != 0)
                        first = std::min(first, stamps[i]);
                for (size_t pass = 0; pass < slot->labels.size(); ++pass)
                {
                    const uint64_t begin = stamps[pass * 2], end = stamps[pass * 2 + 1];
                    Profiler::GpuPass gpu;
                    gpu.name = slot->labels[pass];
                    gpu.start = begin >= first && first != ~uint64_t(0) ? double(begin - first) * 1e-6 : 0.0;
                    gpu.ms = end > begin ? double(end - begin) * 1e-6 : 0.0;
                    result.passes.push_back(std::move(gpu));
                }
            }
            slot->readback.Unmap();
            slot->state.store(kFree);
            State& s = S();
            std::lock_guard lock(s.mutex);
            s.results.push_back(std::move(result));
        }
    }

    void Setup(bool timestamps, const std::string& why)
    {
        State& s = S();
        s.enabled = timestamps;
        if (!timestamps)
        {
            Profiler::SetGpuStatus(false, why);
            return;
        }
        for (std::shared_ptr<Slot>& slot : s.slots)
        {
            slot = std::make_shared<Slot>();
            wgpu::QuerySetDescriptor set{ .type = wgpu::QueryType::Timestamp, .count = kQueries };
            slot->set = device.CreateQuerySet(&set);
            wgpu::BufferDescriptor resolve{ .usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc, .size = kQueries * sizeof(uint64_t) };
            slot->resolve = device.CreateBuffer(&resolve);
            wgpu::BufferDescriptor readback{ .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst, .size = kQueries * sizeof(uint64_t) };
            slot->readback = device.CreateBuffer(&readback);
        }
        Profiler::SetGpuStatus(true, why);
    }

    bool Enabled()
    {
        return S().enabled;
    }

    bool Pass(const std::string& label, wgpu::PassTimestampWrites& writes)
    {
        State& s = S();
        if (!s.enabled || s.skipFrame)
            return false;
        if (!s.current)
        {
            for (const std::shared_ptr<Slot>& slot : s.slots)
            {
                int free = kFree;
                if (slot->state.compare_exchange_strong(free, kRecording))
                {
                    s.current = slot.get();
                    break;
                }
            }
            if (!s.current)
            {
                s.skipFrame = true;
                return false;
            }
            s.current->used = 0;
            s.current->labels.clear();
        }
        Slot& slot = *s.current;
        if (slot.used + 2 > kQueries)
            return false;
        writes.querySet = slot.set;
        writes.beginningOfPassWriteIndex = slot.used;
        writes.endOfPassWriteIndex = slot.used + 1;
        slot.used += 2;
        slot.labels.push_back(label);
        return true;
    }

    void EndFrame(uint64_t frame)
    {
        State& s = S();
        std::vector<Result> results;
        {
            std::lock_guard lock(s.mutex);
            results.swap(s.results);
        }
        for (Result& result : results)
            Profiler::SetGpuPasses(result.frame, std::move(result.passes));

        s.skipFrame = false;
        Slot* current = s.current;
        s.current = nullptr;
        if (!current)
            return;
        if (current->used == 0)
        {
            current->state.store(kFree);
            return;
        }

        current->frame = frame;
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.ResolveQuerySet(current->set, 0, current->used, current->resolve, 0);
        encoder.CopyBufferToBuffer(current->resolve, 0, current->readback, 0, current->used * sizeof(uint64_t));
        wgpu::CommandBuffer commands = encoder.Finish();
        device.GetQueue().Submit(1, &commands);
        current->state.store(kReading);

        std::shared_ptr<Slot> keepAlive;
        for (const std::shared_ptr<Slot>& slot : s.slots)
            if (slot.get() == current)
                keepAlive = slot;
        (void)current->readback.MapAsync(wgpu::MapMode::Read, 0, current->used * sizeof(uint64_t), wgpu::CallbackMode::AllowSpontaneous,
                                         [keepAlive](wgpu::MapAsyncStatus status, wgpu::StringView) {
                                             if (status == wgpu::MapAsyncStatus::Success)
                                                 Read(keepAlive);
                                             else
                                                 keepAlive->state.store(kFree);
                                         });
    }
}
