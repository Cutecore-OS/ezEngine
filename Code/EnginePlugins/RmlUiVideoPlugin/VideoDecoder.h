#pragma once

#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Threading/Thread.h>
#include <Foundation/Threading/ThreadSignal.h>
#include <RmlUiVideoPlugin/RmlUiVideoPluginDLL.h>

/// File decoder with bounded memory. FFmpeg objects live exclusively on the worker thread.
/// The worker retains one due frame and one future frame; the output mailbox holds one RGBA frame.
class EZ_RMLUIVIDEOPLUGIN_DLL ezRmlUiVideoDecoder : public ezThread
{
public:
  struct Frame
  {
    ezDynamicArray<ezUInt8> m_Pixels;
    ezUInt32 m_uiWidth = 0;
    ezUInt32 m_uiHeight = 0;
  };

  explicit ezRmlUiVideoDecoder(ezStringView sPath);
  ~ezRmlUiVideoDecoder();

  void RequestFrame(ezTime position, bool bSeek = false);
  bool TakeFrame(Frame& out_frame, bool& out_bEnded, bool& out_bFailed);

private:
  ezUInt32 Run() override;
  ezResult Decode();

  ezString m_sPath;
  ezMutex m_Mutex;
  ezThreadSignal m_Signal;
  ezTime m_Position;
  Frame m_Frame;
  ezUInt64 m_uiSeekSerial = 0;
  bool m_bStop = false;
  bool m_bFrameAvailable = false;
  bool m_bEnded = false;
  bool m_bFailed = false;
};
