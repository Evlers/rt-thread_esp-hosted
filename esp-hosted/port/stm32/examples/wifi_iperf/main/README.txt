Only one of the following example files should be enabled at a time when building, as they all contain app_main():

1. station_example_main.c: A station example to connect to an AP and run iperf. iperf can run as a UDP and TCP client or server.

2. ota_example_main.c: An OTA example to connect to an AP, fetch the co-processor binary file from a HTTP server, and flash it to the ESP32 co-processor. The system does a reboot after a successful OTA download.

3. scan_example.c: A station example to perform a scan for APs.

4. softap_example.c: An example of starting up as a SoftAP. Devices can connect to the SoftAP and get a DHCP IP address.

5. rssi_bssid_example.c: A station example of connect to an AP, gets the BSSID on the connected AP when the connected event happens, then periodically get the RSSI of the connection.
