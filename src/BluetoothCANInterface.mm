#include "BluetoothCANInterface.hpp"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include "isobus/utility/system_timing.hpp"
#include <iostream>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>

// Uncomment to enable CAN frame logging
// #define ENABLE_CAN_FRAME_LOGGING

// Nordic UART Service UUIDs
#define NUS_SERVICE_UUID @"6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      @"6e400002-b5a3-f393-e0a9-e50e24dcca9e" // Write
#define NUS_TX_UUID      @"6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Notify

// Simple struct to match the expected raw binary format
struct __attribute__((packed)) RawCANFrame {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};

// Forward declaration of the C++ class to use in the delegate
class BluetoothCANInterface;

// Objective-C Delegate Interface
@interface BluetoothDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
{
    CBCentralManager *centralManager;
    CBPeripheral *discoveredPeripheral;
    CBCharacteristic *rxCharacteristic; // For writing
    CBCharacteristic *txCharacteristic; // For receiving
    
    BluetoothCANInterface *cppInterface;
    BOOL isConnected;
    BOOL isNotifying;
    NSMutableData *rxBuffer; // Buffer for incoming fragmented data
}

- (instancetype)initWithInterface:(BluetoothCANInterface *)interface;
- (void)startScan;
- (void)stopScan;
- (void)disconnect;
- (void)writeData:(NSData *)data;
- (BOOL)isReady;

@end

// Global pointer to the delegate for this instance
static BluetoothDelegate *g_delegate = nil;

BluetoothCANInterface::BluetoothCANInterface()
{
    // Create the delegate
    // We use a global for simplicity as we can't easily modify the header to add void* impl
    // without another tool call, but we will modify the header next to add receive_frame_internal.
}

BluetoothCANInterface::~BluetoothCANInterface()
{
    close();
    g_delegate = nil;
}

std::string BluetoothCANInterface::get_name() const
{
    return "Bluetooth CAN (AgroBridge)";
}

bool BluetoothCANInterface::get_is_valid() const
{
    return (g_delegate != nil) && [g_delegate isReady];
}

void BluetoothCANInterface::open()
{
    if (g_delegate == nil) {
        g_delegate = [[BluetoothDelegate alloc] initWithInterface:this];
    }
    
    std::cout << "[BluetoothCAN] Starting scan for 'AgroBridge'..." << std::endl;
    [g_delegate startScan];
    connected = true; 
}

void BluetoothCANInterface::close()
{
    if (g_delegate != nil) {
        [g_delegate disconnect];
        [g_delegate stopScan];
    }
    connected = false;
}

bool BluetoothCANInterface::read_frame(isobus::CANMessageFrame &canFrame)
{
    std::lock_guard<std::mutex> lock(rxMutex);
    if (!rxQueue.empty())
    {
        canFrame = rxQueue.front();
        rxQueue.pop();
        return true;
    }
    return false;
}

bool BluetoothCANInterface::write_frame(const isobus::CANMessageFrame &canFrame)
{
    if (g_delegate == nil || ![g_delegate isReady]) return false;

    RawCANFrame rawFrame;
    rawFrame.id = canFrame.identifier;
    // Handle extended frame flag in ID if needed, usually bit 31
    if (canFrame.isExtendedFrame) {
        rawFrame.id |= (1 << 31); 
    }
    rawFrame.dlc = canFrame.dataLength;
    std::memset(rawFrame.data, 0, 8);
    std::memcpy(rawFrame.data, canFrame.data, canFrame.dataLength);

    NSData *data = [NSData dataWithBytes:&rawFrame length:sizeof(RawCANFrame)];
    [g_delegate writeData:data];

#ifdef ENABLE_CAN_FRAME_LOGGING
    std::cout << "[BluetoothCAN] TX Frame ID: " << std::hex << canFrame.identifier << std::dec << " DLC: " << (int)canFrame.dataLength << " Data: ";
    for (int i = 0; i < canFrame.dataLength; i++) {
        printf("%02X ", canFrame.data[i]);
    }
    std::cout << std::endl;
#endif

    return true;
}

void BluetoothCANInterface::receive_frame_internal(const isobus::CANMessageFrame& frame)
{
    std::lock_guard<std::mutex> lock(rxMutex);
    rxQueue.push(frame);
}

