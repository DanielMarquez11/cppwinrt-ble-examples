/*
 * A Very Simple BLE Device Scanner
 */

#include "pch.h"
#include <iostream>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <Windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
//--------------------------------------------------------------------------------------------
using winrt::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
using winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
using winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs;
using winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicsResult;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceServicesResult;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSession;
using winrt::Windows::Storage::Streams::DataWriter;
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteOption;
using winrt::Windows::Devices::Bluetooth::BluetoothCacheMode;
//--------------------------------------------------------------------------------------------
void scanForDevice();
//--------------------------------------------------------------------------------------------
uint64_t g_targetDevice = 0;
//--------------------------------------------------------------------------------------------

int main() 
{
    scanForDevice();
}

void OnHeartRateChanged(GattCharacteristic const& sender,
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattValueChangedEventArgs const& args)
{
    auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(args.CharacteristicValue());

    uint8_t flags = reader.ReadByte();
    bool is16bit = flags & 0x01;

    uint16_t bpm = is16bit ? reader.ReadUInt16() : reader.ReadByte();

    std::cout << "❤️ Heart Rate: " << bpm << " bpm\n";
}

//--------------------------------------------------------------------------------------------
void OnAdverReceived(BluetoothLEAdvertisementWatcher watcher,
         BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{        
    if (g_targetDevice != 0)
        return;
    g_targetDevice = eventArgs.BluetoothAddress();
    std::cout << "Connecting to device: " << g_targetDevice << std::endl;
    watcher.Stop();
    auto dev = BluetoothLEDevice::FromBluetoothAddressAsync(g_targetDevice).get();
    if (!dev)
    {
        std::cout << "Failed to connect\n";
        return;
    }
    auto servicesResult = dev.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
    if (servicesResult.Status() != GattCommunicationStatus::Success)
    {
        std::cout << "Failed to get services\n";
        return;
    }
    for (auto service : servicesResult.Services())
    {
        if (service.Uuid() == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceUuids::HeartRate())
        {
            std::cout << "Heart Rate service found\n";

            auto charsResult = service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).get();
            for (auto ch : charsResult.Characteristics())
            {
                if (ch.Uuid() == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicUuids::HeartRateMeasurement())
                {
                    std::cout << "Subscribed to Heart Rate\n";

                    ch.ValueChanged(OnHeartRateChanged);

                    ch.WriteClientCharacteristicConfigurationDescriptorAsync(
                        winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattClientCharacteristicConfigurationDescriptorValue::Notify
                    ).get();
                }
            }
        }
    }
}
//--------------------------------------------------------------------------------------------
void scanForDevice()
{
    std::cout << "Lets Find some BLE Devices: Press Enter to Quit" << std::endl;

    BluetoothLEAdvertisementWatcher watch;
    watch.Received(&OnAdverReceived);
    watch.Start();
    while (getchar() != '\n');
    watch.Stop();
}
//--------------------------------------------------------------------------------------------