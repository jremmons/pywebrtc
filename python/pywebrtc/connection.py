import pywebrtc._ext.pywebrtc as pywebrtc_wrapper
import json
import os
import threading
import time
import websocket
import signal

import logging; logging.basicConfig(level=logging.INFO)


class Connection:

    
    def __init__(self, signaling_url, signaling_id, video_device_number, use_video=True, video_device_name=None, kind="server", timeout=5, webrtc_debug=False):
        # The constructor will check the that video_device exists,
        # but it will neither establish setup the connection to the
        # client. Call `wait_for_client` once you are ready to setup
        # the connection. 

        self.logger = logging.getLogger('signaling_id:{}-video_device_num:{}'.format(signaling_id, video_device_number))
        
        self.signaling_url = signaling_url
        self.signaling_id = signaling_id
        self.signaling_kind = kind
        self.timeoutOccurred = False
        self.signaling_thread = threading.Thread(target=self._signaling_handler, args=(timeout,))
        self.use_video = use_video
        
        if self.use_video:
            self.video_device_number = video_device_number
            video_device_path = '/dev/video{}'.format(self.video_device_number)
            if not os.path.exists(video_device_path):
                raise FileNotFoundError('The video device {} does not exist.'.format(video_device_path))

            # For pyfakewebcam, video device names are assigned as the following:
            # self.video_device_name = 'platform:v4l2loopback-{}'.format(str(video_device_number).zfill(3))
            # Otherwise, to find video device name, run:
            # v4l2-ctl --list-devices
            if video_device_name is None:
                self.video_device_name = 'platform:v4l2loopback-{}'.format(str(self.video_device_number).zfill(3))
            else:
                self.video_device_name = video_device_name
        
        # Prevent python from eating ctrl-C signals
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        self.rtc_connection = pywebrtc_wrapper.PyWebRTCConnection(kind, webrtc_debug)
        self.ws = websocket.WebSocketApp(self.signaling_url, 
                                         on_message=self._on_message(),
                                         on_error=self._on_error(),
                                         on_close=self._on_close(),
                                         on_open = self._on_open())

        self.rtc_connection_ready = False
        

    def wait_for_client(self):
        # This method sets up the RTC connection with the client.
        # Once this method returns, you will be able to use the
        # `send_message` and `receive_messages` methods to
        # communication with the client. 
        
        self.ws.run_forever()
        self.signaling_thread.join()

        self.rtc_connection_ready = True
        return self.timeoutOccurred
        
        
    def send_message(self, message):
        # Expects a python string as input.
        # Will send the entire string as a single 'message'.

        if not self.rtc_connection_ready:
            raise Exception('An RTC connection to the client is not setup. Please call `wait_for_client` first.')
        
        self.rtc_connection.sendString(message)

        
    def receive_messages(self):
        # Will return a python list of ALL received messages.
        # Will block if there are no new messages
        #
        # Note: WebRTC data channels are 'message based' meaning that
        # the received should received the entire message with a single
        # receive. There is no need to parse the stream yourself.

        if not self.rtc_connection_ready:
            raise Exception('An RTC connection to the client is not setup. Please call `wait_for_client` first.')

        return self.rtc_connection.readFromDataChannel()
    
    def is_data_channel_open(self):
        return self.rtc_connection.dataChannelOpen()

    
    # private methods below... (don't use unless you know what you are doing)
    def _on_error(self):
        def f(ws, error):
            self.logger.error('an error occured on the websocket connection to the signaliing server: ' + str(error))
        return f

        
    def _on_close(self):
        def f(ws):
            self.logger.info('websocket closed')
        return f

        
    def _on_open(self):
        def f(ws):
                self.logger.info('websocket open')
                self.signaling_thread.start()
        return f

        
    def _signaling_handler(self, timeout):

        # Send information about ourselves
        self.logger.info('Sending Kind')
        message = json.dumps({'type': 'kind',
                              'kind': self.signaling_kind,
                              'connection_id': self.signaling_id}) 
        self.ws.send(message)
        
        self.logger.info('kind sent! waiting for client to connect.')
        
        # wait until data channel is open
        currentSleep = 0
        while(not self.rtc_connection.dataChannelOpen()):
            time.sleep(0.1)
            currentSleep += 0.1
            if(currentSleep > timeout):
                self.logger.info('Attempted to connect to data channel but timeout exceeded.')
                self.timeoutOccurred = True
                self.ws.close()
                return
        if self.use_video:
              
          # add video/audio streams
          self.rtc_connection.addTracks(self.video_device_name)
          sdp = self.rtc_connection.getSDP()
          
          self.logger.info('Sending SDP')
          sdpValues = {'type': 'offer', 'sdp': json.loads(sdp)}            
          message = json.dumps(sdpValues)
          self.ws.send(message)
          
          self.logger.info('SDP sent')
        
        # wait until video and audio are ready?
        time.sleep(5) # for now, just wait 5 seconds
        self.ws.close()
        
        
    def _on_message(self):
        def f(ws, data):
                self.logger.info('Received: ' + data)
                parsedData = json.loads(data)

                if(parsedData['type'] == 'offer'):
                    answer = self._on_rtc_offer(parsedData['sdp']['sdp'])
                    sdpValues = {'type': 'answer', 'sdp': json.loads(answer)}
                    message = json.dumps(sdpValues)
                    self.ws.send(message)
                    self._send_candidate_information()

                elif(parsedData['type'] == 'answer'):
                    self._on_rtc_answer(parsedData['sdp']['sdp'])
                    self._send_candidate_information()

                elif(parsedData['type'] == 'candidate'):
                    candidate = parsedData['candidate']
                    self._on_rtc_candidate(json.dumps([candidate]))

                else:
                    error_message = 'Undefined message received on from signaling server. Shutting down websocket.'
                    self.logger.error(error_message)
                    raise Exception(error_message)
        return f
            
    def _on_rtc_offer(self, offer):
        self.logger.info('received an offer: ' + offer)
        return self.rtc_connection.receiveOffer(offer)

    
    def _on_rtc_answer(self, answer):
        self.logger.info('received an answer: ' + answer)
        self.rtc_connection.receiveAnswer(answer)

        
    def _on_rtc_candidate(self, candidate):
        self.logger.info('received a candidate: ' + candidate)
        self.rtc_connection.setICEInformation(candidate) 

        
    def _send_candidate_information(self):
        self.logger.info('sending candidate information')

        jsonICE = json.loads(self.rtc_connection.getICEInformation())
        for iceCandidate in jsonICE:
            candidateValue = {'type': 'candidate', 'candidate': iceCandidate}
            candidateMessage = json.dumps(candidateValue)
            self.ws.send(candidateMessage)
            self.logger.info('Message: ' + candidateMessage)

        self.logger.info('done! sending candidate information')


