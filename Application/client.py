import sys
from util.DB import DBAccess
import json
from coapthon.client.helperclient import HelperClient
from tabulate import tabulate
from datetime import datetime, timedelta

# Max number of EVs
MAX_VEHICLES = 5

class EV:
    def __init__(self, ev_tuple):
        self.uid = ev_tuple[0]
        self.id = ev_tuple[1]
        self.battery = ev_tuple[2]
        self.priority = ev_tuple[3]
        self.charging = ev_tuple[4]

    def __str__(self):
        return f"Vehicle {self.uid} (ID: {self.id}) | Battery: {self.battery}% | Priority: {'High' if self.priority == 1 else 'Low'} | Charging: {'Yes' if self.charging == 1 else 'No'}"

class EVChargingApp:
    def __init__(self):
        #ip of actuator and sensor
        self.actuator_ip = None
        self.sensor_ip   = None
        
        self.db = DBAccess(
            host= "localhost",
            user= "root",
            password= "root",
            database= "EVChargingDB"
        )
        print("Searching for node IP...")
        # Find IP of actuator and sensor, iterate until they are not found
        while not (self.actuator_ip is not None and self.sensor_ip is not None):
            self.actuator_ip = self._fetch_node_ip("status")
            self.sensor_ip   = self._fetch_node_ip("energy")

        print("All nodes registered:")
        print(f"  Actuator @ {self.actuator_ip}")
        print(f"  Sensor   @ {self.sensor_ip}")

    def _fetch_node_ip(self, resource):
        """read from table nodes the IP of the node with resource= resource."""
        self.db.connect()
        row = self.db.query(
            "SELECT ip FROM nodes WHERE resource=%s",
            (resource,),
            True
        )
        self.db.close()
        if not row:
            return None
        return row[0][0]
        
    def _read_priorities(self):
        """read the JSON 'settings' from nodes and return priorities."""
        self.db.connect()
        row = self.db.query(
            "SELECT settings FROM nodes WHERE resource='status'",
            (),
            True
        )
        self.db.close()
        settings = json.loads(row[0][0])
        return settings.get("priorities", [-1]*MAX_VEHICLES)

    def _write_priorities(self, prio_list):
        """update settings column in nodes."""
        new_settings = json.dumps({"priorities": prio_list})
        self.db.connect()
        self.db.query(
            "UPDATE nodes SET settings = %s WHERE resource='status'",
            (new_settings,)
        )
        self.db.close()
    #send a put request
    def _coap_put(self, ip, path, query=None, payload=None):
        c = HelperClient(server=(ip, 5683))
        if payload is not None:
            res = c.put(path, payload)
        else:
            res = c.put(f"{path}?{query}", "")
        c.stop()
        return res
    #read all vehicles
    def fetch_vehicles(self):
        self.db.connect()
        result = self.db.query("SELECT uid, id, battery, priority, charging FROM vehicles", (), True)
        self.db.close()
        return [EV(row) for row in result] if result else []
    #get the last 24 hours energy values
    def get_energy(self):
        self.db.connect()
        res = self.db.query("SELECT MAX(timestamp) FROM energy", (), True)
        if not res or res[0][0] is None:
            print("No energy data available in the database.")
            self.db.close()
            return

        # res[0][0] is a datetime object
        max_ts = res[0][0]
        time_limit = max_ts - timedelta(hours=24)
        query = """
            SELECT timestamp, predicted, sampled
            FROM energy
            WHERE timestamp BETWEEN %s AND %s
            ORDER BY timestamp ASC
        """
        records = self.db.query(query, (time_limit, max_ts), True)
        self.db.close()
        if records:
            print(tabulate(records, headers=["Timestamp", "Predicted Energy", "Sampled Energy"], tablefmt="psql"))
        else:
            print("No energy data available.")
    #shows nodes table
    def show_nodes(self):
        self.db.connect()
        result = self.db.query("SELECT ip, name, resource, settings FROM nodes", (), True)
        self.db.close()
        
        if result:
            print("\n--- Registered Nodes ---")
            print(tabulate(result, headers=["IP", "Name", "Resource", "Settings"], tablefmt="psql"))
        else:
            print("No nodes found in the database.")
    #show station status
    def show_status(self):
        vehicles = self.fetch_vehicles()
        print("\n--- Vehicle Status ---")
        for v in vehicles:
            print(v)            #__str__ method of EV
        print()
    #get the <id> priority or all priorities
    def get_priority(self, vid=None):
        
        query = f"id={vid}" if vid is not None else ""

        print(f"[DEBUG] Actuator IP = {self.actuator_ip!r}")
        try:
            c = HelperClient(server=(self.actuator_ip, 5683))
            if vid is not None:
                response = c.get("settings", uri_query=f"id={vid}")         #get with <id> parameter
            else:
                response = c.get("settings")                                #get without parameter
            c.stop()
        except Exception as e:
            import traceback; traceback.print_exc()
            print(f"Error creating or using CoAP client: {e}")
            return

        if response and response.code == 69:
            raw = response.payload
            if isinstance(raw, bytes):
                raw = raw.decode()
            try:
                data = json.loads(raw)
            except Exception as e:
                print(f"Error parsing the JSON response: {e}")
                print(f"Raw payload was: {raw!r}")
                return

            #response with full array
            if "priorities" in data:
                priorities = data["priorities"]
                #read priorities from the DB
                db_priorities = self._read_priorities()
                #update DB if data are different
                if priorities != db_priorities:
                    self._write_priorities(priorities)
                    print("Priorities updated in database.")
                print("Priorities:")
                for idx, prio in enumerate(priorities):
                    print(f"EV {idx}: {'High' if prio == 1 else 'Low'}")
            
            #response with only one element
            elif "priority" in data:
                if 0 <= vid < MAX_VEHICLES:
                    #read priorities from the DB
                    priorities = self._read_priorities()
                    #update the element if it is different
                    if priorities[vid] != data["priority"]:
                        priorities[vid] = data["priority"]
                        #update DB
                        self._write_priorities(priorities)
                        print("Priorities updated in database.")

                    print(f"Priority of {vid}: {'High' if priorities[vid] == 1 else 'Low'}")
                else:
                    print("Not valid ID.")
            else:
                print("Error: response format not valid.")
        else:
            print("Error: impossible get priorities from the actuator.")

    #set the <id> priority with new_prio
    def set_priority(self, vid, new_prio):
        if vid < 0 or vid >= MAX_VEHICLES or new_prio not in (0,1):
            print("Usage: set-priority <0..4> <0|1>")
            return
        # 1) update DB
        prio = self._read_priorities()
        prio[vid] = new_prio
        self._write_priorities(prio)
        print(f"DB: priority[{vid}] aggiornata a {new_prio}")

        # 2) update actuator via CoAP (PUT request)
        print(f"[DEBUG] PUT to Actuator IP = {self.actuator_ip!r}, query=id={vid}&priority={new_prio}")
        try:
            resp = self._coap_put(self.actuator_ip, "settings", query=f"id={vid}&priority={new_prio}")
        except Exception as e:
            import traceback; traceback.print_exc()
            print(f"Error sending CoAP PUT: {e}")
            return
        if resp and resp.code == 68:
            print("Actuator: priority updated with success")
        else:
            print(f"Actuator: priority update failed (code={getattr(resp,'code',None)})")
    #read sampling rate from DB
    def _read_sampling(self):
        self.db.connect()
        row = self.db.query(
            "SELECT settings FROM nodes WHERE name='Sensor'",
            (), 
            True
        )
        self.db.close()
        return json.loads(row[0][0]).get("sampling_period")
    #write sampling rate into the database
    def _write_sampling(self, minutes):
        new_s = json.dumps({"sampling_period": minutes})
        self.db.connect()
        self.db.query("UPDATE nodes SET settings=%s WHERE name='Sensor'",
                      (new_s,))
        self.db.close()

    def get_sampling_period(self):
        sp = self._read_sampling()
        print(f"Sampling period = {sp} minutes")
    #update sampling period in the DB and in the sensor node via PUT request
    def set_sampling_period(self, minutes):
        if minutes not in (15,30,45,60):
            print("Uso: set-sampling-period <15|30|45|60>")
            return
        # aggiorna DB
        self._write_sampling(minutes)
        print(f"DB: sampling_period = {minutes}")
        # aggiorna nodo Sensor via CoAP PUT con payload JSON
        payload = json.dumps({"sampling period": minutes})
        print(f"[DEBUG] PUT to Sensor IP = {self.sensor_ip!r}, payload={payload}")
        try:
            resp = self._coap_put(self.sensor_ip, "settings", payload=payload)
        except Exception as e:
            import traceback; traceback.print_exc()
            print(f"Error sending CoAP PUT to sensor: {e}")
            return
        if resp and resp.code == 68:
            print("Sensor: aggiornamento sampling_period riuscito")
        else:
            print(f"Sensor: aggiornamento sampling_period FALLITO (code={getattr(resp,'code',None)})")


    def run(self):

        print("Welcome to EV Charging CLI")
        self.show_help()

        while True:
            try:
                command = input("EV> ").strip().lower()

                if command == 'status':
                    self.show_status()

                elif command == 'show-nodes':
                    self.show_nodes()

                elif command == 'help':
                    self.show_help()
                
                elif command.startswith("get-priority"):
                    parts = command.split()
                    if len(parts) == 2:
                        self.get_priority(int(parts[1]))
                    else:
                        self.get_priority()

                elif command.startswith("set-priority"):
                    parts = command.split()
                    if len(parts) == 3:
                        try:
                            vid = int(parts[1])
                            prio = parts[2]
                            self.set_priority(vid, int(prio))
                            print(f"Priority for Vehicle {vid} set to {prio.upper()}.")
                        except ValueError:
                            print("Invalid vehicle ID.")
                    else:
                        print("Usage: set-priority <id> <high/low>")
                
                elif command == "get-sampling-period":
                    self.get_sampling_period()
                elif command.startswith("set-sampling-period"):
                    parts = command.split()
                    if len(parts) == 2 and parts[1].isdigit():
                        self.set_sampling_period(int(parts[1]))
                    else:
                        print("Usage: set-sampling-period <15|30|45|60>")

                elif command == 'get-energy':
                    self.get_energy()

                elif command == 'exit':
                    print("Exiting...")
                    break

                else:
                    print("Unknown command. Type 'help' to list commands.")

            except KeyboardInterrupt:
                print("\nInterrupted. Type 'exit' to quit.")
            except Exception as e:
                print(f"Error: {e}")

    def show_help(self):
        print("\nAvailable Commands:")
        print("status                               Show status of all vehicles")
        print("show-nodes                           Show all registere nodes")
        print("set-priority <id> <0/1>              Set priority for a vehicle")
        print("get-priority <id>                    Show priority (all or single)")
        print("get-sampling-period                  Show the sampling period")
        print("set-sampling-period <15|30|45|60>    Set the sampling period")  
        print("get-energy                           Show energy logs (last 24 hours)")
        print("exit                                 Exit the program\n")

if __name__ == '__main__':
    app = EVChargingApp()
    app.run()
