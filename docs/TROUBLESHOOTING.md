# Troubleshooting

Repeated `AT` attempts with no `OK` are expected when the SIM800L is physically disconnected. Connect and power the modem correctly before treating this as a firmware or network failure.

- `CPIN` is not `READY`: unlock or reseat the SIM; do not log the PIN.
- `CREG` is not `1` or `5`: verify antenna, SIM activation, GSM/2G coverage, power stability, and operator registration.
- `CSQ` is `99`: signal is unknown; reposition the antenna and recheck registration.
- `SAPBR` fails: confirm MTN APN `web.gprs.mtnnigeria.net`, active data bundle, registration, and modem power.
- `HTTPACTION` returns `601/603`: inspect bearer/DNS/network configuration. A `4xx` or `5xx` status is not a successful sync.
- `AT+HTTPSSL=1` fails or HTTPS handshake fails: the installed SIM800L firmware may not support the server's current TLS requirements. Do not downgrade Supabase traffic to HTTP. Record the modem firmware and test results before designing a tightly scoped HTTPS-terminating gateway.
- AHT20/BMP280 fails: scan the I2C bus, check 3.3/5 V compatibility of the exact breakouts, verify A4/A5 and common ground. BMP280 is tried at `0x76` then `0x77`; LCD is expected at `0x27`.
