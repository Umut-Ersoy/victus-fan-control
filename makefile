CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -pedantic

TARGET = victus-fan-control
SRC = victus-fan-control.c
SERVICE_IN = victus-fan-control.service.in
SYSTEMD_SERVICE = /etc/systemd/system/victus-fan-control.service

.PHONY: all clean install service-install uninstall service-uninstall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)

install service-install: $(TARGET)
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: 'make install' must be run with root privileges (e.g. sudo make install)"; \
		exit 1; \
	fi
	@echo "Configuring systemd service pointing to $(CURDIR)..."
	@sed "s|@WORKING_DIR@|$(CURDIR)|g" $(SERVICE_IN) > $(SYSTEMD_SERVICE)
	@chmod 644 $(SYSTEMD_SERVICE)
	@systemctl daemon-reload
	@echo "Enabling and starting $(TARGET) service..."
	@systemctl enable --now $(TARGET).service
	@echo "Service successfully installed and started."
	@echo "Check status with: systemctl status $(TARGET).service"

uninstall service-uninstall:
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: 'make uninstall' must be run with root privileges (e.g. sudo make uninstall)"; \
		exit 1; \
	fi
	@echo "Stopping and disabling $(TARGET) service..."
	@-systemctl stop $(TARGET).service 2>/dev/null || true
	@-systemctl disable $(TARGET).service 2>/dev/null || true
	@echo "Removing $(SYSTEMD_SERVICE)..."
	@rm -f $(SYSTEMD_SERVICE)
	@systemctl daemon-reload
	@echo "Ensuring fans are returned to automatic/BIOS mode..."
	@-for p in /sys/devices/platform/hp-wmi/hwmon/hwmon*/pwm1_enable; do \
		if [ -f "$$p" ]; then echo 2 > "$$p" 2>/dev/null || true; fi; \
	done
	@echo "Uninstall complete. You may now delete this directory."
