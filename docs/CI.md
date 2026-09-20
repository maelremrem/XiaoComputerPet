# GitHub Actions

## Web Configurator

`pages.yml` deploys the static `web/` directory to GitHub Pages. Enable **Settings → Pages → Source: GitHub Actions** once after the first push.

## Firmware

`firmware.yml` installs PlatformIO, builds `seeed_xiao_nrf52840`, runs the size target and uploads firmware artifacts for 30 days.
