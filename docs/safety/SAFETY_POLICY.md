# SkullGate — Safety Policy

## Intended Use

SkullGate is a security research and education platform. It is intended for use on:

- **Your own devices** (ESP32 boards you own)
- **Your own networks** (networks you operate or have explicit written permission to test)
- **Explicitly permitted lab environments** (penetration testing labs with proper authorisation)

## Prohibited Use

The following uses are **strictly prohibited**:

- Scanning, probing, or accessing networks, devices, or systems you do not own or have explicit written permission to test
- Intercepting network traffic without consent of all parties
- Disrupting wireless communications (e.g. deauthentication attacks)
- Any activity that violates local, national, or international law
- Using SkullGate as a weapon or harassment tool

## Legal Notice

Radio scanning and wireless security testing are regulated activities. Laws vary by jurisdiction. Some relevant regulations include:

- **US:** Computer Fraud and Abuse Act (CFAA), Electronic Communications Privacy Act (ECPA), FCC regulations
- **UK:** Computer Misuse Act 1990, Wireless Telegraphy Act 2006
- **EU:** Directive on Attacks Against Information Systems, national implementations
- **Australia:** Criminal Code Act 1995, Radiocommunications Act 1992

**Consult a qualified legal professional in your jurisdiction before deploying SkullGate.**

## Recon-Only Mode

SkullGate boots in **Recon-Only mode** by default. In this mode:

- Wi-Fi scanning is **passive** (listens for beacons; does NOT transmit probe requests)
- No AP association is attempted
- No BLE advertising or connection is initiated
- No network transmissions are made

Passive scanning is generally considered legal in most jurisdictions for your own premises, but consult local laws.

## Lab Mode

Lab Mode enables active features such as Wi-Fi connection, BLE advertising, and proxy tools. It requires:

1. Physical access to the device (SD card slot)
2. A flag file `/lab_mode.flag` on the SD card
3. A PIN entered at the device prompt

Lab Mode features must only be used in explicitly permitted environments.

## Reporting Vulnerabilities

If you discover a security vulnerability in SkullGate, please report it responsibly via GitHub Issues with the `security` label or via private disclosure to the maintainers.

Do NOT publish proof-of-concept exploits targeting production infrastructure.
