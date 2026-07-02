# MeshAQ
A portable sensor node designed to be carried and deployed anywhere.

This project serves as a development platform for Meshtastic, aiming to integrate the SEN6x family of environmental sensors and to support future upgrades in air quality and environmental telemetry within the Meshtastic ecosystem.

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
| PM (PM10)         | µg/m³ | SEN66  | Alergens, dust and visible particles                                              |                                |
| VOC Index         | -     | SEN66  | Sensor reports a relative VOC Index (unitless); indicates overall VOC load.       |                                | Index >100                                   |
| NOx (NO2)         | -     | SEN66  | Sensor reports a relative NOx Index (unitless); NO2 is a respiratory irritant.    |                                | Index >100                                   |
| CO2               | ppm   | SEN66  | Proxy for ventilation and occupancy; high levels impair cognition.                |                                | >1000 ppm (poor ventilation)                 |

### Future upgrades

| Measurement  | Unit  | Sensor                | Significance                                                                  | Unhealthy level - 24h average       |
| ------------ | ----- | --------------------- | ----------------------------------------------------------------------------- | ----------------------------------- |
| Formaldehyde | ppm   | SFA40                 | Irritant and known carcinogen; common indoor emission from materials.         | ??                                  |
| Ground Ozone | µg/m³ | (TBD)                 | Respiratory irritant and strong oxidant; forms from NOx/VOCs and sunlight.    | Peak-season 8‑hr mean >60 µg/m³     |
| UV Index     | -     | LTR390UV / AMS AS7331 | Indicates UV radiation intensity; affects skin/eye health and photochemistry. | UVI >8 (very high/extreme exposure) |
| Sound level  | dB    | TDK ICS43432 (mic)    | High noise impacts sleep, stress, and hearing; affects wellbeing indirectly.  | >85 dB (prolonged exposure harmful) |

### Resources

- AI - It was crosschecked with material bellow
- https://www.who.int/publications/i/item/9789240034228
- https://sensirion.com/media/documents/02232963/6294E043/Info_Note_VOC_Index.pdf
- https://sensirion.com/media/documents/9F289B95/6294DFFC/Info_Note_NOx_Index.pdf
- https://sensirion.com/products/catalog/SEN66

## Images

![Front](/Images/Front.png)  
![Back](/Images/Back.png)  
### Prototype
![Prototype](https://cdn.hackclub.com/019f23f8-23f4-7395-831d-e00cda036d53/img_20260702_195404.jpg)  

## Software

### Prototype for outpost
It will have it's own custom Arduino code made to interface with the sensor and display the values on the Eink screen.  

### Final version
I'm hoping to integrate it into meshtastic.  

## BOM

| Item                       | Price (USD) | Link & source                                                                                           |
| -------------------------- | ----------- | ------------------------------------------------------------------------------------------------------- |
| 3d printed case            | ~ 1         | MeshAQ.3mf                                                                                              |
| Heltec Mesh Node T114      | 63.38       | https://www.laskakit.cz/heltec-mesh-node-t114-v2-0-868mhz-nrf52840-sx1262/?variantId=17030              |
| SEN66 AQ sensor            | 68.96       | https://www.laskakit.cz/senserion-sen66-sin-t-senzor-kvality-ovzdusi/                                   |
| JST GHR-06V-S connector    | 0.85        | https://www.laskakit.cz/laskakit-airboard-propojovaci-kabel-pro-senserion-sen6x-senzor-kvality-ovzdusi/ |
| battery                    | 10.31       | https://www.laskakit.cz/baterie-li-po-3-7v-3000mah-lipo/                                                |
| JST-PH-2 2mm to JST 1.25mm | 0.85        | https://www.laskakit.cz/jst-ph-2-2mm-do-jst-gh-2-1-25mm-adapter-pro-baterie/                            |
| Total                      | 144,35      |

## HackClub is fire! 🔥
https://blueprint.hackclub.com/projects/11552
