# MeshAQ
This project aimed to create the ultimate Air Quality sensor station to monitor. But after being a bit bussy I didn't have time to make it up to my original idea.  
But I managed to make a super small neat little all in one AQ helth monitor. Although I miss the other sensors.  
Formaldehyde sensor is super expensive and an increased levels can can be deducted from the VOC index.  

So yeah I'm so happy with the result! A neat little usb powered AQ station.

## Wiring diagram

| Heltec T114 Pin | SEN66 Pin |
| --------------- | --------- |
| GND             | GND       |
| 3V3             | VCC       |
| GPIO 0.16 (SDA) | SDA       |
| GPIO 0.13 (SCL) | SCL       |

## Sensors

| Measurement       | Unit  | Sensor | Significance                                                                      | Unhealthy level - suden spikes | Unhealthy level - 24h average                |
| ----------------- | ----- | ------ | --------------------------------------------------------------------------------- | ------------------------------ | -------------------------------------------- |
| Temperature       | °C    | SEN66  | Affects human comfort and thermoregulation; influences pollutant chemistry.       |                                | >30°C (heat stress risk)                     |
| Relative humidity | %     | SEN66  | Impacts respiratory comfort, aerosol growth, and mould/virus survival.            |                                | <30% (dry) or >60% (mould/comfort issues)    |
| PM (PM2.5)        | µg/m³ | SEN66  | Fine particles penetrate lungs/bloodstream; linked to cardio-respiratory disease. |                                | annual ≤5 µg/m³; 24‑h >15 µg/m³ (exceedance) |
| PM (PM10)         | µg/m³ | SEN66  | Alergens, dust and visible particles                                              |
| VOC Index         | -     | SEN66  | Sensor reports a relative VOC Index (unitless); indicates overall VOC load.       |                                | Index >100                                   |
| NOx (NO2)         | -     | SEN66  | Sensor reports a relative NOx Index (unitless); NO2 is a respiratory irritant.    |                                | Index >100                                   |
| CO2               | ppm   | SEN66  | Proxy for ventilation and occupancy; high levels impair cognition.                |                                | >1000 ppm (poor ventilation)                 |

### Future upgrades

| Measurement       | Unit  | Sensor                | Significance                                                                      | Unhealthy level - suden spikes | Unhealthy level - 24h average                |
| Formaldehyde      | ppm   | SFA40                 | Irritant and known carcinogen; common indoor emission from materials.             |                                | ??                                           |
| Ground Ozone      | µg/m³ | (TBD)                 | Respiratory irritant and strong oxidant; forms from NOx/VOCs and sunlight.        |                                | Peak-season 8‑hr mean >60 µg/m³              |
| UV Index          | -     | LTR390UV / AMS AS7331 | Indicates UV radiation intensity; affects skin/eye health and photochemistry.     |                                | UVI >8 (very high/extreme exposure)          |
| Sound level       | dB    | TDK ICS43432 (mic)    | High noise impacts sleep, stress, and hearing; affects wellbeing indirectly.      |                                | >85 dB (prolonged exposure harmful)          |

### Resources

- AI - It was crosschecked with material bellow
- https://www.who.int/publications/i/item/9789240034228
- https://sensirion.com/media/documents/02232963/6294E043/Info_Note_VOC_Index.pdf
- https://sensirion.com/media/documents/9F289B95/6294DFFC/Info_Note_NOx_Index.pdf
- https://sensirion.com/products/catalog/SEN66

## Images

![Front](/Images/Front.png)
![Back](/Images/Back.png)

## Software

Runs on Meshtastic.

## BOM

| Item                    | Price (USD) | I'll source myself | Link & source                                                                                         |
| ----------------------- | ----------- | ------------------ | ----------------------------------------------------------------------------------------------------- |
| 3d printed case         | ~ 1         | YES                | MeshAQ.3mf                                                                                            |
| Heltec Mesh Node T114   | 33.13       | YES                | https://www.aliexpress.com/item/1005011784585928.html                                                 |
| SEN66 AQ sensor         | 58.72       | NO                 | https://cz.farnell.com/en-CZ/sensirion/sen66-sin-t/sensor-module-air-0-to-40000ppm/dp/4587290         |
| JST GHR-06V-S connector | 0.14        | NO                 | https://cz.farnell.com/en-CZ/jst-japan-solderless-terminals/ghr-06v-s/housing-1-25mm-6way/dp/1516242" |
| battery                 | 0           | YES                | I already have one                                                                                    |
| HackClub Total          | 58.89       |
| Total                   | 92.02       |

## HackClub is fire! 🔥
https://blueprint.hackclub.com/projects/11552
