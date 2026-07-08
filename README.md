# robocup2026-esp32-kanto

## Overview

This repository contains the ESP32 firmware used in our RoboCup 2026 Kanto project.

## Sending formats

### Messages

```
<id> <Message>
```

### Motors

send

```
MOTOR <l> <r>
```

contraints

- -10,000 <= l, r <= 10,000

reply

```
ok
```

### arm

send

```
ARM <angle>
```

reply

```
ok
```

### buzzer

send

```
BUZZ <frequency(hz)> <duration(msec)>
```

reply

```
ok
```

### bno

send

```
BNO
```

reply

```
ok <head> <roll> <pitch> <x_acc> <y_acc> <z_acc>
```

### VL53L0X

send

```
TOF <l,f,r>
```

reply

```
ok [distance(int, space splitted)]
```

### Load cell

send
```
LOAD
```

reply
```
ok <weight_L> <weight_R>
```

### Cage

send
```
CAGE <O/C>
```
where O is open and C is close.

reply

```
ok
```

### Switch

send
```
SWITCH
```

reply
```
ok <(on|off)>
```

## Dev

This project uses PlatformIO.

```code
pio run
pio run --target upload
```
