#include <EditorPluginAssets/EditorPluginAssetsPCH.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeImport.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace
{
  struct GltfData
  {
    QJsonObject m_Root;
    QByteArray m_Bin;
    QString m_Directory;
    QList<QByteArray> m_Buffers;
    ezStatus Read(ezStringView sFile)
    {
      ezStringBuilder path(sFile);
      QFile file(QString::fromUtf8(path.GetData()));
      if (!file.open(QIODevice::ReadOnly))
        return ezStatus("Cannot open glTF file.");
      if (file.size() > 1024ll * 1024 * 1024)
        return ezStatus("glTF file exceeds 1 GiB import limit.");
      QByteArray bytes = file.readAll();
      m_Directory = QFileInfo(file).absolutePath();
      QByteArray json = bytes;
      if (bytes.size() >= 12 && qFromLittleEndian<quint32>(bytes.constData()) == 0x46546c67)
      {
        if (qFromLittleEndian<quint32>(bytes.constData() + 4) != 2 || qFromLittleEndian<quint32>(bytes.constData() + 8) != bytes.size())
          return ezStatus("Invalid GLB header.");
        json.clear();
        for (qsizetype offset = 12; offset + 8 <= bytes.size();)
        {
          const quint32 size = qFromLittleEndian<quint32>(bytes.constData() + offset);
          const quint32 type = qFromLittleEndian<quint32>(bytes.constData() + offset + 4);
          offset += 8;
          if (size > bytes.size() - offset)
            return ezStatus("Truncated GLB chunk.");
          if (type == 0x4e4f534a)
            json = bytes.mid(offset, size);
          if (type == 0x004e4942)
            m_Bin = bytes.mid(offset, size);
          offset += size;
        }
      }
      QJsonParseError error;
      const auto doc = QJsonDocument::fromJson(json, &error);
      if (error.error != QJsonParseError::NoError || !doc.isObject())
        return ezStatus("Invalid glTF JSON.");
      m_Root = doc.object();
      return ezStatus(EZ_SUCCESS);
    }
    ezStatus LoadBuffers()
    {
      const auto buffers = m_Root["buffers"].toArray();
      for (const auto value : buffers)
      {
        const auto object = value.toObject();
        const QString uri = object["uri"].toString();
        QByteArray bytes;
        if (uri.isEmpty())
          bytes = m_Bin;
        else if (uri.startsWith("data:"))
        {
          const int comma = uri.indexOf(',');
          if (comma < 0 || !uri.left(comma).endsWith(";base64"))
            return ezStatus("Unsupported data URI.");
          bytes = QByteArray::fromBase64(uri.mid(comma + 1).toLatin1());
        }
        else
        {
          const QUrl url(uri);
          if (!url.isRelative())
            return ezStatus("glTF buffers must be local relative files.");
          QFile file(QDir(m_Directory).filePath(QUrl::fromPercentEncoding(uri.toUtf8())));
          if (!file.open(QIODevice::ReadOnly) || file.size() > 1024ll * 1024 * 1024)
            return ezStatus("Cannot read glTF buffer.");
          bytes = file.readAll();
        }
        if (bytes.size() < object["byteLength"].toInteger())
          return ezStatus("Truncated glTF buffer.");
        m_Buffers.push_back(std::move(bytes));
      }
      return ezStatus(EZ_SUCCESS);
    }
    ezStatus ReadElements(int iView, qint64 iOffset, qint64 iCount, int iBytes, bool bStrided, QList<quint32>& out_values) const
    {
      const auto views = m_Root["bufferViews"].toArray();
      if (iView < 0 || iView >= views.size() || iOffset < 0 || iCount < 0 || iCount > 16000000)
        return ezStatus("Invalid glTF buffer view.");
      const auto view = views[iView].toObject();
      const int buffer = view["buffer"].toInt(-1);
      const qint64 start = view["byteOffset"].toInteger(0);
      const qint64 length = view["byteLength"].toInteger(-1);
      const qint64 stride = bStrided ? view["byteStride"].toInteger(iBytes) : iBytes;
      if (buffer < 0 || buffer >= m_Buffers.size() || start < 0 || length < 0 || start > m_Buffers[buffer].size() ||
          length > m_Buffers[buffer].size() - start || stride < iBytes || stride > 252 ||
          iOffset > length || (iCount > 0 && (iCount - 1) * stride + iBytes > length - iOffset))
        return ezStatus("glTF accessor exceeds its buffer view.");
      out_values.resize(iCount);
      for (qint64 i = 0; i < iCount; ++i)
      {
        const char* p = m_Buffers[buffer].constData() + start + iOffset + i * stride;
        out_values[i] = iBytes == 1 ? static_cast<unsigned char>(*p) : iBytes == 2 ? qFromLittleEndian<quint16>(p)
                                                                                   : qFromLittleEndian<quint32>(p);
      }
      return ezStatus(EZ_SUCCESS);
    }
    ezStatus ReadScalars(int iAccessor, QList<float>& out_values) const
    {
      const auto accessors = m_Root["accessors"].toArray();
      if (iAccessor < 0 || iAccessor >= accessors.size())
        return ezStatus("Invalid glTF accessor index.");
      const auto accessor = accessors[iAccessor].toObject();
      const qint64 count = accessor["count"].toInteger(-1);
      if (accessor["type"].toString() != "SCALAR" || accessor["componentType"].toInt() != 5126 || count < 0 || count > 16000000)
        return ezStatus("Morph animation requires scalar float accessors.");
      QList<quint32> raw;
      raw.fill(0, count);
      if (accessor.contains("bufferView"))
        EZ_SUCCEED_OR_RETURN(ReadElements(accessor["bufferView"].toInt(-1), accessor["byteOffset"].toInteger(0), count, 4, true, raw));
      if (accessor.contains("sparse"))
      {
        const auto sparse = accessor["sparse"].toObject();
        const qint64 n = sparse["count"].toInteger(-1);
        const auto indices = sparse["indices"].toObject();
        const auto values = sparse["values"].toObject();
        const int component = indices["componentType"].toInt();
        const int size = component == 5121 ? 1 : component == 5123 ? 2
                                               : component == 5125 ? 4
                                                                   : 0;
        if (size == 0 || n < 0 || n > count)
          return ezStatus("Invalid sparse accessor.");
        QList<quint32> dst, src;
        EZ_SUCCEED_OR_RETURN(ReadElements(indices["bufferView"].toInt(-1), indices["byteOffset"].toInteger(0), n, size, false, dst));
        EZ_SUCCEED_OR_RETURN(ReadElements(values["bufferView"].toInt(-1), values["byteOffset"].toInteger(0), n, 4, false, src));
        for (qint64 i = 0; i < n; ++i)
        {
          if (dst[i] >= count || (i > 0 && dst[i] <= dst[i - 1]))
            return ezStatus("Invalid sparse index.");
          raw[dst[i]] = src[i];
        }
      }
      out_values.resize(count);
      for (qint64 i = 0; i < count; ++i)
      {
        std::memcpy(&out_values[i], &raw[i], 4);
        if (!std::isfinite(out_values[i]))
          return ezStatus("Non-finite glTF animation value.");
      }
      return ezStatus(EZ_SUCCESS);
    }
  };
} // namespace

