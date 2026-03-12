from coapthon.resources.resource import Resource
import json
from jsonschema import validate, ValidationError
from util.DB import DBAccess
from observer import Observer

class Registration(Resource):

    def __init__(self, name="Node Registration"):
        # Initialize the CoAP resource as non-observable
        super(Registration, self).__init__(name, observable=False)
        self.payload = "Registered"  # default response

    def render_POST(self, request):
        """manages the POST request on /registration:"""
        res = Registration()

        # JSON schema contains: 'node', 'resource' and 'settings'
        json_register_schema = {
            "node": {"type": "string"},
            "resource": {"type": "string"},
            "settings": {"type": "string"}
        }

        res.location_query = request.uri_query
        print(f"Registration: POST request received from: {request.source[0]}")

        # Verify JSON payload validity
        try:
            node_info = json.loads(request.payload)
            validate(instance=node_info, schema=json_register_schema)
        except Exception as e:
            if isinstance(e, json.JSONDecodeError):
                print(f"Registration: Failed to decode JSON: {e.msg}")
                print(f"\t- Raw payload: {request.payload}")
            elif isinstance(e, ValidationError):
                print(f"Registration: Invalid JSON schema: {e.message}")
                print(f"\t- BAD JSON content: {node_info if 'node_info' in locals() else 'N/A'}")
            else:
                print(f"Registration: Unexpected error while parsing JSON: {e}")
            return None

        print(f"Registration: Node registration data validated for: {node_info['node']}")

        # Query SQL to insert and update table `nodes`
        query = "REPLACE INTO nodes (ip, name, resource, settings) VALUES (%s, %s, %s, %s)"
        val = (
            request.source[0],
            node_info["node"],
            str(node_info["resource"]),
            str(node_info["settings"])
        )
        print(f"Query values: {val}")

        database = DBAccess(
            host= "localhost",
            user= "root",
            password= "root",
            database= "EVChargingDB"
        )
        
        if database.connect() is None:
            print("Registration: Failed to connect to the database.")
            return None

        ret = database.query(query, val, False)

        database.close()

        if ret is None:
            print("Registration: Failed to insert node information into the database.")
            return None

        print(f"Registration: Node info inserted: {val}")

        # start automatic observing of the resource
        #observed resources: energy, status
        print(f"Registration: Starting observer for resource '{node_info['resource']}' on node {request.source[0]}")
        observer = Observer(request.source[0], node_info["resource"])
        observer.start()

        return res
