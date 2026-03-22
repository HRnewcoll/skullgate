# skullgate
SkullGate — Command the Dark. Modular, board‑agnostic ESP cyber‑deck OS. 
# SkullGate — Command the Dark

SkullGate is a modular, board‑agnostic ESP cyber‑deck OS designed for handheld devices.  
It is built for **local lab testing**, **education**, and **modular expansion** — not for misuse.

## Core Principles
- Modular plugin system (SD‑based modules)
- Board‑agnostic HAL (works on any ESP board)
- Safe defaults (Recon‑Only on boot)
- Lab Mode gating for active features
- Touchscreen UI + headless mode
- Radio expansion (LoRa, Sub‑GHz, NFC, IR)
- Local‑only proxy/VPN tools for your own devices
- Mesh chat, BLE tools, dashboards, games

## Status
Phase‑1 development: HAL, module loader, UI skeleton, Wi‑Fi scanner.

## Safety Notice
SkullGate is for **your own devices**, **your own networks**, and **explicitly permitted lab environments** only.

SkullGate: A Modular, Board-Agnostic ESP Cyber-Deck OS for Handheld Devices
Executive Summary

SkullGate aspires to be the definitive open-source, modular firmware platform for ESP32-based cyber-decks and handhelds, focusing on extensibility, board-agnostic design, and robust support for wireless reconnaissance, mesh networking, radio modules, and advanced UI. This report delivers a comprehensive, actionable blueprint for building SkullGate, synthesizing the best practices, code patterns, and hardware integration strategies from the current ESP32 ecosystem. It covers project-by-project analysis of relevant open-source repositories, hardware driver research, networking and security feasibility, modular firmware architecture, web flashing tooling, and a prioritized implementation roadmap. The recommendations emphasize stability, maintainability, and extensibility, targeting the "golden path" of the ESP32-S3 ecosystem. All findings are grounded in up-to-date, credible references and are tailored for firmware developers seeking to build or contribute to SkullGate.
