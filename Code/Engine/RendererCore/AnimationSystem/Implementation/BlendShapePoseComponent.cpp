#include <RendererCore/RendererCorePCH.h>

#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/AnimationSystem/BlendShapePoseComponent.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezBlendShapePoseComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_ACCESSOR_PROPERTY("BlendShapes", GetBlendShapes, SetBlendShapes)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_BlendShapes")),
    EZ_MAP_ACCESSOR_PROPERTY("Weights", GetWeights, GetWeight, SetWeight, RemoveWeight)->AddAttributes(new ezExposedParametersAttribute("BlendShapes"), new ezContainerAttribute(false, true, false)),
  }
  EZ_END_PROPERTIES;

  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Animation"),
  }
  EZ_END_ATTRIBUTES;

  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(SetBlendShapeWeight, In, "Shape", In, "Weight"),
    EZ_SCRIPT_FUNCTION_PROPERTY(GetBlendShapeWeight, In, "Shape"),
    EZ_SCRIPT_FUNCTION_PROPERTY(ResetWeights),
  }
  EZ_END_FUNCTIONS;

  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgAnimationCurveValue, OnMsgAnimationCurveValue),
    EZ_MESSAGE_HANDLER(ezMsgSetBlendShapeWeight, OnMsgSetBlendShapeWeight),
  }
  EZ_END_MESSAGEHANDLERS;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezBlendShapePoseComponent::ezBlendShapePoseComponent() = default;
ezBlendShapePoseComponent::~ezBlendShapePoseComponent() = default;

void ezBlendShapePoseComponent::Update()
{
  if (m_uiResendWeights == 0)
    return;

  if (--m_uiResendWeights > 0)
  {
    static_cast<ezBlendShapePoseComponentManager*>(GetOwningManager())->EnqueueUpdate(GetHandle());
  }

  SendWeights();
}

void ezBlendShapePoseComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  s << m_hBlendShapes;

  m_Weights.Sort();
  ezUInt16 numWeights = static_cast<ezUInt16>(m_Weights.GetCount());
  s << numWeights;

  for (ezUInt16 i = 0; i < numWeights; ++i)
  {
    s << m_Weights.GetKey(i);
    s << m_Weights.GetValue(i);
  }
}

void ezBlendShapePoseComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  s >> m_hBlendShapes;

  ezHashedString sKey;
  float fWeight = 0.0f;

  ezUInt16 numWeights = 0;
  s >> numWeights;
  m_Weights.Reserve(numWeights);

  for (ezUInt16 i = 0; i < numWeights; ++i)
  {
    s >> sKey;
    s >> fWeight;

    m_Weights[sKey] = fWeight;
  }

  ResendWeights();
}

void ezBlendShapePoseComponent::OnActivated()
{
  SUPER::OnActivated();

  if (!m_hBlendShapes.IsValid())
  {
    ezMsgQueryAnimationBlendShapes msg;
    GetOwner()->SendMessage(msg);
    if (msg.m_hBlendShapes.IsValid())
    {
      m_hBlendShapes = msg.m_hBlendShapes;
    }
  }

  ResendWeights();
}

void ezBlendShapePoseComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();

  ResendWeights();
}

void ezBlendShapePoseComponent::SetBlendShapes(const ezBlendShapeResourceHandle& hResource)
{
  if (m_hBlendShapes != hResource)
  {
    m_hBlendShapes = hResource;
    ResendWeights();
  }
}

void ezBlendShapePoseComponent::ResendWeights()
{
  if (m_uiResendWeights == 2)
    return;

  m_uiResendWeights = 2;
  static_cast<ezBlendShapePoseComponentManager*>(GetOwningManager())->EnqueueUpdate(GetHandle());
}

