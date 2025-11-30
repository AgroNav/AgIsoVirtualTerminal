import asyncio
from bleak import BleakClient, BleakScanner, BleakError

# Nordic UART Service (NUS)
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e" # Write
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e" # Notify

async def run_connection(address=None):
    # If no address is provided, scan for the device by name
    if address is None:
        print("Scanning for device 'AgroBridge'...")
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: d.name and "AgroBridge" in d.name,
            timeout=10.0
        )
        
        if not device:
            print("Device 'AgroBridge' not found.")
            return
            
        address = device.address
        print(f"Found device: {device.name} ({address})")

    print(f"Attempting to connect to {address}...")
    
    def notification_handler(sender, data):
        try:
            text = data.decode('utf-8').strip()
            print(f"RX: {data} -> '{text}'")
        except:
            print(f"RX: {data}")

    try:
        async with BleakClient(address) as client:
            print(f"Connected: {client.is_connected}")
            
            # Ensure we are subscribed BEFORE writing
            await client.start_notify(NUS_TX_UUID, notification_handler)
            print("Subscribed to notifications.")
            
            # Small delay to ensure subscription is active on device side
            await asyncio.sleep(1.0)
            
            print("Sending 'Hello'...")
            # Send a test message to trigger response
            await client.write_gatt_char(NUS_RX_UUID, b"Hello\n", response=True)
            
            print("Waiting for data... (Press Ctrl+C to stop)")
            while True:
                await asyncio.sleep(1)

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    asyncio.run(run_connection())
