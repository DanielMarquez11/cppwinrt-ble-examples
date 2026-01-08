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
BluetoothLEDevice g_device{ nullptr };
GattCharacteristic g_heartRateChar{ nullptr };
//--------------------------------------------------------------------------------------------

SOCKET s;
SOCKET clientSocket;

void handleData(const std::string& data) {
    std::cout << "Client sent: " + data << std::endl;
}

int main()
{

    WSADATA wsa_data;
    WSAStartup(MAKEWORD(1, 1), &wsa_data);
    
    scanForDevice();
    return 0;
}

//--------------------------------------------------------------------------------------------
void OnHeartRateChanged(GattCharacteristic const& sender,
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattValueChangedEventArgs const& args)
{
    auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(args.CharacteristicValue());

    uint8_t flags = reader.ReadByte();
    bool is16bit = flags & 0x01;

    uint16_t bpm = is16bit ? reader.ReadUInt16() : reader.ReadByte();

    std::cout << "❤️ Heart Rate: " << bpm << " bpm\n";
	unsigned char byte = static_cast<unsigned char>(bpm);

	int r = send(clientSocket, (const char*)&byte, sizeof(byte), 0);
	std::cout << "Sent " << r << " bytes to client." << std::endl;
    if (r == -1) {
        std::cerr << "send() failed with error: " << WSAGetLastError() << std::endl;
	}
}

void OnAdverReceived(BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
    if (g_targetDevice != 0)
        return;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(3457);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(s, (const sockaddr*)&server_addr, sizeof(server_addr));
	listen(s, SOMAXCONN);

    sockaddr_in client;
    int clientSize = sizeof(client);
    clientSocket = accept(s, (sockaddr*)&client, &clientSize);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "accept() failed with error: " << WSAGetLastError() << std::endl;
        closesocket(s);
        return;
    }

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
    {
        std::cout << host << " connected on port " << service << std::endl;
    }
    else
    {
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        std::cout << host << " connected on port " <<
            ntohs(client.sin_port) << std::endl;
    }   

    //g_targetDevice = eventArgs.BluetoothAddress();
    g_targetDevice = 232406127976466;
    std::cout << "Connecting to device: " << g_targetDevice << std::endl;

    watcher.Stop();

    g_device = BluetoothLEDevice::FromBluetoothAddressAsync(g_targetDevice).get();
    auto servicesResult = g_device.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();

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
                    g_heartRateChar = ch;

                    g_heartRateChar.ValueChanged(OnHeartRateChanged);

                    auto status = g_heartRateChar.WriteClientCharacteristicConfigurationDescriptorAsync(
                        winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattClientCharacteristicConfigurationDescriptorValue::Notify
                    ).get();

                    std::cout << "CCCD write status: " << (int)status << std::endl;
                    std::cout << "Subscribed to Heart Rate\n";
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
void scanForDevice()
{
    BluetoothLEAdvertisementWatcher watch;
    watch.Received(&OnAdverReceived);
    watch.Start();
    while (getchar() != '\n');
    watch.Stop();
}
//--------------------------------------------------------------------------------------------