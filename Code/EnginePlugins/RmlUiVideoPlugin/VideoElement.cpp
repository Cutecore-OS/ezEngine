#include <RmlUiVideoPlugin/RmlUiVideoPluginPCH.h>

#include <Core/ResourceManager/ResourceManager.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Threading/AtomicInteger.h>
#include <RendererFoundation/Device/Device.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/PropertyIdSet.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUiVideoPlugin/VideoDecoder.h>
#include <RmlUiVideoPlugin/VideoElement.h>

static ezAtomicInteger32 s_iTextureId;

ezRmlUiVideoElement::ezRmlUiVideoElement(const Rml::String& sTag)
  : Rml::Element(sTag)
{
}

ezRmlUiVideoElement::~ezRmlUiVideoElement() = default;

void ezRmlUiVideoElement::WakeContext()
{
  if (auto* pContext = GetContext())
    pContext->RequestNextUpdate(0);
}

void ezRmlUiVideoElement::Play()
{
  if (m_bEnded)
    Seek(0);
  m_bPaused = false;
  m_bPlaybackOverride |= m_bSourceDirty;
  m_fLastUpdate = Rml::GetSystemInterface()->GetElapsedTime();
  WakeContext();
}

void ezRmlUiVideoElement::Pause()
{
  m_bPaused = true;
  m_bPlaybackOverride |= m_bSourceDirty;
  WakeContext();
}

void ezRmlUiVideoElement::Seek(double fSeconds)
{
  if (!ezMath::IsFinite(fSeconds))
    return;
  m_fPosition = ezMath::Max(0.0, fSeconds);
  m_bSeeking = true;
  if (m_bSourceDirty)
    m_fPendingSeek = m_fPosition;
  m_bEnded = false;
  m_fLastUpdate = Rml::GetSystemInterface()->GetElapsedTime();
  if (m_pDecoder)
    m_pDecoder->RequestFrame(ezTime::MakeFromSeconds(m_fPosition), true);
  // A paused seek still needs updates until a decoded frame arrives.
  m_bReady = false;
  WakeContext();
}

void ezRmlUiVideoElement::LoadSource()
{
  m_bSourceDirty = false;
  m_pDecoder = nullptr;
  m_Texture = {};
  m_hTexture.Invalidate();
  m_vDimensions = {300, 150};
  m_fPosition = ezMath::Max(0.0, m_fPendingSeek);
  m_bReady = false;
  m_bEnded = false;
  m_bFailed = false;
  m_bLoadedData = false;
  if (!m_bPlaybackOverride)
    m_bPaused = !HasAttribute("autoplay") || HasAttribute("paused");
  m_bPlaybackOverride = false;
  m_fLastUpdate = Rml::GetSystemInterface()->GetElapsedTime();
  DirtyLayout();

  const auto sSource = GetAttribute<Rml::String>("src", "");
  if (sSource.empty())
    return;

  Rml::String sPath;
  Rml::GetSystemInterface()->JoinPath(sPath, GetOwnerDocument()->GetSourceURL(), sSource);
  m_pDecoder = EZ_DEFAULT_NEW(ezRmlUiVideoDecoder, sPath.c_str());
  if (m_fPendingSeek >= 0)
    m_pDecoder->RequestFrame(ezTime::MakeFromSeconds(m_fPendingSeek), true);
  m_fPendingSeek = -1;
}

bool ezRmlUiVideoElement::GetIntrinsicDimensions(Rml::Vector2f& out_vDimensions, float& out_fRatio)
{
  out_vDimensions = Rml::Vector2f(m_vDimensions);
  out_fRatio = out_vDimensions.x / out_vDimensions.y;
  const float fWidth = GetAttribute<float>("width", -1);
  const float fHeight = GetAttribute<float>("height", -1);
  if (fWidth > 0)
    out_vDimensions = {fWidth, fWidth / out_fRatio};
  if (fHeight > 0)
    out_vDimensions = {fWidth > 0 ? fWidth : fHeight * out_fRatio, fHeight};
  out_fRatio = out_vDimensions.x / out_vDimensions.y;
  return true;
}