// Objective-C Delegate Implementation
@implementation BluetoothDelegate

- (instancetype)initWithInterface:(BluetoothCANInterface *)interface
{
    self = [super init];
    if (self) {
        cppInterface = interface;
        // Use a dedicated serial queue for Bluetooth operations to avoid blocking the main thread
        dispatch_queue_t centralQueue = dispatch_queue_create("com.agiso.bluetooth", DISPATCH_QUEUE_SERIAL);
        centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:centralQueue];
        isConnected = NO;
        isNotifying = NO;
        rxBuffer = [[NSMutableData alloc] init];
    }
    return self;
}

- (void)startScan
{
    if (centralManager.state == CBManagerStatePoweredOn) {
        [centralManager scanForPeripheralsWithServices:nil options:nil];
    }
}

- (void)stopScan
{
    [centralManager stopScan];
}

- (void)disconnect
{
    if (discoveredPeripheral) {
        [centralManager cancelPeripheralConnection:discoveredPeripheral];
        discoveredPeripheral = nil;
        isConnected = NO;
        isNotifying = NO;
    }
}

- (BOOL)isReady
{
    return isConnected && isNotifying && (rxCharacteristic != nil) && (txCharacteristic != nil);
}

- (void)writeData:(NSData *)data
{
    if (discoveredPeripheral && rxCharacteristic) {
        [discoveredPeripheral writeValue:data forCharacteristic:rxCharacteristic type:CBCharacteristicWriteWithResponse];
    }
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    if (central.state == CBManagerStatePoweredOn) {
        std::cout << "[BluetoothCAN] Bluetooth is ON. Scanning..." << std::endl;
        [self startScan];
    } else {
        std::cout << "[BluetoothCAN] Bluetooth is not ready." << std::endl;
    }
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI
{
    NSString *name = peripheral.name;
    if (name && [name containsString:@"AgroBridge"]) {
        NSNumber *isConnectable = advertisementData[CBAdvertisementDataIsConnectable];
        if (isConnectable && ![isConnectable boolValue]) {
             std::cout << "[BluetoothCAN] Found 'AgroBridge' but it is not connectable." << std::endl;
             return;
        }

        std::cout << "[BluetoothCAN] Found device: " << [name UTF8String] << " (RSSI: " << [RSSI intValue] << ")" << std::endl;
        
        // Retain the peripheral strongly
        discoveredPeripheral = peripheral;
        discoveredPeripheral.delegate = self;
        
        std::cout << "[BluetoothCAN] Connecting..." << std::endl;
        [centralManager connectPeripheral:peripheral options:nil];
        
        // Stop scanning after initiating connection
        [centralManager stopScan];
    }
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    std::cout << "[BluetoothCAN] Failed to connect: " << [[error localizedDescription] UTF8String] << std::endl;
    discoveredPeripheral = nil;
    [self startScan];
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
    std::cout << "[BluetoothCAN] Connected to " << [[peripheral name] UTF8String] << std::endl;
    isConnected = YES;
    [peripheral discoverServices:@[[CBUUID UUIDWithString:NUS_SERVICE_UUID]]];
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    std::cout << "[BluetoothCAN] Disconnected." << std::endl;
    isConnected = NO;
    isNotifying = NO;
    rxCharacteristic = nil;
    txCharacteristic = nil;
    // Restart scan to auto-reconnect
    [self startScan];
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
    if (error) {
        std::cout << "[BluetoothCAN] Error discovering services: " << [[error localizedDescription] UTF8String] << std::endl;
        return;
    }
    
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:[CBUUID UUIDWithString:NUS_SERVICE_UUID]]) {
            [peripheral discoverCharacteristics:@[[CBUUID UUIDWithString:NUS_RX_UUID], [CBUUID UUIDWithString:NUS_TX_UUID]] forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
    if (error) {
        std::cout << "[BluetoothCAN] Error discovering characteristics: " << [[error localizedDescription] UTF8String] << std::endl;
        return;
    }
    
    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:NUS_RX_UUID]]) {
            rxCharacteristic = characteristic;
            std::cout << "[BluetoothCAN] Found RX Characteristic (Write)" << std::endl;
        } else if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:NUS_TX_UUID]]) {
            txCharacteristic = characteristic;
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            std::cout << "[BluetoothCAN] Found TX Characteristic (Notify)" << std::endl;
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (error) {
        std::cout << "[BluetoothCAN] Error receiving data: " << [[error localizedDescription] UTF8String] << std::endl;
        return;
    }
    
    if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:NUS_TX_UUID]]) {
        NSData *data = characteristic.value;
        
        // Log raw incoming data for debugging integrity
        // std::cout << "[BluetoothCAN] Raw RX (" << [data length] << " bytes): ";
        // const uint8_t *rawBytes = (const uint8_t *)[data bytes];
        // for (NSUInteger i = 0; i < [data length]; i++) {
        //     printf("%02X ", rawBytes[i]);
        // }
        // std::cout << std::endl;
        
        // Append new data to the buffer
        [rxBuffer appendData:data];
        
        // Safety valve: If buffer gets too large, we are hopelessly out of sync
        if (rxBuffer.length > 256) {
             std::cout << "[BluetoothCAN] Buffer overflow (" << rxBuffer.length << " bytes). Clearing buffer to resync." << std::endl;
             [rxBuffer setLength:0];
             return;
        }
        
        // Process buffer while we have enough data for at least the header (ID + DLC = 5 bytes)
        while (rxBuffer.length >= 5) {
            const uint8_t *bytes = (const uint8_t *)[rxBuffer bytes];
            
            // Parse DLC (5th byte)
            uint8_t dlc = bytes[4];
            
            // Sanity check DLC
            if (dlc > 8) {
                std::cout << "[BluetoothCAN] Invalid DLC (" << (int)dlc << ") at offset 4. Resyncing (dropping 1 byte)..." << std::endl;
                // Invalid DLC, likely lost sync. Drop one byte and try again to find a valid header.
                [rxBuffer replaceBytesInRange:NSMakeRange(0, 1) withBytes:NULL length:0];
                continue;
            }
            
            NSUInteger expectedLength = 5 + dlc;
            
            if (rxBuffer.length >= expectedLength) {
                // We have a full frame
                RawCANFrame rawFrame;
                // Copy ID
                std::memcpy(&rawFrame.id, bytes, 4);
                // Copy DLC
                rawFrame.dlc = dlc;
                // Copy Data
                if (dlc > 0) {
                    std::memcpy(rawFrame.data, &bytes[5], dlc);
                }
                
                isobus::CANMessageFrame frame;
                frame.identifier = rawFrame.id & 0x7FFFFFFF; // Mask out extended flag
                frame.isExtendedFrame = ((rawFrame.id & 0x80000000) != 0) || (frame.identifier > 0x7FF);
                frame.dataLength = rawFrame.dlc;
                // Ensure we don't overflow the internal data buffer (max 8)
                if (frame.dataLength > 8) frame.dataLength = 8;
                
                std::memcpy(frame.data, rawFrame.data, frame.dataLength);
                
                // Use ISOBUS stack timing for compatibility
                frame.timestamp_us = isobus::SystemTiming::get_timestamp_us();
                frame.channel = 0; // Explicitly set channel to 0

#ifdef ENABLE_CAN_FRAME_LOGGING
                std::cout << "[BluetoothCAN] RX Frame ID: " << std::hex << frame.identifier << std::dec << " DLC: " << (int)frame.dataLength << " Data: ";
                for (int i = 0; i < frame.dataLength; i++) {
                    printf("%02X ", frame.data[i]);
                }
                std::cout << std::endl;
#endif

                cppInterface->receive_frame_internal(frame);
                
                // Remove the processed frame from the buffer
                [rxBuffer replaceBytesInRange:NSMakeRange(0, expectedLength) withBytes:NULL length:0];
            } else {
                // Not enough data for the full payload yet, wait for more
                break;
            }
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (error) {
        std::cout << "[BluetoothCAN] Error changing notification state: " << [[error localizedDescription] UTF8String] << std::endl;
    } else {
        isNotifying = characteristic.isNotifying;
        std::cout << "[BluetoothCAN] Notification state updated for " << [[characteristic.UUID UUIDString] UTF8String] << ": " << (characteristic.isNotifying ? "Enabled" : "Disabled") << std::endl;
    }
}

@end
