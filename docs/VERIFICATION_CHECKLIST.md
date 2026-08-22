# Verification checklist

| Requirement | Status | Evidence needed |
|---|---|---|
| Firmware source compiles for `arduino:avr:uno` | PASS | Configured build: 27,938 bytes flash (86%); 1,405 bytes global SRAM (68%) |
| Configured firmware uploaded to Uno-class target | PASS | Arduino CLI upload exited successfully on `/dev/ttyACM0`; authentication logging is redacted |
| Post-flash sensor boot diagnostics | PASS | AHT20 and BMP280 reported ONLINE; valid live reading displayed over serial |
| SIM800L initialization and MTN registration | DEFERRED | Modem/SIM hardware was not connected during the test |
| Sensor and LCD wiring operates | NOT VERIFIED | Real-device readings and display rotation |
| SIM800L registers on MTN | NOT VERIFIED | Live `CPIN`, `CREG`, `COPS`, and `CSQ` transcript without secrets |
| SIM800L negotiates Supabase HTTPS | NOT VERIFIED | Live `HTTPSSL` and successful `HTTPACTION: 1,201,...` |
| Schema/RLS applied | PASS | Project `gwisrdrvhwpawahlqajm`; tables inspected; security advisor has no findings |
| Edge Function deployed | PASS | `ingest-weather` version 1 is ACTIVE; JWT disabled for custom station authentication |
| Unsupported method is rejected | PASS | Live GET returned `405 method_not_allowed` |
| Endpoint remains closed without station secret | PASS | Live POST returned `503 service_unavailable`; no row inserted |
| Missing and wrong station keys rejected | PASS | Live requests returned `401` |
| Malformed and impossible measurements rejected | PASS | Live requests returned `400` and `422` |
| Authenticated request inserts one row | PASS | Live request returned `201`; query confirmed exactly one synthetic row with ID 1 |
| Dashboard typecheck/test/build | PASS | TypeScript, ESLint, Vitest, and Vite production build |
| Dashboard displays real data | PASS | Configured to the live project containing verified synthetic reading ID 1 |
| GitHub repository pushed | BLOCKED | Git initialization plus authenticated GitHub CLI |
| Android debug APK builds | PASS | Capacitor 8.4.2; Gradle `assembleDebug`; 4,339,279-byte APK |
| Android installation and runtime workflow | NOT VERIFIED | ADB daemon is blocked in this environment; install on a real Android device |
