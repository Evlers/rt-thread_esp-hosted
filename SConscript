Import('RTT_ROOT')
Import('rtconfig')
from building import *

cwd = GetCurrentDir()

# add the general drivers.
src = Split("""
""")

# add esp-hosted common source and header files
src += Glob('common/*.c')
src += Glob('host/common/*.c')
path = [cwd + '/host']
path += [cwd + '/common/include']

# add protobuf source and header files
src += Glob('common/protobuf-c/protobuf-c/*.c')
path += [cwd + '/common/protobuf-c']

# add host components source and header files
src += Glob('host/components/src/*.c')
path += [cwd + '/host/components/include']

# add host control_lib source and header files
src += Glob('host/control_lib/src/*.c')
path += [cwd + '/host/control_lib/include']
path += [cwd + '/host/control_lib/src/include']

# add virtual_serial_if source and header files
src += Glob('host/virtual_serial_if/src/*.c')
path += [cwd + '/host/virtual_serial_if/include']

# add driver source and header files
src += Glob('host/driver/*/*.c')
src += Glob('host/driver/transport/spi/*.c')
path += [cwd + '/host/driver/transport/spi']
path += [cwd + '/host/driver/netif']
path += [cwd + '/host/driver/network']
path += [cwd + '/host/driver/serial']
path += [cwd + '/host/driver/transport']

# add port source and header files
src += Glob('host/port/src/*.c')
path += [cwd + '/host/port/include']

# add wlan source and header files
src += Glob('wlan/*.c')
path += [cwd + '/wlan']


CPPDEFINES = ['']

group = DefineGroup('esp-hosted', src, depend = ['RT_USING_ESP_HOSTED'], CPPPATH = path, CPPDEFINES = CPPDEFINES)

Return('group')
