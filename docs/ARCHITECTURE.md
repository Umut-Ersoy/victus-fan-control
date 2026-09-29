# Safety & Fault-Tolerance Architecture

[🇬🇧 English](ARCHITECTURE.md) | [🇹🇷 Türkçe](ARCHITECTURE.tr.md)

```
                    ┌────────────────────────┐
                    │  Read Sensor (sysfs)   │
                    └───────────┬────────────┘
                                │
                 ┌──────────────┴──────────────┐
                 ▼                             ▼
        [ temp < 0 OR stuck <= 25°C ]    [ Normal Reading ]
                 │                             │
                 ▼                             ▼
         Engage Emergency 100%         Calculate Fan Curve
         Log [ERROR] to stderr          (Linear Interpolation)
                 │                             │
                 └──────────────┬──────────────┘
                                ▼
                    Write Targets to Sysfs
```

1. **Stuck Sensor Watchdog:** If an ACPI glitch causes the sensor to read a stuck value $\le 25^\circ\text{C}$ for 5 consecutive polling cycles, the daemon logs an error to `stderr` and engages 100% emergency cooling.
2. **Emergency Upper Bound:** Any temperature exceeding the highest defined curve point or `temp_critical` immediately triggers 100% fan speed.
3. **Crash Recovery:** Systemd `ExecStopPost` automatically executes `echo 2 > pwm1_enable` upon service exit, crash, or `SIGKILL`.
