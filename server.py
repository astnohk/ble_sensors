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

current_data = {}

class BLEScanner:
    def __init__(self):
        self.current_data = {}

    async def update_grid_eye(self):
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
                if 0xffff in advertise_data.manufacturer_data.keys():
                    bytes = advertise_data.manufacturer_data[0xffff]
                    thermal_data = [0 for i in range(len(bytes))]
                    for i in range(len(bytes)):
                        thermal_data[i] = int(bytes[i])
                    current_data[device.address] = {
                        "name": device.name,
                        "timestamp": now.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
                        "thermal_data": thermal_data,
                    }
        except Exception as err:
            logger.error("Failed to read advertisement data.")
            logger.error(err)

async def update_sensors(scanner: BLEScanner):
    while True:
        await scanner.update_grid_eye()
        await asyncio.sleep(5)

def start_ble_receiver(scanner: BLEScanner):
    asyncio.run(update_sensors(scanner))

@app.route("/")
def root():
    return "sensor."

@app.route("/api/get-grid-eyes")
def grid_eyes():
    tmp = copy.deepcopy(current_data)
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
