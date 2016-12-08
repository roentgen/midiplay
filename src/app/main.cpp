#include <stdio.h>
#include <string>
#include <list>
#include <regex>
#include <algorithm>
#include <thread>
#include <functional>
#include <stdlib.h>
//#include "json/parse.hpp"
#include <iostream>
#include "led.hpp"
#include "snd/snd.hpp"
#include "midi/midi.hpp"

struct node_t {
	int prog;
	int patch;
	double bpm;
	int count;
	std::string pcmfile;
	std::string seqfile;
};


// patch:0 pcm:filename bpm:xxx seq:filename

std::list< node_t > parse_text(const char* fn)
{
	FILE* fp = fopen(fn, "r");
	std::list< node_t > v;
	if (fp == nullptr) {
		return v;
	}
	char buf[1024];
	std::regex pat(R"([a-z]+\:[^ ]+)");
	while (fgets(buf, 1023, fp)) {
		std::cmatch l;
		std::regex_search(buf, l, std::regex("^[^#\n]+"));
		bool found_patch = false;
		bool found_pcm = false;
		if (l.str().length() > 0) {
			node_t n;
			std::string str = l.str(); /* l.str() */
			std::sregex_iterator it = std::sregex_iterator(str.cbegin(), str.cend(), pat); // split to token (K:V)
			std::sregex_iterator end = std::sregex_iterator();
			for ( ; it != end; ++ it) {
				std::smatch kv;
				std::regex_match(it->str(), kv, std::regex(R"(([a-z]+):([^ ]+))"));
				if (kv.size() > 2) {
					std::string key = kv[1].str();
					std::string value = kv[2].str();
					printf("k:%s v:%s <= [%s] match:%d\n", key.c_str(), value.c_str(), it->str().c_str(), it->size());
					n.prog = 0;
					if (key == "patch") {
						n.patch = atoi(value.c_str());
						found_patch = true;
					}
					else if (key == "pcm") {
						n.pcmfile = value;
						found_pcm = true;
					}
					else if (key == "seq") {
						n.seqfile = value;
					}
					else if (key == "bpm") {
						n.bpm = strtod(value.c_str(), nullptr);
					}
					else if (key == "count") {
						n.count = atoi(value.c_str());
					}
				}
			}
			if (found_pcm && found_patch) 
				v.push_back(n);
		}
	}
	fclose(fp);
	return v;
}

int load_seq(const node_t& n)
{
	return 0;
}

int load_pcm(const node_t& n)
{
	return 0;
}

snd::device_t* snd_ = nullptr;
midi::device_t* midi_ = nullptr;
rpi::led_device_t* led_ = nullptr;
std::list< node_t > ents;

node_t lookup(int pg, int patch)
{
	return std::find_if(ents,begin(), ents.end(), [=pg,patch](const node_t& t) { return t.prog == pg && t.patch == patch; }
}

void command_func()
{
	using namespace port;
	midi::midi_client cl;
	cl.init(midi_);
	int state = 0;

	volatile std::atomic< bool > loading = false;
	volatile std::atomic< size_t > pos = 0ULL;
	/* FIXME: FIFO にしよう */
	uint16_t* pcm_buffer = (uint16_t*)malloc(128*1024*1024);
	auto loadfunc = [&](FILE* fp) {
		const size_t u = sizeof(uint16_t) * 2;
		do {
			/* 16M = 16bit/stereo 48KHz で 96 sec くらい */
			size_t r = fread(pcm_buffer + pos, u, (16 * 1024 * 1024)/u);
			if (r <= 0)
				break;
			pos += r;
		}
		while (loading);
	};

	std::thread< void(FILE*) > pcm_load_thr;
	try {
		cmd_t cmd;
		while (cl.get(&cmd) == 0) {
			switch (cmd.tag) {
			case CHANGE_PROG:
				int pg = cmd.arg;
				node_t cur = lookup(pg);
				
				load_pcm(cur);
				FILE* fp = fopen(cur.pcmfile.c_str(), "rb");
				if (fp) {
					loadfunc(fp);
					loading = true;
					pcm_load_thr = std::move(std::thread(loadfunc, fp));
				}
				
				ready(led_);
				state = 1;
				break;
			case START:
				if (state == 1) {
					state = 2;
					// playback
				}
				break;
			case STOP:
				if (state = 2) {
					// stop;
					loading = false;
					pcm_load_thr.join();
					state = 1;
				}
				break;
			default:
				break;
			}
		}
	}
	catch () {
	}

	free(pcm_buffer);
}

int main(int argc, char** argv)
{
	snd_ = snd::init_sound();
	midi_ = midi::init_midi();
	led_ = led::init_led(led::LED_MODE_ONESHOT, 0);
	
	printf("load config:%s\n", argv[1]);
	//json::Parser conf;
	//auto ent = conf.parse(argv[1]);
	ents = parse_text(argv[1]);
	
	for (auto& e : ents) {
	}

	std::thread comthr();
	comthr.join();
	
	return 0;
}
