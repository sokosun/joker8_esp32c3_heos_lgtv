
#include <unordered_map>
#include <Arduino.h>
#include <WiFi.h>
#include "LgtvControl.h"

namespace {
  String json_pairing(R"({"forcePairing":false,"pairingType":"PROMPT","manifest":{"manifestVersion":1,"appVersion":"1.1","signed":{"created":"20140509","appId":"com.lge.test","vendorId":"com.lge","localizedAppNames":{"":"LG Remote App","ko-KR":"리모컨 앱","zxx-XX":"ЛГ Rэмotэ AПП"},"localizedVendorNames":{"":"LG Electronics"},"permissions":["TEST_SECURE","CONTROL_INPUT_TEXT","CONTROL_MOUSE_AND_KEYBOARD","READ_INSTALLED_APPS","READ_LGE_SDX","READ_NOTIFICATIONS","SEARCH","WRITE_SETTINGS","WRITE_NOTIFICATION_ALERT","CONTROL_POWER","READ_CURRENT_CHANNEL","READ_RUNNING_APPS","READ_UPDATE_INFO","UPDATE_FROM_REMOTE_APP","READ_LGE_TV_INPUT_EVENTS","READ_TV_CURRENT_TIME"],"serial":"2f930e2d2cfe083771f68e4fe7bb07"},"permissions":["LAUNCH","LAUNCH_WEBAPP","APP_TO_APP","CLOSE","TEST_OPEN","TEST_PROTECTED","CONTROL_AUDIO","CONTROL_DISPLAY","CONTROL_INPUT_JOYSTICK","CONTROL_INPUT_MEDIA_RECORDING","CONTROL_INPUT_MEDIA_PLAYBACK","CONTROL_INPUT_TV","CONTROL_POWER","READ_APP_STATUS","READ_CURRENT_CHANNEL","READ_INPUT_DEVICE_LIST","READ_NETWORK_STATE","READ_RUNNING_APPS","READ_TV_CHANNEL_LIST","WRITE_NOTIFICATION_TOAST","READ_POWER_STATE","READ_COUNTRY_INFO","READ_SETTINGS","CONTROL_TV_SCREEN","CONTROL_TV_STANBY","CONTROL_FAVORITE_GROUP","CONTROL_USER_INFO","CHECK_BLUETOOTH_DEVICE","CONTROL_BLUETOOTH","CONTROL_TIMER_INFO","STB_INTERNAL_CONNECTION","CONTROL_RECORDING","READ_RECORDING_STATE","WRITE_RECORDING_LIST","READ_RECORDING_LIST","READ_RECORDING_SCHEDULE","WRITE_RECORDING_SCHEDULE","READ_STORAGE_DEVICE_LIST","READ_TV_PROGRAM_INFO","CONTROL_BOX_CHANNEL","READ_TV_ACR_AUTH_TOKEN","READ_TV_CONTENT_STATE","READ_TV_CURRENT_TIME","ADD_LAUNCHER_CHANNEL","SET_CHANNEL_SKIP","RELEASE_CHANNEL_SKIP","CONTROL_CHANNEL_BLOCK","DELETE_SELECT_CHANNEL","CONTROL_CHANNEL_GROUP","SCAN_TV_CHANNELS","CONTROL_TV_POWER","CONTROL_WOL"],"signatures":[{"signatureVersion":1,"signature":"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw=="}]}})");

  const std::unordered_map<LgtvControl::InputId, String> INPUTID_LIST = {
    { LgtvControl::InputId::HDMI1, String("HDMI_1") },
    { LgtvControl::InputId::HDMI2, String("HDMI_2") },
    { LgtvControl::InputId::HDMI3, String("HDMI_3") },
    { LgtvControl::InputId::HDMI4, String("HDMI_4") }
  };

  String GetInputIdString(LgtvControl::InputId id){
    return INPUTID_LIST.count(id) > 0 ? INPUTID_LIST.at(id) : String();
  }
}

void LgtvControlTaskThread(void * lc){
  ((LgtvControl *)lc)->CommandHandler();
  vTaskDelete(NULL);
}

LgtvControl::LgtvControl(){
}

LgtvControl::~LgtvControl(){
}

String LgtvControl::GetClientKey(){
  return m_clientkey;
}

bool LgtvControl::Connect(const IPAddress lgtv, String clientkey){
  if(m_webSocket.isConnected()){
    Disconnect();
  }

  m_clientkey = clientkey;
  m_state = STATE_DISCONNECTED;

  m_webSocket.begin(lgtv, lgtvport);
  m_webSocket.onEvent([this](WStype_t type, uint8_t * payload, size_t length){WebSocketEventHandler(type, payload, length);});
  m_webSocket.setReconnectInterval(5000);

  std::queue<TASK> empty_queue;
  m_task_queue.swap(empty_queue);
  xTaskCreatePinnedToCore(LgtvControlTaskThread, "LgtvControl::CommandHandler", 8192, (void*)this, 1, nullptr, 0);

  uint32_t spent = 0;
  const uint32_t timeout_ms = 5000;
  while(m_state != STATE_REGISTERED){
    if(spent > timeout_ms){
      return false;
    }
    spent += 10;
    delay(10);
  }

  return true;
}

