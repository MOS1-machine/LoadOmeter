import time
import serial
import requests
import json
import re
import sys

COM_PORT = 'COM24'
BAUD_RATE = 9600
UPDATE_INTERVAL = 1.0
OHM_URL = 'http://localhost:8085/data.json'

def find_sensor_value_recursive(data_node, sensor_text_partial, hardware_text_partial, found_hardware=False):
    node_text = data_node.get('Text', '')
    node_value_str = data_node.get('Value', '')

    is_target_hardware = hardware_text_partial in node_text

    if found_hardware or is_target_hardware:
        found_hardware = True

        if sensor_text_partial in node_text and node_value_str:
            match = re.search(r'(\d+\.?\d*)', node_value_str)
            if match:
                try:
                    value = int(float(match.group(1)))
                    return value
                except (ValueError, TypeError):
                    pass

    if 'Children' in data_node:
        for child in data_node['Children']:
            result = find_sensor_value_recursive(
                child,
                sensor_text_partial,
                hardware_text_partial,
                found_hardware
            )
            if result is not None:
                return result

    return None


def get_normalized_data():
    try:
        response = requests.get(OHM_URL)
        response.raise_for_status()
        data = response.json()

        # CPU
        cpu_temp = find_sensor_value_recursive(data, 'CPU Package', 'AMD Ryzen 5 7600')
        cpu_load = find_sensor_value_recursive(data, 'CPU Total', 'AMD Ryzen 5 7600')

        # GPU
        gpu_temp = find_sensor_value_recursive(data, 'GPU Core', 'Temperatures')
        gpu_load = find_sensor_value_recursive(data, 'GPU Core', 'Load')

        # RAM
        ram_usage = find_sensor_value_recursive(data, 'Memory', 'Generic Memory')

        cpu_temp = cpu_temp if cpu_temp is not None else 0
        cpu_load = cpu_load if cpu_load is not None else 0
        gpu_temp = gpu_temp if gpu_temp is not None else 0
        gpu_load = gpu_load if gpu_load is not None else 0
        ram_usage = ram_usage if ram_usage is not None else 0

        return {
            'CPU_TEMP': max(0, min(100, cpu_temp)),
            'GPU_TEMP': max(0, min(100, gpu_temp)),
            'CPU_LOAD': max(0, min(100, cpu_load)),
            'GPU_LOAD': max(0, min(100, gpu_load)),
            'RAM_USAGE': max(0, min(100, ram_usage))
        }

    except requests.exceptions.RequestException as e:
        print(f"no OHM Web Server. 8085. {e}")
        return None
    except json.JSONDecodeError:
        print("no JSON")
        return None


def main():
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0)
        print(f"on {COM_PORT} @ {BAUD_RATE}.")
    except serial.SerialException as e:
        print(f"no-Serial: {e}")
        print("COM_PORTArduino")
        sys.exit(1)

    print("---------------------------------")
    print("CPU_TEMP, GPU_TEMP, RAM_USAGE, CPU_LOAD, GPU_LOAD")

    try:
        while True:
            start_time = time.time()
            data = get_normalized_data()

            if data:
                serial_data = (
                    f"{data['CPU_TEMP']},"
                    f"{data['GPU_TEMP']},"
                    f"{data['RAM_USAGE']},"
                    f"{data['CPU_LOAD']},"
                    f"{data['GPU_LOAD']}\n"
                )

                ser.write(serial_data.encode('ascii'))
                print(f"OUT: {serial_data.strip()}")
            else:
                print("waiting_data_OHM...")

            elapsed_time = time.time() - start_time
            sleep_time = UPDATE_INTERVAL - elapsed_time
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\nuser_stop.")
    except Exception as e:
        print(f"\nError: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial exit.")


if __name__ == "__main__":
    main()
