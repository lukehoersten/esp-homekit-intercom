# ESP HomeKit Intercom

## Overview

An ESP32 HomeKit implementation of a [Lobby Door Intercom](https://wrt.nth.io/luke/siri-controlled-1970s-intercom-door)
with a solenoid door lock control and a voltage-based doorbell "buzzer".

This is a reimplementation of the [original based on a RaspberryPi Zero W and HAP-NodeJS](https://github.com/lukehoersten/homekit-door).

The impetus for this ESP32 implementation was [Espressif's HAP SDK](https://github.com/espressif/esp-homekit-sdk) which
is much better than the official [Apple "ADK"](https://github.com/espressif/esp-apple-homekit-adk).

## Electronics

The input voltage for the doorbell has a 23VDC and a 52VAC component and the same circuit is shared with all the units
in a teir. I haven't yet determined exactly how the buzzer indexes to the AC/DC voltage change but it's pulled
high. When the buzzer is triggered, the ADC measures a voltage drop of about 20%. I'm assuming this is the DC component
only measured by the ADC.

## Partition Sizing

When flashing, the following error may occur:

```
E (703) esp_image: Image length 1435760 doesn't fit in partition length 1048576
E (703) boot: Factory app partition is not bootable
E (705) boot: No bootable app partitions in the partition table
```

The fix is [documented](https://github.com/micropython/micropython-esp32/issues/235) as the following:

`esp-idf/components/partition_table/partitions_singleapp.csv`
```
# Name,   Type, SubType, Offset,  Size, Flags
# Note: if you have increased the bootloader size, make sure to update the offsets to avoid overlap
nvs,      data, nvs,     ,        0x6000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        0x180000, # Change from 1M to 0x180000
```