ezStatus ezReadBlendShapeGltfCurves(ezStringView sFile, ezStringView sAnimation, ezAnimationClipResourceDescriptor& inout_desc)
{
  GltfData gltf;
  EZ_SUCCEED_OR_RETURN(gltf.Read(sFile));
  EZ_SUCCEED_OR_RETURN(gltf.LoadBuffers());
  const auto animations = gltf.m_Root["animations"].toArray();
  QJsonObject animation;
  for (int i = 0; i < animations.size(); ++i)
  {
    const auto a = animations[i].toObject();
    const QByteArray name = a["name"].toString(QString("animations[%1]").arg(i)).toUtf8();
    if (sAnimation.IsEmpty() || sAnimation == name.constData())
    {
      animation = a;
      break;
    }
  }
  if (animation.isEmpty())
    return ezStatus("glTF animation not found.");
  const auto samplers = animation["samplers"].toArray();
  const auto nodes = gltf.m_Root["nodes"].toArray();
  const auto meshes = gltf.m_Root["meshes"].toArray();
  ezSet<ezString> usedNames;
  for (const auto c : animation["channels"].toArray())
  {
    const auto channel = c.toObject();
    const auto target = channel["target"].toObject();
    if (target["path"].toString() != "weights")
      continue;
    const int nodeIndex = target["node"].toInt(-1), samplerIndex = channel["sampler"].toInt(-1);
    if (nodeIndex < 0 || nodeIndex >= nodes.size() || samplerIndex < 0 || samplerIndex >= samplers.size())
      return ezStatus("Invalid morph channel.");
    const auto node = nodes[nodeIndex].toObject();
    const int meshIndex = node["mesh"].toInt(-1);
    if (meshIndex < 0 || meshIndex >= meshes.size())
      return ezStatus("Morph channel targets a node without a mesh.");
    const auto mesh = meshes[meshIndex].toObject();
    const auto primitives = mesh["primitives"].toArray();
    if (primitives.isEmpty())
      return ezStatus("Morph mesh has no primitives.");
    const int count = primitives[0].toObject()["targets"].toArray().size();
    if (count <= 0 || count > 4096)
      return ezStatus("Invalid morph target count.");
    for (const auto primitive : primitives)
      if (primitive.toObject()["targets"].toArray().size() != count)
        return ezStatus("Mesh primitives have different morph target counts.");
    const auto names = mesh["extras"].toObject()["targetNames"].toArray();
    const auto sampler = samplers[samplerIndex].toObject();
    const QString interpolation = sampler["interpolation"].toString("LINEAR");
    if (interpolation != "LINEAR" && interpolation != "STEP" && interpolation != "CUBICSPLINE")
      return ezStatus("Unknown morph interpolation.");
    QList<float> times, values;
    EZ_SUCCEED_OR_RETURN(gltf.ReadScalars(sampler["input"].toInt(-1), times));
    EZ_SUCCEED_OR_RETURN(gltf.ReadScalars(sampler["output"].toInt(-1), values));
    const bool cubic = interpolation == "CUBICSPLINE";
    if (times.isEmpty() || times.size() > 30000 || values.size() != times.size() * count * (cubic ? 3 : 1))
      return ezStatus("Invalid morph sampler size.");
    for (int k = 0; k < times.size(); ++k)
      if (times[k] < 0 || (k > 0 && times[k] <= times[k - 1]))
        return ezStatus("Morph key times must increase strictly.");
    const QString scope = node["name"].toString(QString("nodes[%1]").arg(nodeIndex));
    for (int t = 0; t < count; ++t)
    {
      const QString shape = t < names.size() ? names[t].toString() : QString("Target%1").arg(t);
      const QByteArray utf8 = ("BlendShape/" + scope + "/" + shape).toUtf8();
      if (usedNames.Contains(utf8.constData()))
        return ezStatus("Ambiguous morph names. Give mesh nodes and targets unique names.");
      usedNames.Insert(utf8.constData());
      auto& curve = inout_desc.m_CustomCurves.ExpandAndGetRef();
      curve.m_sName.Assign(utf8.constData());
      for (int k = 0; k < times.size(); ++k)
      {
        const int offset = k * count * (cubic ? 3 : 1);
        auto& cp = curve.m_Curve.AddControlPoint(times[k]);
        cp.m_Position.y = values[offset + (cubic ? count : 0) + t];
        cp.m_TangentModeLeft = cubic ? ezCurveTangentMode::Bezier : ezCurveTangentMode::Linear;
        cp.m_TangentModeRight = cp.m_TangentModeLeft;
        if (cubic)
        {
          const float left = k > 0 ? (times[k] - times[k - 1]) / 3.0f : 0;
          const float right = k + 1 < times.size() ? (times[k + 1] - times[k]) / 3.0f : 0;
          cp.m_LeftTangent.Set(-left, -left * values[offset + t]);
          cp.m_RightTangent.Set(right, right * values[offset + 2 * count + t]);
        }
        if (interpolation == "STEP" && k + 1 < times.size())
        {
          // ezCurve1D has no constant segments. Keep the transition within four float ULPs of the next key.
          float next = times[k + 1];
          for (int i = 0; i < 4; ++i)
            next = std::nextafter(next, times[k]);
          auto& hold = curve.m_Curve.AddControlPoint(next);
          hold.m_Position.y = values[offset + t];
          hold.m_TangentModeLeft = ezCurveTangentMode::Linear;
          hold.m_TangentModeRight = ezCurveTangentMode::Linear;
        }
      }
      curve.m_Curve.SortControlPoints();
      curve.m_Curve.CreateLinearApproximation();
    }
  }
  return ezStatus(EZ_SUCCESS);
}

