import pywebrtc._ext.pywebrtc as pywebrtc_wrapper

class Connection:

    def __init__(self, message):

        self.conn = pywebrtc_wrapper.PyWebRTCConnection()
        self.message = message


    def get_sdp(self):

        return self.conn.getSDP()


    def say_message(self):
    
        self.conn.start()
        self.conn.stop()
        print(self.message)