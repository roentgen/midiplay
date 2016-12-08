#include "led.hpp"

namespace rpi {
led_device_t* init_led(int mode, int idx);
{
	auto d = new led_device_t;
	d->mode = mode;
	d->idx = idx;

	char buf[512];
	sprintf(buf, "/sys/class/leds/led%d/trigger", idx);
	d->trigger_path = std::string(buf);
	return d;
}
}