void ezAddBlendShapeSourceDependencies(ezStringView sSource, ezSet<ezString>& inout_dependencies)
{
  if (sSource.IsEmpty())
    return;
  ezStringBuilder source(sSource);
  if (!ezQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(source))
    return;
  GltfData gltf;
  if (gltf.Read(source).Failed())
    return;
  for (const auto value : gltf.m_Root["buffers"].toArray())
  {
    const QString uri = value.toObject()["uri"].toString();
    if (uri.isEmpty() || uri.startsWith("data:"))
      continue;
    const auto full = QDir(gltf.m_Directory).filePath(QUrl::fromPercentEncoding(uri.toUtf8())).toUtf8();
    inout_dependencies.Insert(full.constData());
  }
}

ezStatus ezApplyBlendShapeGltfDefaults(ezStringView sFile, ezBlendShapeResourceDescriptor& inout_desc)
{
  GltfData gltf;
  EZ_SUCCEED_OR_RETURN(gltf.Read(sFile));
  const auto nodes = gltf.m_Root["nodes"].toArray();
  const auto meshes = gltf.m_Root["meshes"].toArray();
  ezSet<ezString> usedNames;
  for (int n = 0; n < nodes.size(); ++n)
  {
    const auto node = nodes[n].toObject();
    const int m = node["mesh"].toInt(-1);
    if (m < 0 || m >= meshes.size())
      continue;
    const auto mesh = meshes[m].toObject();
    const auto primitives = mesh["primitives"].toArray();
    if (primitives.isEmpty())
      continue;
    const int count = primitives[0].toObject()["targets"].toArray().size();
    const auto names = mesh["extras"].toObject()["targetNames"].toArray();
    const auto weights = node.contains("weights") ? node["weights"].toArray() : mesh["weights"].toArray();
    if (!weights.isEmpty() && weights.size() != count)
      return ezStatus("Default weight count differs from the target count.");
    const QString scope = node["name"].toString(QString("nodes[%1]").arg(n));
    for (int t = 0; t < count; ++t)
    {
      const QString target = t < names.size() ? names[t].toString() : QString("Target%1").arg(t);
      if (target.isEmpty())
        return ezStatus("Morph target names must be non-empty.");
      const QByteArray name = (scope + "/" + target).toUtf8();
      if (usedNames.Contains(name.constData()))
        return ezStatus("Morph mesh nodes and target names must be unambiguous.");
      usedNames.Insert(name.constData());
      for (auto& shape : inout_desc.m_Targets)
        if (shape.m_sName.GetView() == name.constData())
          shape.m_fDefaultWeight = t < weights.size() ? static_cast<float>(weights[t].toDouble()) : 0;
    }
  }
  return ezStatus(EZ_SUCCESS);
}

