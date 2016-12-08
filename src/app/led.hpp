#if !defined(LED_HPP__)
#define LED_HPP__

#include <string>

namespace rpi {
	static const int LED_MODE_NONE = 0;
	static const int LED_MODE_HEARTBEAT = 1;
	static const int LED_MODE_ONESHOT = 2;
	struct led_device_t {
		int mode;
		int idx;
		std::string trigger_path;
	};
	
	led_device_t* init_led(int mode, int idx);
}

#endif
