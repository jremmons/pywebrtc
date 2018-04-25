import pywebrtc._ext.pywebrtc as pywebrtc_wrapper
import websocket
import json
try:
    import thread
except ImportError:
    import _thread as thread
import time


class Connection:
    
    def __init__(self, type_, id_, signalingServer):
        self.conn = pywebrtc_wrapper.PyWebRTCConnection()
        self.ws = websocket.WebSocketApp(signalingServer, 
          on_message=self.on_message,
          on_error=self.on_error,
          on_close=self.on_close)
        self.ws.on_open = self.on_open
        self.type = type_
        self.id = {"type": "kind", "kind": type_, "connection_id": id_}

    def onOffer(self, offer):
        print("Received an offer: " + offer)
        return receiveOffer(offer)

    def onAnswer(self, answer):
        print("Received an answer: " + answer)
        self.conn.receiveAnswer(answer)

    # TODO
    def onCandidate(self, candidate):
        print ("Candidate: " + candidate)

    def on_message(self, ws, data):
        print("Received: " + data)
        parsedData = json.loads(data)

        if(parsedData['type'] == "offer"):
            answer = self.onOffer(parsedData['sdp']['sdp'])
            sdpValues = {"type": "answer", "sdp": json.loads(answer)}
            message = json.dumps(sdpValues)
            self.ws.send(message)
            print("Answer Sent!")
        elif(parsedData['type'] == "answer"):
            self.onAnswer(parsedData['sdp']['sdp'])
        elif(parsedData['type'] == "candidate"):
            self.onCandidate(data)
        else:
            print("Undefined Message. Shutting down websocket.")
            self.ws.close()

    def on_error(self, ws, error):
        print("Error: ")
        print(error)

    def on_close(self, ws):
        print("Websocket Closed")

    def on_open(self, ws):
        print("Websocket Open")
        def run(*args):
            # Send information about ourselves
            print("Sending Kind")
            message = json.dumps(self.id) 
            print("Message: "+message)
            self.ws.send(message)
            print("Kind sent!")
            
            print(self.type)
            if(self.type == "client"):
                print("Hello")
                # Get sdp information and send offer
                sdp = self.conn.getSDP()
                print("Sending SDP")
                sdpValues = {"type": "offer", "sdp": json.loads(sdp)}            
                message = json.dumps(sdpValues)
                print("Message: "+message)
                self.ws.send(message)
                print("SDP Sent!")
                
        thread.start_new_thread(run, ())

    
    def run_websocket(self):
        #websocket.enableTrace(True)
        
        self.ws.run_forever()



