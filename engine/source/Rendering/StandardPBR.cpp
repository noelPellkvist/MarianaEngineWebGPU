#include <Mariana/Rendering/StandardPBR.hpp>
#include <algorithm>
#include <cstring>

StandardPBRPipeline::StandardPBRPipeline()
    : uboLayout(false, ubo, ubo.lightDir, ubo.cascadeVP, ubo.cascadeSplits, ubo.cascadeTexel,
          ubo.sunColor, ubo.skySH, ubo.skyParams),
      transformLayout(kMaxTransforms, transformData,
          transformData.modelMatrix,
          transformData.normalMatrix,
          transformData.entityID),
      materialsLayout(MaterialAsset::kMaxMaterials, materialData,
          materialData.baseColor,
          materialData.metallicFactor,
          materialData.roughnessFactor,
          materialData.normalMapStrength,
          materialData.occlusionStrength,
          materialData.emissiveFactor,
          materialData.alphaCutoff,
          materialData.uvSets),
      camLayout(false, cameraInfo,
          cameraInfo.proj,
          cameraInfo.view,
          cameraInfo.viewProj,
          cameraInfo.invView,
          cameraInfo.invProj,
          cameraInfo.invViewProj,
          cameraInfo.pos,
          cameraInfo.exposure),
      boneLayout(kMaxBones, boneData, boneData.model),
      drawLayout(kMaxDraws, drawData, drawData.transformIndex, drawData.materialIndex, drawData.boneOffset),
      cascadeLayout(true, cascadeData, cascadeData.viewProj),
      vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1,
          v.color0, v.boneIndices, v.boneWeights},
      uboBuffer(uboLayout),
      transformBuffer(transformLayout),
      materialsBuffer(materialsLayout),
      cameraBuffer(camLayout),
      boneBuffer(boneLayout),
      drawBuffer(drawLayout),
      shadowCascadeBuffer(cascadeLayout)
{
    BuildBuffers();
}

StandardPBRPipeline::~StandardPBRPipeline()
{
}

void StandardPBRPipeline::BuildBuffers()
{
    uboBuffer.Build();
    transformBuffer.Build();
    materialsBuffer.Build();
    cameraBuffer.Build();
    boneBuffer.Build();
    drawBuffer.Build();
    shadowCascadeBuffer.Build();
}
void StandardPBRPipeline::UploadDirtyMaterials(AssetManager2& assets)
{
    assets.ForEach<MaterialAsset>([this](AssetID, MaterialAsset& material) {
        if (!material.IsDirty() || material.GetGpuSlot() == MaterialAsset::kInvalidSlot)
            return;

        materialsBuffer.Write(material.properties, material.GetGpuSlot());
        material.ClearDirty();
    });
}

void StandardPBRPipeline::QueueTransform(uint32_t index, const TransformData& data)
{
    if (index >= kMaxTransforms)
        return;

    const std::vector<std::byte>& packed = transformLayout.PackElement(data);
    const size_t offset = static_cast<size_t>(index) * packed.size();
    if (transformStaging.size() < offset + packed.size())
        transformStaging.resize(offset + packed.size());

    std::memcpy(transformStaging.data() + offset, packed.data(), packed.size());
    dirtyTransforms.push_back(index);
}

void StandardPBRPipeline::FlushTransforms()
{
    if (dirtyTransforms.empty())
        return;

    // One write per run of nearby slots: re-sending a few unchanged slots inside a small gap is
    // cheaper than starting another write.
    constexpr uint32_t kMaxGap = 16;
    std::sort(dirtyTransforms.begin(), dirtyTransforms.end());
    const size_t stride = transformLayout.GetElementStride();

    size_t runStart = 0;
    for (size_t i = 1; i <= dirtyTransforms.size(); ++i)
    {
        if (i < dirtyTransforms.size() && dirtyTransforms[i] - dirtyTransforms[i - 1] <= kMaxGap)
            continue;

        const size_t first = dirtyTransforms[runStart];
        const size_t last = dirtyTransforms[i - 1];
        transformBuffer.WriteRawBytes(transformStaging.data() + first * stride, (last - first + 1) * stride, first * stride);
        runStart = i;
    }
    dirtyTransforms.clear();
}
