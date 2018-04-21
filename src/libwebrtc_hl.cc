
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

//#define WEBRTC_ANDROID 1
//#define WEBRTC_IOS 1
//#define WEBRTC_LINUX 1
//#define WEBRTC_WIN 1

// WebRTC関連のヘッダ
#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/base/flags.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/base/ssladapter.h>
#include <webrtc/base/thread.h>

// picojsonはコピペ用データ構造を作るために使う
#include "picojson.h"
#include "libwebrtc_hl.hh"


class Connection {
 public:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
  std::string sdp_type;
  picojson::array ice_array;

  // Offer/Answerの作成が成功したら、LocalDescriptionとして設定 & 相手に渡す文字列として表示
  void onSuccessCSD(webrtc::SessionDescriptionInterface* desc) {
    peer_connection->SetLocalDescription(ssdo, desc);

    std::string sdp;
    desc->ToString(&sdp);
    std::cout << sdp_type << " SDP:begin" << std::endl << sdp << sdp_type << " SDP:end" << std::endl;
  }

  // ICEを取得したら、表示用JSON配列の末尾に追加
  void onIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    picojson::object ice;
    std::string candidate_str;
    candidate->ToString(&candidate_str);
    ice.insert(std::make_pair("candidate", picojson::value(candidate_str)));
    ice.insert(std::make_pair("sdpMid", picojson::value(candidate->sdp_mid())));
    ice.insert(std::make_pair("sdpMLineIndex", picojson::value(static_cast<double>(candidate->sdp_mline_index()))));
    ice_array.push_back(picojson::value(ice));
  }

  class PCO : public webrtc::PeerConnectionObserver {
   private:
    Connection& parent;

   public:
    PCO(Connection& parent) : parent(parent) {
    }
  
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::SignalingChange(" << new_state << ")" << std::endl;
    };

    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> ) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::AddStream" << std::endl;
    };

    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> ) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::RemoveStream" << std::endl;
    };

    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::DataChannel(" << data_channel
                << ", " << parent.data_channel.get() << ")" << std::endl;
      // Answer送信側は、onDataChannelでDataChannelの接続を受け付ける
      parent.data_channel = data_channel;
      parent.data_channel->RegisterObserver(&parent.dco);
    };

    void OnRenegotiationNeeded() override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::RenegotiationNeeded" << std::endl;
    };

    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceConnectionChange(" << new_state << ")" << std::endl;
    };

    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceGatheringChange(" << new_state << ")" << std::endl;
    };

    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
      std::cout << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceCandidate" << std::endl;
      parent.onIceCandidate(candidate);
    };
  };
  
  class DCO : public webrtc::DataChannelObserver {
   private:
    Connection& parent;

   public:
    DCO(Connection& parent) : parent(parent) {
    }

    // 接続状況が変化した時に発火する。切断は発火タイミングで値を確認して検知可能
    void OnStateChange() override {
      std::cout << std::this_thread::get_id() << ":"
                << "DataChannelObserver::StateChange" << std::endl;
    };
    
    // メッセージ受信
    void OnMessage(const webrtc::DataBuffer& buffer) override {
      std::cout << std::this_thread::get_id() << ":"
                << "DataChannelObserver::Message" << std::endl;
      std::cout << std::string(buffer.data.data<char>(), buffer.data.size()) << std::endl;
    };

    void OnBufferedAmountChange(uint64_t previous_amount) override {
      std::cout << std::this_thread::get_id() << ":"
                << "DataChannelObserver::BufferedAmountChange(" << previous_amount << ")" << std::endl;
    };
  };

  class CSDO : public webrtc::CreateSessionDescriptionObserver {
   private:
    Connection& parent;

   public:
    CSDO(Connection& parent) : parent(parent) {
    }
  
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
      std::cout << std::this_thread::get_id() << ":"
                << "CreateSessionDescriptionObserver::OnSuccess" << std::endl;
      parent.onSuccessCSD(desc);
    };

    void OnFailure(const std::string& error) override {
      std::cout << std::this_thread::get_id() << ":"
                << "CreateSessionDescriptionObserver::OnFailure" << std::endl << error << std::endl;
    };
  };

  class SSDO : public webrtc::SetSessionDescriptionObserver {
   private:
    Connection& parent;

   public:
    SSDO(Connection& parent) : parent(parent) {
    }
    
    void OnSuccess() override {
      std::cout << std::this_thread::get_id() << ":"
                << "SetSessionDescriptionObserver::OnSuccess" << std::endl;
    };

    void OnFailure(const std::string& error) override {
      std::cout << std::this_thread::get_id() << ":"
                << "SetSessionDescriptionObserver::OnFailure" << std::endl << error << std::endl;
    };
  };

  PCO  pco;
  DCO  dco;
  rtc::scoped_refptr<CSDO> csdo;
  rtc::scoped_refptr<SSDO> ssdo;

  Connection() :
      pco(*this),
      dco(*this),
      csdo(new rtc::RefCountedObject<CSDO>(*this)),
      ssdo(new rtc::RefCountedObject<SSDO>(*this)) {
  }
};

