#pragma once

#include <RmlUiVideoPlugin/RmlUiVideoPluginDLL.h>

#include <Foundation/Types/UniquePtr.h>
#include <RendererCore/Textures/Texture2DResource.h>
#include <RmlUi/Core/CallbackTexture.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/Texture.h>

class ezRmlUiVideoDecoder;

/// A replaced RmlUi element rendered through the standard ezEngine RmlUi renderer.
/// Attributes: src, autoplay, loop, paused, poster, width, height.
/// Video playback is silent. Use Play/Pause/Seek or data-bind the paused attribute.
class EZ_RMLUIVIDEOPLUGIN_DLL ezRmlUiVideoElement : public Rml::Element
{
public:
  RMLUI_RTTI_DefineWithParent(ezRmlUiVideoElement, Rml::Element);

  explicit ezRmlUiVideoElement(const Rml::String& sTag);
  ~ezRmlUiVideoElement();

  void Play();
  void Pause();
  void Seek(double fSeconds);
  bool IsPaused() const { return m_bPaused; }
  bool HasEnded() const { return m_bEnded; }
  double GetCurrentTime() const { return m_fPosition; }

  bool GetIntrinsicDimensions(Rml::Vector2f& out_vDimensions, float& out_fRatio) override;

protected:
  void OnUpdate() override;
  void OnRender() override;
  void OnResize() override;
  void OnAttributeChange(const Rml::ElementAttributes& attributes) override;
  void OnPropertyChange(const Rml::PropertyIdSet& properties) override;

private:
  void LoadSource();
  void WakeContext();

  ezUniquePtr<ezRmlUiVideoDecoder> m_pDecoder;
  ezTexture2DResourceHandle m_hTexture;
  Rml::CallbackTexture m_Texture;
  Rml::Texture m_Poster;
  Rml::Geometry m_Geometry;
  Rml::Vector2i m_vDimensions = {300, 150};
  double m_fPosition = 0;
  double m_fLastUpdate = 0;
  double m_fPendingSeek = -1;
  ezUInt32 m_uiRefreshFrames = 0;
  bool m_bSourceDirty = true;
  bool m_bPosterDirty = true;
  bool m_bGeometryDirty = true;
  bool m_bPaused = true;
  bool m_bPlaybackOverride = false;
  bool m_bSeeking = false;
  bool m_bLoadedData = false;
  bool m_bReady = false;
  bool m_bEnded = false;
  bool m_bFailed = false;
};
