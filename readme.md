# MeshAQ

## Sensors - not finalized
| Measurement       | Unit  | Sensor                | Significance                                                                      | Unhealthy level - suden spikes | Unhealthy level - 24h average                |
| ----------------- | ----- | --------------------- | --------------------------------------------------------------------------------- | ------------------------------ | -------------------------------------------- |
| Temperature       | °C    | SEN66                 | Affects human comfort and thermoregulation; influences pollutant chemistry.       |                                | >30°C (heat stress risk)                     |
| Relative humidity | %     | SEN66                 | Impacts respiratory comfort, aerosol growth, and mould/virus survival.            |                                | <30% (dry) or >60% (mould/comfort issues)    |
| PM (PM2.5)        | µg/m³ | SEN66                 | Fine particles penetrate lungs/bloodstream; linked to cardio-respiratory disease. |                                | annual ≤5 µg/m³; 24‑h >15 µg/m³ (exceedance) |
| PM (PM10)         | µg/m³ | SEN66                 | Alergens, dust and visible particles                                              |
| VOC Index         | -     | SEN66                 | Sensor reports a relative VOC Index (unitless); indicates overall VOC load.       |                                | Index >100                                   |
| NOx (NO2)         | -     | SEN66                 | Sensor reports a relative NOx Index (unitless); NO2 is a respiratory irritant.    |                                | Index >100                                   |
| CO2               | ppm   | SEN66                 | Proxy for ventilation and occupancy; high levels impair cognition.                |                                | >1000 ppm (poor ventilation)                 |
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


## HackClub is fire! 🔥
https://blueprint.hackclub.com/projects/11552
