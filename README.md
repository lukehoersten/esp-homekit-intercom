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

### AC

- 52V
- Peak 73V
- 100kO, 12.2kO
- Peak 8V
-

- f = 60 Hz
- C = 33 * 10^-6

4.7uF capacitor


### DC

- 23V
- HI (2.475V, 3.6V)
- LO (-0.3V, 0.825V)

- r1 = 100kO
- r2 = 10kO + 2.2kO


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


## Doorbell ADC Output

```I (65813) HAP Intercom: Intercom bell ring value in range [2960]
I (66813) HAP Intercom: Intercom bell ring value in range [2966]
I (67813) HAP Intercom: Intercom bell ring value in range [2957]
I (68813) HAP Intercom: Intercom bell ring value in range [2945]
I (69813) HAP Intercom: Intercom bell ring value in range [2953]
I (70813) HAP Intercom: Intercom bell ring value in range [2951]
I (71813) HAP Intercom: Intercom bell ring value in range [2959]
I (72813) HAP Intercom: Intercom bell ring value in range [2930]
I (73813) HAP Intercom: Intercom bell ring value in range [2951]
I (74813) HAP Intercom: Intercom bell ring value in range [2960]
I (75813) HAP Intercom: Intercom bell ring value in range [2978]
I (76813) HAP Intercom: Intercom bell ring value in range [2987]
I (77813) HAP Intercom: Intercom bell ring value in range [2949]
I (78813) HAP Intercom: Intercom bell ring value in range [2951]
I (79813) HAP Intercom: Intercom bell ring value in range [2929]
I (80813) HAP Intercom: Intercom bell ring value in range [2961]
I (81813) HAP Intercom: Intercom bell ring value in range [2960]
I (82813) HAP Intercom: Intercom bell ring value in range [2953]
I (83813) HAP Intercom: Intercom bell ring value in range [2957]
I (84813) HAP Intercom: Intercom bell ring value in range [2957]
I (85813) HAP Intercom: Intercom bell ring value in range [2705]
I (86813) HAP Intercom: Intercom bell ring value in range [2343] ---- start
I (87813) HAP Intercom: Intercom bell ring value in range [2343]
I (88813) HAP Intercom: Intercom bell ring value in range [2359] ---- stop
I (89813) HAP Intercom: Intercom bell ring value in range [2909]
I (90813) HAP Intercom: Intercom bell ring value in range [3045]
I (91813) HAP Intercom: Intercom bell ring value in range [2951]
I (92813) HAP Intercom: Intercom bell ring value in range [2959]
I (93813) HAP Intercom: Intercom bell ring value in range [2951]
I (94813) HAP Intercom: Intercom bell ring value in range [2949]
```
