from coapthon.resources.resource import Resource
from coapthon.client.helperclient import HelperClient
import coapthon.defines as defines
from util.DB import DBAccess

class Discovery(Resource):

    def __init__(self, name="CoAP Discovery"):
        # Initialize the CoAP resource as non-observable
        super(Discovery, self).__init__(name, observable=False)
        self.payload = ""

    def check_resource(self, host, port, resource):
        """
        Check if a certain CoAP resource is available and the node is active. Make a GET request to the node and check the response.
        """
        client = HelperClient(server=(host, port))
        response = client.get(resource)
        client.stop()

        if response is None or response.code != defines.Codes.CONTENT.number:
            return False
        else:
            return True

    def render_GET(self, request):
        """
        Handles a GET request on /discovery: 
        - Receives the name of the resource in the payload 
        - Queries the DB to find the node that offers that resource 
        - Checks if the node is active by querying the resource 
        - Returns the node's IP if everything is valid
        """
        res = Discovery()
        res.location_query = request.uri_query

        print(f"Discovery: Request for resource: {request.payload}")

        database = DBAccess(
            host= "localhost",
            user= "root",
            password= "root",
            database= "EVChargingDB"
        )

        if database.connect() is None:
            print("Discovery: Connection to the database failed")
            return None

        # Verify the existence of the resource in the DB
        query = "SELECT COUNT(*) FROM nodes WHERE resource = %s"
        val = (request.payload,)
        node_ip = database.query(query, val, True)

        if node_ip is None:
            print("Discovery: Error in the resource verification query")
            database.close()
            return None
        elif node_ip[0][0] <= 0:
            print(f"Discovery: Respurce not found: {request.payload}")
            database.close()
            return None

        # Retrieve the IP of the node in which there is the resource
        query = "SELECT ip FROM nodes WHERE resource = %s"
        node_ip = database.query(query, val, True)

        if node_ip is None:
            print("Discovery: Error during IP search")
            database.close()
            return None

        # Verify that the node and the resource are active
        ip = node_ip[0][0]
        print(f"Discovery: Verify availability of the resource '{request.payload}' on {ip}:5683")
        if not self.check_resource(ip, 5683, request.payload):
            print("Discovery: Node or resource not active")
            database.close()
            return None

        database.close()
        print(f"Discovery: Resource available on IP: {ip}")
        res.payload = ip

        return res