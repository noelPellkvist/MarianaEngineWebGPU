#include <StandardPBR.hpp>

StandardPBRPipeline::StandardPBRPipeline()
    : uboLayout(false, ubo, ubo.lightDir, ubo.lightVP),
      transformLayout(131072, transformData,
          transformData.modelMatrix,
          transformData.normalMatrix,
          transformData.entityID),
      materialsLayout(131072, materialData,
          materialData.baseColor,
          materialData.metallicFactor,
          materialData.roughnessFactor,
          materialData.normalMapStrength,
          materialData.occlusionStrength,
          materialData.emissiveFactor,
          materialData.alphaCutoff),
      camLayout(false, cameraInfo,
          cameraInfo.proj,
          cameraInfo.view,
          cameraInfo.viewProj,
          cameraInfo.invView,
          cameraInfo.invProj,
          cameraInfo.invViewProj,
          cameraInfo.pos,
          cameraInfo.exposure),
      boneLayout(131072, boneData, boneData.model, boneData.normal),
      drawLayout(131072, drawData, drawData.transformIndex, drawData.materialIndex),
      vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1,
          v.color0, v.boneIndices, v.boneWeights},
      uboBuffer(uboLayout),
      transformBuffer(transformLayout),
      materialsBuffer(materialsLayout),
      cameraBuffer(camLayout),
      boneBuffer(boneLayout),
      drawBuffer(drawLayout)
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
}