// Assimp discards the source interpolation mode on skeletal keys. Validate the JSON before importing them.
ezStatus ezValidateBlendShapeGltfBoneChannels(ezStringView sFile, ezStringView sAnimation)
{
  GltfData gltf;
  EZ_SUCCEED_OR_RETURN(gltf.Read(sFile));
  const auto animations = gltf.m_Root["animations"].toArray();
  for (int i = 0; i < animations.size(); ++i)
  {
    const auto animation = animations[i].toObject();
    const auto name = animation["name"].toString(QString("animations[%1]").arg(i)).toUtf8();
    if (!sAnimation.IsEmpty() && sAnimation != name.constData())
      continue;
    const auto samplers = animation["samplers"].toArray();
    for (const auto value : animation["channels"].toArray())
    {
      const auto channel = value.toObject();
      if (channel["target"].toObject()["path"].toString() == "weights")
        continue;
      const int sampler = channel["sampler"].toInt(-1);
      if (sampler < 0 || sampler >= samplers.size())
        return ezStatus("Invalid skeletal sampler.");
      if (samplers[sampler].toObject()["interpolation"].toString("LINEAR") != "LINEAR")
        return ezStatus("Non-linear skeletal source channels require baking to linear keys before import.");
    }
    return ezStatus(EZ_SUCCESS);
  }
  return ezStatus("glTF animation not found.");
}


