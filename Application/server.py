#!/usr/bin/env python
from coapthon.server.coap import CoAP
from resources.registration import Registration
from resources.discovery import Discovery
from resources.clock import Clock

class CoAPServer(CoAP):
    def __init__(self, host, port):
        # inizialize the CoAP server
        CoAP.__init__(self, (host, port), multicast=False)
        # register available resources
        self.add_resource("/registration", Registration())
        self.add_resource("/discovery", Discovery())
        self.add_resource("/clock", Clock())
        print(f"CoAP Server started on {host}:{port}")
    
    
# ---------- Main ----------
if __name__ == "__main__":
    ip = "fd00::1"
    port = 5683

    server = CoAPServer(ip, port)
    try:
        server.listen(10)
    except KeyboardInterrupt:
        print("Server Shutdown")
        server.close()
        print("Exiting...")