std::unique_ptr<rtc::Thread> thread;
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
webrtc::PeerConnectionInterface::RTCConfiguration configuration;
Connection connection;
rtc::PhysicalSocketServer socket_server;

class CustomRunnable : public rtc::Runnable {
 public:
  void Run(rtc::Thread* subthread) override {
    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory());
    if (peer_connection_factory.get() == nullptr) {
      std::cout << "Error on CreatePeerConnectionFactory." << std::endl;
      return;
    }

    subthread->Run();
  }
};

void cmd_sdp1() {
  connection.peer_connection = peer_connection_factory->CreatePeerConnection(configuration, nullptr, nullptr, &connection.pco);

  webrtc::DataChannelInit config;
  // DataChannelの設定

  connection.data_channel = connection.peer_connection->CreateDataChannel("data_channel", &config);
  connection.data_channel->RegisterObserver(&connection.dco);

  if (connection.peer_connection.get() == nullptr) {
    peer_connection_factory = nullptr;
    std::cout << "Error on CreatePeerConnection." << std::endl;
    return;
  }
  connection.sdp_type = "Offer"; // 表示用の文字列、webrtcの動作には関係ない
  connection.peer_connection->CreateOffer(connection.csdo, nullptr);
}

void cmd_sdp2(const std::string& parameter) {
  connection.peer_connection = peer_connection_factory->CreatePeerConnection(configuration, nullptr, nullptr, &connection.pco);

  if (connection.peer_connection.get() == nullptr) {
    peer_connection_factory = nullptr;
    std::cout << "Error on CreatePeerConnection." << std::endl;
    return;
  }
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* session_description(
      webrtc::CreateSessionDescription("offer", parameter, &error));
  if (session_description == nullptr) {
    std::cout << "Error on CreateSessionDescription." << std::endl
              << error.line << std::endl
              << error.description << std::endl;
    std::cout << "Offer SDP:begin" << std::endl << parameter << std::endl << "Offer SDP:end" << std::endl;
  }
  connection.peer_connection->SetRemoteDescription(connection.ssdo, session_description);

  connection.sdp_type = "Answer"; // 表示用の文字列、webrtcの動作には関係ない
  connection.peer_connection->CreateAnswer(connection.csdo, nullptr);
}

void cmd_sdp3(const std::string& parameter) {
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* session_description(
      webrtc::CreateSessionDescription("answer", parameter, &error));
  if (session_description == nullptr) {
    std::cout << "Error on CreateSessionDescription." << std::endl
              << error.line << std::endl
              << error.description << std::endl;
    std::cout << "Answer SDP:begin" << std::endl << parameter << std::endl << "Answer SDP:end" << std::endl;
  }
  connection.peer_connection->SetRemoteDescription(connection.ssdo, session_description);
}

