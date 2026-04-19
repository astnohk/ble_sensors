import asyncio
import copy
import datetime
import logging
import threading

import bleak

from flask import Flask, jsonify

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

app = Flask(__name__)

current_data = {
    "temp-humid": {},
    "grid-eyes": {},
}
tmp = {}

class BLEScanner:
    def __init__(self):
        self.current_data = {}

    async def update(self):
        try:
            devices = await bleak.BleakScanner.discover(
                return_adv=True,
            )
        except Exception as err:
            logger.error("Failed to scan devices.")
            logger.error(err)
            return
        now = datetime.datetime.now(tz=datetime.timezone.utc)
        try:
            for device, advertise_data in devices.values():
                if 0xf4ff in advertise_data.manufacturer_data.keys():
                    bytes = advertise_data.manufacturer_data[0xf4ff]
                    if len(bytes) == 4:
                        temperature = ((bytes[0] << 8) | bytes[1]) / 4.0
                        humidity = ((bytes[2] << 8) | bytes[3]) / 4.0
                        current_data["temp-humid"][device.address] = {
                            "name": device.name,
                            "timestamp": now.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
                            "temperature": temperature,
                            "humidity": humidity,
                        }
                elif 0xffff in advertise_data.manufacturer_data.keys():
                    bytes = advertise_data.manufacturer_data[0xffff]
                    if len(bytes) == 16:
                        thermal_data = [0 for i in range(len(bytes))]
                        for i in range(len(bytes)):
                            thermal_data[i] = int(bytes[i])
                        current_data["grid-eyes"][device.address] = {
                            "name": device.name,
                            "timestamp": now.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
                            "thermal_data": thermal_data,
                        }
                elif 0xfeff in advertise_data.manufacturer_data.keys():
                    # Header of sequential data sending
                    bytes = advertise_data.manufacturer_data[0xfeff]
                    if (len(bytes) >= 6 and
                        bytes[1:5] == b"CRC\x00"
                    ):
                        packet_length = int(bytes[0])
                        tmp[device.address] = {
                            "timestamp": now.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
                            "name": device.name,
                            "packet_length": packet_length,
                            "crc": int(bytes[5]),
                            "packets": [[] for _ in range(packet_length)],
                        }
                if device.address in tmp.keys():
                    # Maybe a packet of sequential data sending
                    keys = sorted(list(advertise_data.manufacturer_data.keys()))
                    for key in keys:
                        if ((key & 0x8000) > 0 or
                            (key & 0x00ff) != 0x00ff
                        ):
                            # Skip
                            continue
                        index = (key & 0xff00) >> 8
                        bytes = advertise_data.manufacturer_data[key]
                        ## Parse bytes
                        data = [0 for i in range(len(bytes))]
                        for i in range(len(bytes)):
                            data[i] = int(bytes[i])
                        ## Append to tmp
                        tmp[device.address]["packets"][index] = data
                    ## Move completed tmp data to current_data
                    if len(tmp[device.address]["packets"]) == tmp[device.address]["packet_length"]:
                        thermal_data = []
                        for d in tmp[device.address]["packets"]:
                            thermal_data.extend(d)
                        current_data["grid-eyes"][device.address] = {
                            "name": device.name,
                            "timestamp": now.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
                            "thermal_data": thermal_data,
                        }
                        ## Clear tmp cache
                        del tmp[device.address]
        except Exception as err:
            logger.error("Failed to read advertisement data.")
            logger.error(err)

async def update_sensors(scanner: BLEScanner):
    while True:
        # Scan all BLE advertisement
        await scanner.update()

        # Clear old data
        now = datetime.datetime.now(tz=datetime.timezone.utc)
        for key in current_data["temp-humid"].keys():
            timestamp = datetime.datetime.fromisoformat(
                current_data["temp-humid"][key]["timestamp"])
            dt = now - timestamp
            if dt.total_seconds() > 60 * 60 * 24:
                # Delete old non-updated data
                del current_data["temp-humid"][key]
        for key in current_data["grid-eyes"].keys():
            timestamp = datetime.datetime.fromisoformat(
                current_data["grid-eyes"][key]["timestamp"])
            dt = now - timestamp
            if dt.total_seconds() > 60 * 60 * 24:
                # Delete old non-updated data
                del current_data["temp-humid"][key]

        # Wait until next update
        await asyncio.sleep(1)

def start_ble_receiver(scanner: BLEScanner):
    asyncio.run(update_sensors(scanner))

@app.route("/")
def root():
    return "sensor."

@app.route("/api/get-temp-humid")
def temp_humid():
    tmp = copy.deepcopy(current_data["temp-humid"])
    return jsonify(tmp)

@app.route("/api/get-grid-eyes")
def grid_eyes():
    tmp = copy.deepcopy(current_data["grid-eyes"])
    return jsonify(tmp)

# Scanner threads
scanner = BLEScanner()
bleth = threading.Thread(
    target=start_ble_receiver,
    args=(scanner,),
    daemon=True,
)
bleth.start()

if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=False)
