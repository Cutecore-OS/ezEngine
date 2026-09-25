#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorPluginAssets/BlendShapeAsset/BlendShapeImport.h>
#include <ModelImporter2/ModelImporter.h>
#include <assimp/scene.h>

ezStatus ezApplyBlendShapeGltfDefaults(ezStringView sFile, ezBlendShapeResourceDescriptor& inout_desc);

namespace
{
  ezVec3 ToVec(const aiVector3D& v)
  {
    return ezVec3(v.x, v.y, v.z);
  }
} // namespace

ezStatus ezImportBlendShapeMesh(const ezModelImporter2::ImportOptions& options, ezBlendShapeResourceDescriptor& out_desc)
{
  out_desc = ezBlendShapeResourceDescriptor();
  auto importer = ezModelImporter2::RequestImporterForFileType(options.m_sSourceFile);
  if (importer == nullptr)
    return ezStatus("No importer for the Animated Mesh source.");
  auto opt = options;
  opt.m_pMeshOutput = &out_desc.m_Mesh;
  opt.m_bHighPrecision = true;
  ezHashTable<ezHashedString, ezUInt32> names;
  opt.m_AssimpMeshImported = [&](const aiMesh& source, ezStringView node, const ezMat4& transform, ezUInt32 offset) -> ezResult
  {
    ezMat3 normalMatrix = transform.GetRotationalPart();
    if (normalMatrix.Invert().Failed())
      return EZ_FAILURE;
    normalMatrix.Transpose();
    for (ezUInt32 t = 0; t < source.mNumAnimMeshes; ++t)
    {
      const auto& target = *source.mAnimMeshes[t];
      if (target.mNumVertices != source.mNumVertices)
        return EZ_FAILURE;
      ezStringBuilder name(node, "/");
      if (target.mName.length > 0)
        name.Append(target.mName.C_Str());
      else
        name.AppendFormat("Target{}", t);
      ezHashedString hash;
      hash.Assign(name);
      ezUInt32 index = 0;
      if (!names.TryGetValue(hash, index))
      {
        index = out_desc.m_Targets.GetCount();
        names.Insert(hash, index);
        auto& shape = out_desc.m_Targets.ExpandAndGetRef();
        shape.m_sName = hash;
        shape.m_fDefaultWeight = target.mWeight;
      }
      auto& shape = out_desc.m_Targets[index];
      for (ezUInt32 v = 0; v < source.mNumVertices; ++v)
      {
        ezBlendShapeDelta delta;
        delta.m_uiVertex = offset + v;
        if (target.HasPositions())
          delta.m_vPosition = transform.TransformDirection(ToVec(target.mVertices[v] - source.mVertices[v]));
        if (target.HasNormals() && source.HasNormals())
        {
          const float length = (normalMatrix * ToVec(source.mNormals[v])).GetLength();
          if (length > 0.000001f)
            delta.m_vNormal = normalMatrix * ToVec(target.mNormals[v] - source.mNormals[v]) / length;
        }
        if (target.HasTangentsAndBitangents() && source.HasTangentsAndBitangents())
        {
          const float length = (normalMatrix * ToVec(source.mTangents[v])).GetLength();
          if (length > 0.000001f)
            delta.m_vTangent = normalMatrix * ToVec(target.mTangents[v] - source.mTangents[v]) / length;
        }
        if (!delta.m_vPosition.IsZero() || !delta.m_vNormal.IsZero() || !delta.m_vTangent.IsZero())
          shape.m_Deltas.PushBack(delta);
      }
    }
    return EZ_SUCCESS;
  };
  opt.m_VertexRemapped = [&](ezArrayPtr<const ezUInt32> remap)
  {
    for (auto& target : out_desc.m_Targets)
    {
      for (auto& delta : target.m_Deltas)
        delta.m_uiVertex = remap[delta.m_uiVertex];
      target.m_Deltas.Sort([](const auto& a, const auto& b)
        { return a.m_uiVertex < b.m_uiVertex; });
    }
  };
  if (importer->Import(opt).Failed())
    return ezStatus("Could not import blend shapes using the Animated Mesh settings.");
  if (options.m_sSourceFile.HasExtension("glb") || options.m_sSourceFile.HasExtension("gltf"))
    EZ_SUCCEED_OR_RETURN(ezApplyBlendShapeGltfDefaults(options.m_sSourceFile, out_desc));
  const auto& buffer = out_desc.m_Mesh.MeshBufferDesc();
  ezDynamicArray<ezVec3> displacement;
  displacement.SetCount(buffer.GetVertexCount(), ezVec3::MakeZero());
  for (const auto& target : out_desc.m_Targets)
    for (const auto& delta : target.m_Deltas)
      displacement[delta.m_uiVertex] += delta.m_vPosition.Abs();
  auto bounds = ezBoundingBox::MakeInvalid();
  ezDynamicArray<ezMat4> inverseBind;
  inverseBind.SetCount(out_desc.m_Mesh.m_Bones.GetCount(), ezMat4::MakeIdentity());
  for (auto bone : out_desc.m_Mesh.m_Bones)
    inverseBind[bone.Value().m_uiBoneIndex] = bone.Value().m_GlobalInverseRestPoseMatrix;
  for (ezUInt32 v = 0; v < buffer.GetVertexCount(); ++v)
  {
    bounds.ExpandToInclude(buffer.GetPosition(v) - displacement[v]);
    bounds.ExpandToInclude(buffer.GetPosition(v) + displacement[v]);
    if (!inverseBind.IsEmpty())
    {
      auto ids = buffer.GetBoneIndices(v);
      auto weights = buffer.GetBoneWeights(v);
      for (ezUInt32 i = 0; i < 4; ++i)
      {
        if (weights.GetData()[i] <= 0)
          continue;
        const auto& bind = inverseBind[ids.GetData()[i]];
        const auto matrix = bind.GetRotationalPart();
        const float scale = ezMath::Sqrt(matrix.GetColumn(0).GetLengthSquared() + matrix.GetColumn(1).GetLengthSquared() + matrix.GetColumn(2).GetLengthSquared());
        out_desc.m_Mesh.m_fMaxBoneVertexOffset = ezMath::Max(out_desc.m_Mesh.m_fMaxBoneVertexOffset,
          bind.TransformPosition(buffer.GetPosition(v)).GetLength() + scale * displacement[v].GetLength());
      }
    }
  }
  out_desc.m_Mesh.SetBounds(ezBoundingBoxSphere::MakeFromBox(bounds));
  return ezStatus(out_desc.Validate());
}

ezStatus ezImportBlendShapeMesh(ezStringView file, bool shapes, ezBlendShapeResourceDescriptor& out_desc)
{
  ezModelImporter2::ImportOptions options;
  options.m_sSourceFile = file;
  options.m_bRecomputeTangents = true;
  EZ_SUCCEED_OR_RETURN(ezImportBlendShapeMesh(options, out_desc));
  if (!shapes)
    out_desc.m_Targets.Clear();
  return ezStatus(EZ_SUCCESS);
}