void ezRmlUiVideoElement::OnUpdate()
{
  Rml::Element::OnUpdate();
  if (!GetOwnerDocument() || !GetRenderManager())
    return;

  const bool bWasFailed = !m_bSourceDirty && m_bFailed;
  if (m_bSourceDirty)
    LoadSource();

  const double fNow = Rml::GetSystemInterface()->GetElapsedTime();
  if (m_bReady && !m_bPaused && !m_bEnded)
    m_fPosition += ezMath::Max(0.0, fNow - m_fLastUpdate);
  m_fLastUpdate = fNow;

  Rml::String sEvent;
  if (m_pDecoder && !m_bFailed)
  {
    m_pDecoder->RequestFrame(ezTime::MakeFromSeconds(m_fPosition));
    ezRmlUiVideoDecoder::Frame frame;
    bool bEnded = false;
    bool bFailed = false;
    if (m_pDecoder->TakeFrame(frame, bEnded, bFailed))
    {
      ezGALSystemMemoryDescription memory;
      memory.m_pData = ezMakeByteBlobPtr(frame.m_Pixels.GetData(), frame.m_Pixels.GetCount());
      memory.m_uiRowPitch = frame.m_uiWidth * 4;
      memory.m_uiSlicePitch = frame.m_Pixels.GetCount();

      const Rml::Vector2i vSize(frame.m_uiWidth, frame.m_uiHeight);
      if (!m_hTexture.IsValid() || vSize != m_vDimensions)
      {
        ezTexture2DResourceDescriptor desc;
        desc.m_DescGAL.m_uiWidth = frame.m_uiWidth;
        desc.m_DescGAL.m_uiHeight = frame.m_uiHeight;
        desc.m_DescGAL.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
        desc.m_DescGAL.m_ResourceAccess.m_bImmutable = false;
        desc.m_InitialContent = ezMakeArrayPtr(&memory, 1);
        desc.m_SamplerDesc.m_AddressU = ezImageAddressMode::Clamp;
        desc.m_SamplerDesc.m_AddressV = ezImageAddressMode::Clamp;
        ezStringBuilder sName;
        sName.SetFormat("RmlUiVideo_{}", s_iTextureId.Increment());
        m_hTexture = ezResourceManager::CreateResource<ezTexture2DResource>(sName, std::move(desc));
        m_Texture = GetRenderManager()->MakeCallbackTexture([sResource = Rml::String(sName.GetData())](const Rml::CallbackTextureInterface& textureInterface)
          {
          Rml::Vector2i vDimensions;
          const auto hTexture = Rml::GetRenderInterface()->LoadTexture(vDimensions, sResource);
          if (!hTexture)
            return false;
          textureInterface.SetTextureHandle(hTexture, vDimensions);
          return true; });
        m_vDimensions = vSize;
        m_bGeometryDirty = true;
        DirtyLayout();
      }
      else
      {
        ezResourceLock<ezTexture2DResource> pTexture(m_hTexture, ezResourceAcquireMode::BlockTillLoaded);
        ezGALDevice::GetDefaultDevice()->UpdateTextureForNextFrame(pTexture->GetGALTexture(), memory);
      }
      if (!m_bLoadedData)
        sEvent = "loadeddata";
      else if (m_bSeeking)
        sEvent = "seeked";
      m_bLoadedData = true;
      m_bSeeking = false;
      m_bReady = true;
      // Keep on-demand canvases fresh through queued GPU upload and extraction.
      m_uiRefreshFrames = 3;
    }
    m_bFailed = bFailed;
    if (bEnded && !m_bEnded)
    {
      m_bEnded = true;
      if (HasAttribute("loop") && !m_bPaused)
        Seek(0);
      else
      {
        m_bPaused = true;
        sEvent = "ended";
      }
    }
  }

  if (m_pDecoder && !m_bFailed && !m_bEnded && (!m_bReady || !m_bPaused))
    WakeContext();
  if (m_uiRefreshFrames > 0)
  {
    --m_uiRefreshFrames;
    WakeContext();
  }
  if (m_bFailed && !bWasFailed)
  {
    ezLog::Warning("Failed to decode RmlUi video '{}'.", GetAttribute<Rml::String>("src", "").c_str());
    sEvent = "error";
  }
  if (!sEvent.empty())
    DispatchEvent(sEvent, {});
}

void ezRmlUiVideoElement::OnRender()
{
  if (m_bPosterDirty)
  {
    m_bPosterDirty = false;
    m_Poster = {};
    const auto sPoster = GetAttribute<Rml::String>("poster", "");
    if (!sPoster.empty() && GetOwnerDocument())
      m_Poster = GetRenderManager()->LoadTexture(sPoster, GetOwnerDocument()->GetSourceURL());
  }

  Rml::Texture texture = m_Texture ? Rml::Texture(m_Texture) : m_Poster;
  if (!texture)
    return;
  if (m_bGeometryDirty)
  {
    Rml::Mesh mesh;
    const auto& computed = GetComputedValues();
    const auto box = GetRenderBox(Rml::BoxArea::Content);
    Rml::MeshUtilities::GenerateQuad(mesh, box.GetFillOffset(), box.GetFillSize(),
      computed.image_color().ToPremultiplied(computed.opacity()));
    m_Geometry = GetRenderManager()->MakeGeometry(std::move(mesh));
    m_bGeometryDirty = false;
  }
  m_Geometry.Render(GetAbsoluteOffset(Rml::BoxArea::Border), texture);
}

void ezRmlUiVideoElement::OnResize()
{
  m_bGeometryDirty = true;
}

void ezRmlUiVideoElement::OnPropertyChange(const Rml::PropertyIdSet& properties)
{
  Rml::Element::OnPropertyChange(properties);
  if (properties.Contains(Rml::PropertyId::Opacity) || properties.Contains(Rml::PropertyId::ImageColor))
    m_bGeometryDirty = true;
}

void ezRmlUiVideoElement::OnAttributeChange(const Rml::ElementAttributes& attributes)
{
  Rml::Element::OnAttributeChange(attributes);
  if (attributes.count("src"))
  {
    m_bSourceDirty = true;
    m_bPlaybackOverride = false;
    m_bSeeking = false;
    m_fPendingSeek = -1;
  }
  if (attributes.count("poster"))
    m_bPosterDirty = true;
  if (attributes.count("width") || attributes.count("height"))
    DirtyLayout();
  if (attributes.count("paused"))
  {
    if (HasAttribute("paused"))
      Pause();
    else
      Play();
  }
  if (attributes.count("autoplay") && HasAttribute("autoplay") && !HasAttribute("paused"))
    Play();
  WakeContext();
}