ezStatus ezReadBlendShapeGltfNames(ezStringView sFile, ezDynamicArray<ezBlendShapeTarget>& out_targets, const ezModelImporter2::ImportOptions* pOptions)
{
  GltfData gltf;
  EZ_SUCCEED_OR_RETURN(gltf.Read(sFile));
  const auto nodes = gltf.m_Root["nodes"].toArray();
  const auto meshes = gltf.m_Root["meshes"].toArray();
  out_targets.Clear();
  ezHashSet<int> activeNodes;
  const auto scenes = gltf.m_Root["scenes"].toArray();
  const int sceneIndex = gltf.m_Root["scene"].toInt(0);
  auto visit = [&](int index, auto&& recurse) -> void
  {
    if (index < 0 || index >= nodes.size() || activeNodes.Contains(index))
      return;
    activeNodes.Insert(index);
    for (const auto child : nodes[index].toObject()["children"].toArray())
      recurse(child.toInt(-1), recurse);
  };
  if (sceneIndex >= 0 && sceneIndex < scenes.size())
    for (const auto root : scenes[sceneIndex].toObject()["nodes"].toArray())
      visit(root.toInt(-1), visit);
  ezHashSet<ezHashedString> names;
  for (int n = 0; n < nodes.size(); ++n)
  {
    if (!scenes.isEmpty() && !activeNodes.Contains(n))
      continue;
    const auto node = nodes[n].toObject();
    if (pOptions && pOptions->m_bImportSkinningData && !node.contains("skin"))
      continue;
    const int m = node["mesh"].toInt(-1);
    if (m < 0 || m >= meshes.size())
      continue;
    const auto mesh = meshes[m].toObject();
    if (pOptions)
    {
      const QByteArray meshName = mesh["name"].toString(QString("meshes[%1]").arg(m)).toUtf8();
      auto matches = [&](const ezDynamicArray<ezString>& tags)
      {
        for (const auto& tag : tags)
          if (ezStringUtils::StartsWith_NoCase(meshName.constData(), tag) || ezStringUtils::EndsWith_NoCase(meshName.constData(), tag))
            return true;
        return false;
      };
      if (!pOptions->m_MeshIncludeTags.IsEmpty())
      {
        if (!matches(pOptions->m_MeshIncludeTags))
          continue;
      }
      else if (matches(pOptions->m_MeshExcludeTags))
        continue;
    }
    const auto primitives = mesh["primitives"].toArray();
    if (primitives.isEmpty())
      continue;
    const int count = primitives[0].toObject()["targets"].toArray().size();
    const auto targetNames = mesh["extras"].toObject()["targetNames"].toArray();
    const auto weights = node.contains("weights") ? node["weights"].toArray() : mesh["weights"].toArray();
    const QString scope = node["name"].toString(QString("nodes[%1]").arg(n));
    for (int i = 0; i < count; ++i)
    {
      auto& target = out_targets.ExpandAndGetRef();
      const QString name = i < targetNames.size() ? targetNames[i].toString() : QString("Target%1").arg(i);
      target.m_sName.Assign((scope + "/" + name).toUtf8().constData());
      if (names.Contains(target.m_sName))
        return ezStatus("Mesh node and shape names must be unambiguous.");
      names.Insert(target.m_sName);
      target.m_fDefaultWeight = i < weights.size() ? static_cast<float>(weights[i].toDouble()) : 0;
    }
  }
  return ezStatus(EZ_SUCCESS);
}
