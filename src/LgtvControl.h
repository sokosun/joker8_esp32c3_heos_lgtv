// LgtvControl class communicates with and controls LG TVs.
// Please find the protocol from lgtv2 repository.
//
// Usage:
// 1. Create an Instance
//   LgtvControl lc;
// 2. Connect to the TV - Dialog appears at the first time
//   const IPAddress lgtv(192,168,1,40);
//   lc.Connect(lgtv);
// 3. Call APIs
//   lc.SwitchInput(LgtvControl::InputId::HDMI1);
// 4. Disconnect
//   lc.Disconnect();
// 5. Reconnect
//   lc.Connect(lgtv, lc.GetClientKey());
//
// Note:
// LgtvControl does not support all commands.
// Error handling is incomplete. Some APIs could get stuck.
// ClientKey should be stored into non-volatile memory and reuse it.

#include <WiFi.h>
#include <functional>
#include <ArduinoJson.h>
#include <queue>
#include <unordered_map>
#include <WebSocketsClient.h>

class LgtvControl {
public:
  enum class InputId {
    HDMI1,
    HDMI2,
    HDMI3,
    HDMI4
  };

  LgtvControl();
  ~LgtvControl();

  // Connect() might spend much time.
  // @return true if succeeded. false if not.
  bool Connect(const IPAddress lgtv, String clientkey = "");

  // Disconnect() waits for completion of tasks in m_task_queue.
  // It might spend much time.
  void Disconnect();

  // SwitchInput() pushes a task to switch input. It returns before the task completes.
  void SwitchInput(InputId inputId);

  // Application may read client key to reuse it.
  String GetClientKey();

  // CommandHandler is used as private
  void CommandHandler();

private:
  enum class TYPE {
    Register,
    Request
  };

  enum class URI {
    SwitchInput
  };
  
  const std::unordered_map<LgtvControl::URI, String> URI_LIST = {
    { LgtvControl::URI::SwitchInput, String("ssap://tv/switchInput") }
  };
  String GetUriString(LgtvControl::URI uri){
    return URI_LIST.count(uri) > 0 ? URI_LIST.at(uri) : String();
  }

  void Register(String clientkey);
  void WebSocketEventHandler(WStype_t type, uint8_t * payload, size_t length);

  String PackRegisterMessage(String id, String clientkey);
  String PackRequestMessage(String id, URI uri, DynamicJsonDocument & payload);
  String PackSwitchInputMessage(String id, InputId inputId);

  String IncrementId();
  String GetCurrentId();

// data
  const uint16_t lgtvport = 3000;

  WebSocketsClient m_webSocket;
  std::function<void(uint8_t * payload, size_t length)> m_text_cbk = nullptr;
  String m_clientkey;
  String m_id = "abcdef000000";

  enum STATE {
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_REGISTERED,
    STATE_HALT
  };

  STATE m_state = STATE_DISCONNECTED;

  struct TASK {
    String id;
    TYPE type;
    String message;

    TASK(String id_in, TYPE type_in, String message_in){
      id = id_in;
      type = type_in;
      message = message_in;
    }
  };

  std::queue<TASK> m_task_queue;
};

