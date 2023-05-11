
#include <unordered_map>
#include "HeosControl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
  const std::unordered_map<HeosControl::COMMAND, String> COMMAND_LIST = {
    { HeosControl::COMMAND::GetPlayers,        String("player/get_players") },
    { HeosControl::COMMAND::SetVolume,         String("player/set_volume")  },
    { HeosControl::COMMAND::VolumeUp,          String("player/volume_up")   },
    { HeosControl::COMMAND::VolumeDown,        String("player/volume_down") },
    { HeosControl::COMMAND::SetMute,           String("player/set_mute")    },
    { HeosControl::COMMAND::ToggleMute,        String("player/toggle_mute") },
    { HeosControl::COMMAND::PlayInputSource,   String("player/play_input")  }
  };

  const std::unordered_map<HeosControl::INPUT_SOURCE, String> INPUT_SOURCE_LIST = {
    { HeosControl::INPUT_SOURCE::ANALOG_IN_1,  String("inputs/analog_in_1")  },
    { HeosControl::INPUT_SOURCE::ANALOG_IN_2,  String("inputs/analog_in_2")  },
    { HeosControl::INPUT_SOURCE::USBDAC,       String("inputs/usbdac")       },
    { HeosControl::INPUT_SOURCE::OPTICAL_IN_1, String("inputs/optical_in_1") },
    { HeosControl::INPUT_SOURCE::OPTICAL_IN_2, String("inputs/optical_in_2") },
    { HeosControl::INPUT_SOURCE::COAX_IN_1,    String("inputs/coax_in_1")    },
    { HeosControl::INPUT_SOURCE::COAX_IN_2,    String("inputs/coax_in_2")    }
  };

  String GetCommandName(HeosControl::COMMAND cmd){
    return COMMAND_LIST.count(cmd) > 0 ? COMMAND_LIST.at(cmd) : String();
  }

  String GetInputSourceName(HeosControl::INPUT_SOURCE input){
    return INPUT_SOURCE_LIST.count(input) > 0 ? INPUT_SOURCE_LIST.at(input) : String();
  }
}

HeosControl::HeosControl(){
}

HeosControl::~HeosControl(){
}

void HeosControlTaskThread(void * hc){
  ((HeosControl *)hc)->CommandHandler();
  vTaskDelete(NULL);
}

void HeosControl::CommandHandler(){
  Serial.printf("(HEOS)CommandHandler started\r\n");

  while(m_self.connected()){
    if(m_task_queue.empty()){
      delay(10);
      continue;
    }

    const TASK task = m_task_queue.front();

    Serial.printf("(HEOS)Send: %s", task.uri.c_str());
    m_self.print(task.uri);

    auto response = WaitJsonResponse(500);
    if(response.isEmpty()){
      Serial.print("(HEOS)Missing response\r\n"); 
      m_task_queue.pop();
      continue;
    }
    
    Serial.printf("(HEOS)Recv: %s", response.c_str());

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response.c_str());
    if(error){
      Serial.printf("Invalid response: %s\r\n", error.c_str());
      m_task_queue.pop();
      continue;
    }

    String response_heos_command = doc["heos"]["command"];
    String response_heos_result  = doc["heos"]["result"];
    
    if(GetCommandName(task.cmd) != response_heos_command){
      Serial.printf("(HEOS)Command mismatch\r\n");
      m_task_queue.pop();
      continue;
    }

    if(response_heos_result != "success"){
      Serial.printf("(HEOS)Command failure\r\n");
      m_task_queue.pop();
      continue;
    }

    if(task.response_callback){
      task.response_callback(doc);
    }
    m_task_queue.pop();
  }

  Serial.printf("(HEOS)CommandHandler stopped\r\n");
}

