from coapthon.client.helperclient import HelperClient
from util.DB import DBAccess
from datetime import datetime
import coapthon.defines as defines
import json

class EnergyData:
    def __init__(self):
        # accuracy used to normalized values
        self._decimal_accuracy = 10000
        # data format
        self._date_format = "%Y-%m-%dT%H:%MZ"
        # contains decoded JSON
        self.json = None
        
    def initialize(self, json_str):
        """
        Decode the SenML in a Python list,
        normalize numbers > _decimal_accuracy,
        remove negative values.
        True if there is an error, False if is OK.
        """
        try:
            # convert JSON string in a Python object
            self.json = json.loads(json_str)
        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
            return True
        except Exception as e:
            print("BAD JSON:", e)
            return True
        
        for i in range(len(self.json)):
            # normalize value
            if 'v' not in self.json[i]:
                continue
            
            self.json[i]['v'] = self.json[i]['v'] / self._decimal_accuracy
            # error if the value is negative
            if self.json[i]['v'] < 0:
                print("Data received is <0, skipping...")
                return True
        
        return False
    
    def get_datetime(self, index):
        """
        Return an object datetime for the reconrd in `index` on the list JSON.
        Return None if the JSON is not inizialized or the date is not valid.
        """
        if self.json is None:
            print("JSON is None")
            return None
        try:
            # Convert the data string into datetime format
            return datetime.strptime(self.json[index]['t'], self._date_format)
        except Exception as e:
            print(f"Failed to parse the date: {e}")
            return None
        
class StatusData:
    def __init__(self):
        self.json = None

    def initialize(self, json_str):
        try:
            parsed = json.loads(json_str)
            if isinstance(parsed, dict) and "vehicles" in parsed and isinstance(parsed["vehicles"], list):
                self.json = parsed["vehicles"]
                return False
            else:
                print("JSON does not contain a list of 'vehicles'")
                return True
        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
            return True


class Observer:
    def __init__(self, ip, resource):
        
        self.client = HelperClient(server=(ip, 5683))
        self.resource = resource  # CoAP resource to observe (es. 'energy')
        self.last_response = None # Ultima risposta ricevuta dal nodo osservato

    def check_and_update_energy(self, data, db):
        """
        Inserts or updates both values ('predicted' and 'sampled') in the database related to the timestamp contained in `data`. 
        It is assumed that: 
        - data.json[0] is 'predicted' 
        - data.json[1] is 'sampled'.
        """
        # Query to check if exists a record for that hour
        search_query = "SELECT COUNT(*) FROM energy WHERE timestamp = %s"
        # Query to insert both energy vaules
        insert_query = "INSERT INTO energy(timestamp, predicted, sampled) VALUES (%s, %s, %s)"
        # Query to update energy
        update_query = "UPDATE energy  SET predicted = %s, sampled = %s WHERE timestamp = %s"
                          
        # extract the timestamp from one of the two, they are referred to the same hour
        time = data.get_datetime(1)
        if time is None:
            print("Failed to get the timestamp from the data")
            return True
        
        db_timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        val_check = (db_timestamp,)

        # Check if there is already a record
        result = db.query(search_query, val_check, True)
        if result is None:
            print("Failed to query the database")
            return True

        predicted_val = data.json[1]["v"]
        sampled_val = data.json[2]["v"]

        if result[0][0] != 0:
            # update both values
            val_update = (predicted_val, sampled_val, db_timestamp )
            db.query(update_query, val_update, False)
        else:
            # insert both values
            val_insert = (db_timestamp, predicted_val, sampled_val)
            db.query(insert_query, val_insert, False)

        return False
    
    def check_and_update_status(self, data, db):
        """
        It replaces all the records in the vehicles table with the 5 received vehicles, assigning uid from 0 to 4.
        """
        if len(data.json) != 5:
            print("Errore: attesi esattamente 5 veicoli")
            return True

        # Total cancellation query
        delete_query = "DELETE FROM vehicles"

        # Query for insertion
        insert_query = """INSERT INTO vehicles (uid, id, battery, priority, charging)
                        VALUES (%s, %s, %s, %s, %s)"""

        db.query(delete_query, None, False)

        for uid, entry in enumerate(data.json):
            vehicle_id = entry.get("id")
            battery = entry.get("battery")
            priority = entry.get("priority")
            charging = entry.get("status")

            values = (uid, vehicle_id, battery, priority, charging)
            db.query(insert_query, values, False)

        return False

    def callback_observe(self, response):
        """
        Method called on each NOTIFY from the observed CoAP node. Parses the JSON payload, opens the connection to the DB, and updates the data.
        """
        print(f"[LOG] Callback Observe triggered for resource '{self.resource}'")
        
        # Check that the response is valid and contains data (CONTENT code)
        if response is None or response.code != defines.Codes.CONTENT.number:
            print("Response is None or not content")
            return None

        # Converts payload to str for debugging
        raw = response.payload
        if isinstance(raw, bytes):
            try:
                raw = raw.decode()
            except Exception as e:
                print(f"[ERROR] Payload decoding failed: {e}")
                pass
        print(f"[LOG] Response code: {response.code}")
        print(f"[LOG] Payload length: {len(response.payload) if response.payload else 0}")
        print(f"[DEBUG] Observe '{self.resource}' payload: {raw!r}")

        database = DBAccess(
            host= "localhost",
            user= "root",
            password= "root",
            database= "EVChargingDB"
        )
        
        if database.connect() is None:
            return None
        
        # If the observed resource is 'energy', update the 'sampled' and 'predicted' values
        if self.resource == 'energy':
            data = EnergyData()
            if data.initialize(raw):
                print("[ERROR] EnergyData.initialize failed")
                database.close()
                return None
            if self.check_and_update_energy(data, database):
                print("[ERROR] check_and_update_energy failed")
                database.close()
                return None
            print("Updated energy")
        
        elif self.resource == 'status':
            data = StatusData()
            if data.initialize(response.payload) is True or self.check_and_update_status(data, database):
                database.close()
                return None
            print("Updated vehicle status")

        #Store the last received answer
        self.last_response = response
        database.close()

    def start(self):
        """Start the CoAP observation on the specified resource."""
        self.client.observe(self.resource, self.callback_observe)
    
    def stop(self):
        """Deletes the observation and closes the client."""
        self.client.cancel_observing(self.last_response)
        self.client.stop()
