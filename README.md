# Notecard-powered Build Monitor

An ESP32 & Cellular IoT Build Status light that monitors the state of GitHub Actions-powered workflows. The full [project writeup is on Hackster.io](https://www.hackster.io/brandonsatrom/esp32-cellular-iot-ci-build-status-light-f30047)

This repository contains the complete source for this project, including:

- Complete [firmware](firmware/firmware.ino) for the ESP32 connected to a Notecard via a [Notecarrier-AF](https://shop.blues.io/products/feather-starter-kit).
- GitHub Actions [Workflow files](github-actions/) with status notification steps.

![Gif of the Build Monitor flashing green](assets/FINAL.gif)
