#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "connection.hh"

namespace LibWebRTC {

  class WebRTCConnection {
  private:

    //rtc::PhysicalSocketServer socket_server; // TODO wtf is happening?

    Connection connection;
    CustomRunnable runnable;

    std::unique_ptr<rtc::Thread> thread;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
    // webrtc::PeerConnectionInterface::RTCConfiguration configuration;
    // rtc::scoped_refptr<webrtc::DataChannelInterface> channel;
    std::mutex peer_connection_factory_mutex;
    
    // void createPeerConnection(void);
    // void disconnectFromCurrentPeer(void);    

  public:
    //WebRTCConnection(void);
    WebRTCConnection(std::string kind);
    ~WebRTCConnection(void);

    std::string get_offer(void);

    //void run(int argc, char* argv[]);
  
  };
  

}