void cmd_ice1() {
  std::cout << picojson::value(connection.ice_array).serialize(true) << std::endl;
  connection.ice_array.clear();
}

void cmd_ice2(const std::string& parameter) {
  picojson::value v;
  std::string err = picojson::parse(v, parameter);
  if (!err.empty()) {
    std::cout << "Error on parse json : " << err << std::endl;
    return;
  }

  webrtc::SdpParseError err_sdp;
  for (auto& ice_it : v.get<picojson::array>()) {
    picojson::object& ice_json = ice_it.get<picojson::object>();
    webrtc::IceCandidateInterface* ice =
      CreateIceCandidate(ice_json.at("sdpMid").get<std::string>(),
                         static_cast<int>(ice_json.at("sdpMLineIndex").get<double>()),
                         ice_json.at("candidate").get<std::string>(),
                         &err_sdp);
    if (!err_sdp.line.empty() && !err_sdp.description.empty()) {
      std::cout << "Error on CreateIceCandidate" << std::endl
                << err_sdp.line << std::endl
                << err_sdp.description << std::endl;
      return;
    }
    connection.peer_connection->AddIceCandidate(ice);
  }
}

void cmd_send(const std::string& parameter) {
  webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(parameter.c_str(), parameter.size()), true);
  std::cout << "Send(" << connection.data_channel->state() << ")" << std::endl;
  connection.data_channel->Send(buffer);
}

void cmd_quit() {
  // スレッドを活かしながらCloseしないと、別スレッドからのイベント待ちになり終了できなくなる
  connection.peer_connection->Close();
  connection.peer_connection = nullptr;
  connection.data_channel = nullptr;
  peer_connection_factory = nullptr;
  // リソースを開放したらスレッドを止めてOK
  thread->Quit();
}


LibWebRTC::WebRTCConnection::WebRTCConnection(void) { }
LibWebRTC::WebRTCConnection::~WebRTCConnection(void) { }

void LibWebRTC::WebRTCConnection::run(int argc, char* argv[]) {
  // 第三引数にtrueを指定すると、WebRTC関連の引数をargvから削除してくれるらしい
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  rtc::FlagList::Print(nullptr, false);

  std::cout << std::this_thread::get_id() << ":"
            << "Main thread" << std::endl;

  // GoogleのSTUNサーバを利用
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = "stun:stun.l.google.com:19302";
  configuration.servers.push_back(ice_server);

  thread.reset(new rtc::Thread(&socket_server));
  
  rtc::InitializeSSL();

  CustomRunnable runnable;
  thread->Start(&runnable);

  std::string line;
  std::string command;
  std::string parameter;
  bool is_cmd_mode = true;

  while (std::getline(std::cin, line)) {
    if (is_cmd_mode) {
      if (line == "") {
        continue;

      } else if (line == "sdp1") {
        cmd_sdp1();

      } else if (line == "sdp2") {
        command = "sdp2";
        is_cmd_mode = false;

      } else if (line == "sdp3") {
        command = "sdp3";
        is_cmd_mode = false;

      } else if (line == "ice1") {
        cmd_ice1();

      } else if (line == "ice2") {
        command = "ice2";
        is_cmd_mode = false;

      } else if (line == "send") {
        command = "send";
        is_cmd_mode = false;

      } else if (line == "quit") {
        cmd_quit();
        break;

      } else {
        std::cout << "?" << line << std::endl;
      }

    } else {
      if (line == ";") {
        if (command == "sdp2") {
          cmd_sdp2(parameter);

        } else if (command == "sdp3") {
          cmd_sdp3(parameter);

        } else if (command == "ice2") {
          cmd_ice2(parameter);

        } else if (command == "send") {
          cmd_send(parameter);
        }
        
        parameter = "";
        is_cmd_mode = true;

      } else {
        parameter += line + "\n";
      }
    }
  }

  thread.reset();
  rtc::CleanupSSL();

}
