#include <MaterialAsset.hpp>
#include <Logger.hpp>

namespace
{
    struct SlotAllocator
    {
        std::vector<uint32_t> freeSlots;
        uint32_t nextSlot = 0;
    };

    SlotAllocator& MaterialSlots()
    {
        static SlotAllocator slots;
        return slots;
    }
}

MaterialAsset::MaterialAsset()
{
    SlotAllocator& slots = MaterialSlots();
    if (!slots.freeSlots.empty())
    {
        m_GpuSlot = slots.freeSlots.back();
        slots.freeSlots.pop_back();
    }
    else if (slots.nextSlot < kMaxMaterials)
    {
        m_GpuSlot = slots.nextSlot++;
    }
    else
    {
        Logger::Error("Out of GPU material slots; the material will render with the default material");
    }
}

MaterialAsset::~MaterialAsset()
{
    if (m_GpuSlot != kInvalidSlot)
        MaterialSlots().freeSlots.push_back(m_GpuSlot);
}
