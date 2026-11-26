REX GEN 2 — PLATFORMIO V1.6 MIGRATION

This is the PlatformIO conversion of the supplied REX_Gen2_Modular_V1_6
Arduino IDE archive. It preserves the modular feature code and existing
pin/touch calibration values, while moving shared headers into include/
and the application entry point to src/main.cpp.

SETUP
1. Open this folder in VS Code.
2. Open include/Secrets.h and enter your local 2.4 GHz Wi-Fi credentials.
3. Build first using PlatformIO: Project Tasks > esp32dev > General > Build.
4. Upload only after a successful build.

Hardware pin assignments and touch calibration are retained from the archive.
This package has not been compiled on the user's machine. Build and inspect
errors before uploading.

NOTE
This archive is a modular V1.6 package, not the original V1.5 .ino source.
It contains Home, Clock, System, Games, Settings, Connectivity, Navigation,
and Input modules. It does not include the separate Desktop Buddy module.

V1.7 DESKTOP BUDDY MODULE
- Added an independent src/Features/Buddy/BuddyFeature.cpp/.h module.
- Added BUDDY as the fifth home tile; existing pages remain available.
- Touch the Buddy face or PET button to increase bond.
- Touch FEED or press DOWN to increase energy.
- Press SELECT to pet; press UP to cycle Curious / Sleepy / Alert moods.
- BACK or the on-screen HOME control returns to the home page.
- Buddy animation is serviced without delay() in the main loop.

The V1.7 source update was prepared from the supplied V1.6 PlatformIO ZIP.
It has not been compiled in this environment; compile in PlatformIO before
uploading. Keep your existing working project as a backup.