bool HeosControl::Connect(const IPAddress heosdevice, bool reuse_pid){
  if(m_self.connected()){
    Disconnect();
  }

// FYI: WiFiClient::connect sometimes fail. Then, Please wait 30 sec. and retry.
  m_self.connect(heosdevice, heosport);
  delay(100);
  if(!m_self.connected()){
    Serial.printf("(HEOS)Cannot connect to HEOS device\r\n");
    return false;
  }
  Serial.printf("(HEOS)Connected\r\n");

  std::queue<TASK> empty_queue;
  m_task_queue.swap(empty_queue);

  xTaskCreatePinnedToCore(HeosControlTaskThread, "HeosControl::CommandHandler", 8192, (void*)this, 1, nullptr, 0);
  
  if(m_pid != 0 && reuse_pid){
    return true;
  }

  bool updated = false;

  auto response_callback = [this, &updated](DynamicJsonDocument doc){
    m_pid = doc["payload"][0]["pid"];
    Serial.printf("(HEOS)Player ID: %d\r\n", m_pid);
    updated = true;
  };

  if(!GetPlayers(response_callback)){
    Serial.printf("(HEOS)Cannot get player_id\r\n");
    return false;
  }

  while(1){
    delay(1);
    if(updated){break;}
  }

  return true;
}

bool HeosControl::Disconnect(){
  if(!m_self.connected()){
    return true;
  }

  while(!m_task_queue.empty()){
    delay(1);
  }

  m_self.stop();
  Serial.printf("(HEOS)Disconnected\r\n");
  return true;
}

String HeosControl::WaitJsonResponse(uint32_t timeout_ms){
  String response;
  uint32_t spent = 0;

// FYI: Delimiter of HEOS CLI protocol is "\r\n" 
  while(1){
    if(!m_self.available()){
      if(spent > timeout_ms){
        return String();
      }
      spent += 10;
      delay(10);
      continue;
    }

    char c = m_self.read();
    response += c;

    if(c == '\n'){
      return response;
    }
  }
}

//----- HEOS Commands -----//

bool HeosControl::GetPlayers(std::function<void(DynamicJsonDocument)> response_callback){
  const String uri = String("heos://") + GetCommandName(COMMAND::GetPlayers) + String("\r\n");
  Serial.print(uri);
  m_task_queue.push(TASK(COMMAND::GetPlayers, uri, response_callback));
  return true;
}

bool HeosControl::SetVolume(unsigned int level){
  if(level > 100){
    return false;
  }
  const auto uri = String("heos://") + GetCommandName(COMMAND::SetVolume) + String("?pid=") + String(m_pid) + String("&level=") + String(level) + String("\r\n");
  m_task_queue.push(TASK(COMMAND::SetVolume, uri));
  return true;
}

bool HeosControl::VolumeUp(unsigned int step){
  if(step == 0 || step > 10){
    return false;
  }
  const auto uri = String("heos://") + GetCommandName(COMMAND::VolumeUp) + String("?pid=") + String(m_pid) + String("&step=") + String(step) + String("\r\n");
  m_task_queue.push(TASK(COMMAND::VolumeUp, uri));
  return true;
}

bool HeosControl::VolumeDown(unsigned int step){
  if(step == 0 || step > 10){
    return false;
  }
  const auto uri = String("heos://") + GetCommandName(COMMAND::VolumeDown) + String("?pid=") + String(m_pid) + String("&step=") + String(step) + String("\r\n");
  m_task_queue.push(TASK(COMMAND::VolumeDown, uri));
  return true;
}

bool HeosControl::SetMute(bool state){
  const auto uri = String("heos://") + GetCommandName(COMMAND::SetMute) + String("?pid=") + String(m_pid) + (state ? String("&state=on\r\n") : String("&state=off\r\n"));
  m_task_queue.push(TASK(COMMAND::SetMute, uri));
  return true;
}

bool HeosControl::ToggleMute(){
  const auto uri = String("heos://") + GetCommandName(COMMAND::ToggleMute) + String("?pid=") + String(m_pid) + String("\r\n");
  m_task_queue.push(TASK(COMMAND::ToggleMute, uri));
  return true;
}

bool HeosControl::PlayInputSource(INPUT_SOURCE input){
  const auto uri = String("heos://") + GetCommandName(COMMAND::PlayInputSource) + String("?pid=") + String(m_pid) + String("&input=") + GetInputSourceName(input) + String("\r\n");
  m_task_queue.push(TASK(COMMAND::PlayInputSource, uri));
  return true;
}