const ezRangeView<const char*, ezUInt32> ezBlendShapePoseComponent::GetWeights() const
{
  return ezRangeView<const char*, ezUInt32>(
    []() -> ezUInt32 { return 0; },
    [this]() -> ezUInt32 { return m_Weights.GetCount(); },
    [](ezUInt32& ref_uiIt) { ++ref_uiIt; },
    [this](const ezUInt32& uiIt) -> const char* { return m_Weights.GetKey(uiIt).GetString().GetData(); });
}

void ezBlendShapePoseComponent::SetWeight(const char* szKey, const ezVariant& value)
{
  ezHashedString hs;
  hs.Assign(szKey);

  m_Weights[hs] = value.ConvertTo<float>();

  ResendWeights();
}

void ezBlendShapePoseComponent::RemoveWeight(const char* szKey)
{
  if (m_Weights.RemoveAndCopy(ezTempHashedString(szKey)))
  {
    ResendWeights();
  }
}

bool ezBlendShapePoseComponent::GetWeight(const char* szKey, ezVariant& out_value) const
{
  ezUInt32 it = m_Weights.Find(szKey);

  if (it == ezInvalidIndex)
    return false;

  out_value = m_Weights.GetValue(it);
  return true;
}

void ezBlendShapePoseComponent::SetBlendShapeWeight(ezStringView sShapeName, float fWeight)
{
  ezHashedString hs;
  hs.Assign(sShapeName);

  m_Weights[hs] = fWeight;

  ResendWeights();
}

float ezBlendShapePoseComponent::GetBlendShapeWeight(ezStringView sShapeName) const
{
  ezUInt32 it = m_Weights.Find(ezTempHashedString(sShapeName));
  if (it == ezInvalidIndex)
    return 0.0f;

  return m_Weights.GetValue(it);
}

void ezBlendShapePoseComponent::ResetWeights()
{
  for (ezUInt32 i = 0; i < m_Weights.GetCount(); ++i)
  {
    m_Weights.GetValue(i) = 0.0f;
  }
  ResendWeights();
}

void ezBlendShapePoseComponent::SendWeights()
{
  ezMsgBlendShapesPoseUpdated msg;
  msg.m_Weights = m_Weights;

  GetOwner()->SendMessageRecursive(msg);
}

void ezBlendShapePoseComponent::OnMsgAnimationCurveValue(ezMsgAnimationCurveValue& msg)
{
  SetBlendShapeWeight(msg.m_sCurveName.GetString().GetData(), msg.m_fAverage);
}

void ezBlendShapePoseComponent::OnMsgSetBlendShapeWeight(ezMsgSetBlendShapeWeight& msg)
{
  SetBlendShapeWeight(msg.m_sShapeName.GetString().GetData(), msg.m_fWeight);
}

//////////////////////////////////////////////////////////////////////////

void ezBlendShapePoseComponentManager::Update(const ezWorldModule::UpdateContext& context)
{
  ezDeque<ezComponentHandle> requireUpdate;

  {
    EZ_LOCK(m_Mutex);
    requireUpdate.Swap(m_RequireUpdate);
  }

  for (const auto& hComp : requireUpdate)
  {
    ezBlendShapePoseComponent* pComp = nullptr;
    if (!TryGetComponent(hComp, pComp) || !pComp->IsActiveAndInitialized())
      continue;

    pComp->Update();
  }
}

void ezBlendShapePoseComponentManager::EnqueueUpdate(ezComponentHandle hComponent)
{
  EZ_LOCK(m_Mutex);

  if (m_RequireUpdate.IndexOf(hComponent) != ezInvalidIndex)
    return;

  m_RequireUpdate.PushBack(hComponent);
}

void ezBlendShapePoseComponentManager::Initialize()
{
  SUPER::Initialize();

  ezWorldModule::UpdateFunctionDesc desc = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezBlendShapePoseComponentManager::Update, this);
  desc.m_Phase = ezWorldUpdatePhase::PreAsync;

  RegisterUpdateFunction(desc);
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_Implementation_BlendShapePoseComponent);
