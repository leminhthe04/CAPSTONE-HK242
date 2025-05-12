import requests
import json
import time
import datetime


# The JWT token for authentication
JWT_TOKEN = "Bearer eyJhbGciOiJIUzUxMiJ9.eyJzdWIiOiJiYW9raGFuMzI3QGdtYWlsLmNvbSIsInVzZXJJZCI6IjRjMjc0N2YwLTExMWEtMTFmMC05MDkyLTJmNzIzNzkzODFiZSIsInNjb3BlcyI6WyJURU5BTlRfQURNSU4iXSwic2Vzc2lvbklkIjoiYzY0YWI2NDUtMWQwOS00N2Y1LWFjOWEtNmRjZDQzMmEwNWZhIiwiZXhwIjoxNzQ0ODQyNzgyLCJpc3MiOiJ0aGluZ3Nib2FyZC5jbG91ZCIsImlhdCI6MTc0NDgxMzk4MiwiZmlyc3ROYW1lIjoiTEUgTUlOSCIsImxhc3ROYW1lIjoiVEhFIiwiZW5hYmxlZCI6dHJ1ZSwiaXNQdWJsaWMiOmZhbHNlLCJpc0JpbGxpbmdTZXJ2aWNlIjpmYWxzZSwicHJpdmFjeVBvbGljeUFjY2VwdGVkIjp0cnVlLCJ0ZXJtc09mVXNlQWNjZXB0ZWQiOnRydWUsInRlbmFudElkIjoiNGJjZDY4MjAtMTExYS0xMWYwLTkwOTItMmY3MjM3OTM4MWJlIiwiY3VzdG9tZXJJZCI6IjEzODE0MDAwLTFkZDItMTFiMi04MDgwLTgwODA4MDgwODA4MCJ9.NdTo5kCgh7UfM92FBRx1iBfu_OsOxXkRc77Ffrk2eT_Jy4xTgjvQwYmS9t7UxRsYpbOUP-24nbFtXGLes3FSTw"

# Thingsboard device ID which is storing the sensor data
DEVICE_ID = "7a84a2a0-111a-11f0-b4bd-8b00c50fcae5"


# You can use this URL with GET method and headers, params in fetchData() function (below)
# to get data into your application
URL = f"https://thingsboard.cloud/api/plugins/telemetry/DEVICE/{DEVICE_ID}/values/timeseries"


# All the data keys to be retrieved
# The keys should be in the format "key_name(unit)"
keys = [
    "humidity(%)",
    "light_level(lux)",
    "rain_level(%)",
    "soil_moisture_1(%)", 
    "soil_moisture_2(%)", 	 
    "temperature(°C)",
    "water_level(%)"
]


def formatData(data):
    """
    Sort the data by timestamp and convert the timestamp to readable format.
    """

    def convertTimestamp2Datetime(ts):
        return datetime.datetime.fromtimestamp(int(ts) / 1000).strftime('%Y-%m-%d %H:%M:%S')

    # Sort the data by timestamp
    for key, values in data.items():
        data[key] = sorted(values, key=lambda x: x['ts'])

    # Convert the timestamp to datetime format
    for key in keys:
        if key in data:
            data[key] = list(map(lambda x: {'timestamp': convertTimestamp2Datetime(x['ts']), 
                                            'value': x['value']}, 
                                            data[key]))
            

def fetchData(isSerial=False, **kwargs):
    """
    Fetch data from the Thingsboard API.
    Function has two modes (based on the isSerial parameter):
    1. Non-serial mode: Fetch the latest data for the specified keys.
        Need: outputFile - a .json file to save the data.
        Example: fetchData(outputFile="latest_data.json")
    2. Serial mode: Fetch data for the specified keys within a time range.
        Need: start_time, end_time, interval, limit, outputFile.
        Example: fetchData(start_time=..., end_time=..., interval=..., limit=..., outputFile="serial_data.json")
    """


    if not kwargs.get("outputFile"):
        raise ValueError("outputFile is required")

    headers = {
        "Content-Type": "application/json",
        "X-Authorization": JWT_TOKEN,
    }

    # if not isSerial, just get the latest data
    params = {
        "keys": ",".join(keys),
        "agg": "NONE",
    }

    if isSerial:
        if not kwargs.get("start_time") or not kwargs.get("end_time"):
            raise ValueError("start_time and end_time are required for serial data retrieval")
        if not kwargs.get("interval"):
            raise ValueError("interval is required for serial data retrieval")
        if not kwargs.get("limit"):
            raise ValueError("limit is required for serial data retrieval")

        params.update({
            "interval": kwargs["interval"],
            "startTs": kwargs["start_time"],
            "endTs": kwargs["end_time"],
            "limit": kwargs["limit"],  
        })

    response = requests.get(URL, headers=headers, params=params)

    if response.status_code == 200:
        data = response.json()
        formatData(data)
        with open(kwargs["outputFile"], "w", encoding="utf-8") as f:
            f.write(json.dumps(data, indent=4, ensure_ascii=False))
        # print(json.dumps(data, indent=4, ensure_ascii=False))
    else:
        print(f"Error: {response.status_code}")
        print(response.text)



if __name__ == "__main__":
    # Fetch data in non-serial mode (latest data)
    fetchData(isSerial=False, outputFile="latest_data.json")

    # Set the start and end time for the data retrieval
    end_time = int(time.time() * 1000) # Current time in milliseconds
    start_time = end_time - 30*24*60*60*1000 # 30 days ago in milliseconds
    # The sensors send data every 2 seconds, set the interval to 2000 milliseconds
    interval = 2000
    limit = 4
    # Fetch data in serial mode
    fetchData(isSerial=True, start_time=start_time, end_time=end_time, interval=interval, limit=limit, outputFile="serial_data.json")