from coapthon.resources.resource import Resource
from datetime import datetime

class Clock(Resource):
    def __init__(self, name="CoapClock"):
        # Initialize the non-observable CoAP resource with the predefined name
        super(Clock, self).__init__(name, observable=False)
        #Set the payload with the current date and time in ISO 8601 format.
        self.payload = datetime.now().strftime("%Y-%m-%dT%H:%MZ")

    def render_GET(self, request):
        """
        Handles GET requests on the /clock resource. Always responds with the current time in the format: "YYYY-MM-DDTHH:MMZ"
        """
        # Create a new response for the Clock resource
        res = Clock()
       
        res.location_query = request.uri_query
        #Update the payload with the current time
        res.payload = datetime.now().strftime("%Y-%m-%dT%H:%MZ")
        print(f"[Clock] Request time by {request.source[0]}, sending: {res.payload}")
        return res