void LgtvControl::Disconnect(){
  if(!m_webSocket.isConnected()){
    return;
  }

  while(!m_task_queue.empty()){
    delay(1);
  }
  m_webSocket.disconnect();
}

void LgtvControl::CommandHandler(){
  Serial.printf("(LGTV)CommandHandler started\r\n");

  while(m_state != STATE_HALT){
    m_webSocket.loop();

    if(m_task_queue.empty()){
      delay(10);
      continue;
    }

    TASK task = m_task_queue.front();

    if(task.type == TYPE::Request && m_state != STATE_REGISTERED){
      Serial.printf("(LGTV)Task dropped.\r\n");
      m_task_queue.pop();
      continue;
    }

    Serial.printf("(LGTV)Send: %s\r\n", task.message.c_str());

    bool response_received = false;
    m_text_cbk = [this, &task, &response_received](uint8_t * payload, size_t length){
      DynamicJsonDocument doc(256); // TODO Check Overflow
      deserializeJson(doc, payload);

      if(doc["id"] != task.id){
        return;
      }

      if(doc["type"] == "response"){
        bool result = doc["payload"]["returnValue"];
        if(!result){
          Serial.printf("(LGTV)Command Failed\r\n");
        }
        if(task.type == TYPE::Request){
          response_received = true;
        }
      }else if(doc["type"] == "registered"){
        m_clientkey = doc["payload"]["client-key"].as<String>();
        Serial.printf("(LGTV)Client Key: %s\r\n", m_clientkey.c_str());
        m_state = STATE_REGISTERED;
        response_received = true;
      }
    };

    m_webSocket.sendTXT(task.message);

    uint32_t spent = 0;
    const uint32_t timeout_ms = 1000;
    while(!response_received){
      m_webSocket.loop();
      if(spent > timeout_ms){
        break;
      }
      spent += 10;
      delay(10);
    }

    m_text_cbk = nullptr;
    m_task_queue.pop();

  }

  Serial.printf("(LGTV)CommandHandler stopped\r\n");
}

String LgtvControl::PackSwitchInputMessage(String id, InputId inputId){
  DynamicJsonDocument payload(64);
  payload["inputId"] = GetInputIdString(inputId);
  return PackRequestMessage(id, URI::SwitchInput, payload);
}

String LgtvControl::PackRegisterMessage(String id, String clientkey){
  DynamicJsonDocument doc(2560);
  doc["id"]      = id;
  doc["type"]    = "register";

  if(m_clientkey.isEmpty()){
    DynamicJsonDocument payload(2560);
    deserializeJson(payload, json_pairing);
    doc["payload"] = payload;
  }else{
    DynamicJsonDocument payload(128);
    payload["client-key"] = m_clientkey;
    doc["payload"] = payload;
  }

  String register_msg;
  serializeJson(doc, register_msg);
  return register_msg;
}

String LgtvControl::PackRequestMessage(String id, URI uri, DynamicJsonDocument & payload){
  DynamicJsonDocument doc(256);
  doc["id"]      = id;
  doc["type"]    = "request";
  doc["uri"]     = GetUriString(uri);
  doc["payload"] = payload;

  String request_msg;
  serializeJson(doc, request_msg);
  return request_msg;
}

void LgtvControl::WebSocketEventHandler(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.printf("(LGTV)Connected\r\n");
      m_state = STATE_CONNECTED;
      Register(m_clientkey);
      break;

    case WStype_TEXT:
      Serial.printf("(LGTV)Recv: %s\r\n", payload);
      if(m_text_cbk){
        m_text_cbk(payload, length);
      }
      break;

    case WStype_DISCONNECTED:
      Serial.printf("(LGTV)Disconnected\r\n");
      m_state = STATE_HALT;
      break;

    default:
      break;
  }
}

void LgtvControl::Register(String clientkey){
  String id = IncrementId();
  String msg = PackRegisterMessage(id, clientkey);
  TASK task(id, TYPE::Register, msg);
  m_task_queue.push(task);
}

void LgtvControl::SwitchInput(InputId inputId){
  String id = IncrementId();
  String msg = PackSwitchInputMessage(id, inputId);
  TASK task(id, TYPE::Request, msg);
  m_task_queue.push(task);
}

String LgtvControl::IncrementId(){
  static uint32_t count = 0;
  const uint32_t offset = 100000;
  count++;
  m_id = "abcdef" + String(count + offset);
  return m_id;
}

String LgtvControl::GetCurrentId(){
  return m_id;
}
