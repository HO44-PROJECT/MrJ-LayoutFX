SHELL := /bin/bash
PORT ?= 8000

.PHONY: web-installer web-installer-build web-installer-serve web-installer-stop help

help:
	@echo "make web-installer        - build firmware + assemble web-installer/ + serve on http://localhost:$(PORT)"
	@echo "make web-installer-build  - build firmware and assemble web-installer/ only (no server)"
	@echo "make web-installer-serve  - serve the already-assembled web-installer/ on http://localhost:$(PORT)"
	@echo "make web-installer-stop   - stop a background server started with web-installer-serve"

web-installer-build:
	pio run -e web_installer
	python3 scripts/build_web_installer.py --env web_installer

web-installer-serve:
	cd web-installer && python3 -m http.server $(PORT)

web-installer: web-installer-build web-installer-serve

web-installer-stop:
	@pkill -f "http.server $(PORT)" 2>/dev/null && echo "stopped server on :$(PORT)" || echo "no server found on :$(PORT)"
