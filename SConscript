Import('RTT_ROOT')
Import('rtconfig')
from building import *

cwd = GetCurrentDir()

# add the general drivers.
src = Split("""
""")

# add esp-hosted common source and header files
src += Glob('esp-hosted/common/proto/*.c')
src += Glob('esp-hosted/host/common/*.c')
path = [cwd + '/esp-hosted/host']
path += [cwd + '/esp-hosted/common/proto']
path += [cwd + '/esp-hosted/common/include']

# add protobuf source and header files
src += Glob('esp-hosted/common/protobuf-c/protobuf-c/*.c')
path += [cwd + '/esp-hosted/common/protobuf-c']

# add host api source and header files
src += Glob('esp-hosted/host/api/src/*.c')
path += [cwd + '/esp-hosted/host/api/include']

# add components source and header files
src += Glob('esp-hosted/host/components/log/*.c')
src += Glob('esp-hosted/host/components/utils/*.c')
src += Glob('esp-hosted/host/components/esp_wifi/*.c')
src += Glob('esp-hosted/host/components/esp_wifi_remote/*.c')
path += [cwd + '/esp-hosted/host/components/log']
path += [cwd + '/esp-hosted/host/components/utils']
path += [cwd + '/esp-hosted/host/components/esp_wifi/include']
path += [cwd + '/esp-hosted/host/components/esp_netif/include']

# add host drivers source and header files
src += Glob('esp-hosted/host/drivers/mempool/*.c')
src += Glob('esp-hosted/host/drivers/rpc/core/*.c')
src += Glob('esp-hosted/host/drivers/rpc/slaveif/*.c')
src += Glob('esp-hosted/host/drivers/rpc/wrap/*.c')
src += Glob('esp-hosted/host/drivers/serial/*.c')
src += Glob('esp-hosted/host/drivers/transport/*.c')
src += Glob('esp-hosted/host/drivers/transport/spi/*.c')
src += Glob('esp-hosted/host/drivers/virtual_serial_if/*.c')
path += [cwd + '/esp-hosted/host/drivers/mempool']
path += [cwd + '/esp-hosted/host/drivers/rpc/core']
path += [cwd + '/esp-hosted/host/drivers/rpc/slaveif']
path += [cwd + '/esp-hosted/host/drivers/rpc/wrap']
path += [cwd + '/esp-hosted/host/drivers/serial']
path += [cwd + '/esp-hosted/host/drivers/transport']
path += [cwd + '/esp-hosted/host/drivers//transport/spi']
path += [cwd + '/esp-hosted/host/drivers/virtual_serial_if']

# add host utils source and header files
src += Glob('esp-hosted/host/utils/*.c')
path += [cwd + '/esp-hosted/host/utils']

# add host port source and header files
src += Glob('porting/port/source/*.c')
path += [cwd + '/porting/port/include']

# add ota source and header files
src += Glob('porting/ota/*.c')
path += [cwd + '/porting/ota']

# add bt source and header files
if GetDepend(['ESP_HOSTED_BT_USING_VHCI_DEVICE_DRIVER']):
    # add vhci device driver source and header files
    src += Glob('porting/bt/*.c')
    src += Glob('porting/bt/vhci/*.c')
    path += [cwd + '/porting/bt/vhci']

if GetDepend(['ESP_HOSTED_BT_USING_NIMBLE_STACK']):
    # add nimble hci driver source and header files
    src += Glob('esp-hosted/host/drivers/bt/*.c')
path += [cwd + '/esp-hosted/host/drivers/bt']

# add host wlan source files
src += Glob('porting/wlan/*.c')

CPPDEFINES = ['']

group = DefineGroup('esp-hosted', src, depend = ['RT_USING_ESP_HOSTED'], CPPPATH = path, CPPDEFINES = CPPDEFINES)

Return('group')
