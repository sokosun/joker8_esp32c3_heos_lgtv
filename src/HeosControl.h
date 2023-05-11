// HeosControl class communicates with and controls HEOS CLI supported devices.
// Please find HEOS CLI Protocol Specification for details.
//
// Usage:
// 1. Create an Instance
//   HeosControl hc;
// 2. Connect to the HEOS device
//   const IPAddress heosdevice(192,168,1,40);
//   hc.Connect(heosdevice);
// 3. Call APIs
//   hc.SetVolume(20);
//   hc.VolumeUp();
//   hc.ToggleMute();
//   ...
// 4. Disconnect
//   hc.Disconnect();
// 5. Reconnect
//   hc.Connect(heosdevice, true);
//
// Note:
// HeosControl does not support all commands.
// Error handling is incomplete. Some APIs could get stuck.

#include <WiFi.h>
#include <functional>
#include <ArduinoJson.h>
#include <queue>

class HeosControl {
public:
  enum class COMMAND {
    GetPlayers,
    SetVolume,
    VolumeUp,
    VolumeDown,
    SetMute,
    ToggleMute,
    PlayInputSource,
    Invalid
  };

  enum class INPUT_SOURCE {
    ANALOG_IN_1,
    ANALOG_IN_2,
    USBDAC,
    OPTICAL_IN_1,
    OPTICAL_IN_2,
    COAX_IN_1,
    COAX_IN_2,
    Invalid
  };

  HeosControl();
  ~HeosControl();

  /// @return false if failed.
  bool Connect(const IPAddress heosdevice, bool reuse_pid = false);
  bool Disconnect();

//----- HEOS Commands -----//
  // Any HEOS commands return true if succeeded, false if not.

  /// @param response_callback is a callback called with response 
  bool GetPlayers(std::function<void(DynamicJsonDocument)> response_callback);

  /// @param level of volume. (0 to 100)
  bool SetVolume(unsigned int level);

  /// @param step level of volume. (1 to 10)
  bool VolumeUp(unsigned int step = 5);

  /// @param step level of volume. (1 to 10)
  bool VolumeDown(unsigned int step = 5);

  /// @param state to be set. Unmute if false, mute if true.
  bool SetMute(bool state = true);

  bool ToggleMute();

  bool PlayInputSource(INPUT_SOURCE input);

//----- Others -----//
  // CommandHandler is used as private
  void CommandHandler();

private:
  /// WaitJsonResponse could get stuck because it does not check timeout.
  /// @return JSON formatted string if succeeded. Return empty String if failed.
  String WaitJsonResponse(uint32_t timeout_ms = 5000);

  struct TASK {
    COMMAND cmd;
    String uri;
    std::function<void(DynamicJsonDocument)> response_callback;

    TASK(COMMAND cmd_in, String uri_in, std::function<void(DynamicJsonDocument)> response_callback_in = nullptr){
      cmd = cmd_in;
      uri = uri_in;
      response_callback = response_callback_in;
    }
  };

  std::queue<TASK> m_task_queue;
  const uint16_t heosport = 1255;
  WiFiClient m_self;
  long m_pid = 0;
};

