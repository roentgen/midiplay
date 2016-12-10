#include <stdio.h>
#include <unistd.h>
#include "midi.hpp"
#include "port/cmdq.hpp"
#include <thread>

namespace midi {

/*

BOSS SY-300
https://static.roland.com/jp/media/pdf/SY-300_MIDI_Imple_j01.pdf

SY-300 | (midi OUT) ---> [midi IN]=====>(Tab)===> plughw:UMONE

(SY-300 MIDI Setting を 'PC OUT' にする.
当たり前だが SEND TEMPO は OFF っておかないと山ほどくる.
Ctrl は設定可能.

default Ctrl1=81, Ctrl2=82, Ctrl3=83

Channle Voice Message:

先頭 0xB に続く 4bit は SYSTEM->MIDI Setting->TX channel で設定される sendor ID.

Bank no は:
Bank=0 U**
Bank=1 P**

)

Power ON/OFF => No control sent (めんどくせえ)

ctrl2,3 で patch 切り替えした場合も、 SELECT でパッチ切り替えした場合も同じ信号がくる.
Page や menu などに遷移してから(液晶モニターが)パッチ画面に戻っても、信号は来ない。
ctrl2.3 を押しっぱなしにしている間にも、単押しで来たのと同じ信号が連続してくる。押しっぱなし開始、解除といった状態遷移はしない。

ctr1

push
ch:b0
ch:51
ch:7f

up
ch:b0
ch:51
ch:00

ctrl2/down
eg)u79->u78

ch:b0
ch:00
ch:00

ch:b0
ch:20
ch:00

ch:c0
ch:4d

ctrl3/up 
eg)u78->79

ch:b0
ch:00
ch:00

ch:b0
ch:20
ch:00

ch:c0
ch:4e


ctrl4,5/exp

push
ch:b0
ch:54
ch:7f

up
ch:b0
ch:54
ch:00
*/



static const int NONE = 0;
static const int SYSTEM_EXCLUSIVE = 1;
static const int CH_VOICE = 2;
static const int PROGRAM_CHANGE = 3;
static const int WAIT_BANK_NUMBER = 4;
static const int WAIT_BANK_END = 5;
static const int WAIT_CONTROL_VALUE = 6;

struct lex_statemachine {
	int bytes_;
	int state_;
	int senderch_;
	int bank_;
	int ctrl_;
	int value_;
	uint8_t buf_[16];
public:
	lex_statemachine() : bytes_(0), state_(NONE), senderch_(0), bank_(0), ctrl_(0), value_(0)
	{
	}

	int put(uint8_t b)
	{
		if (state_ == SYSTEM_EXCLUSIVE || b == 0xf8 || b == 0xfe) {
			/* 0xf* などのリアルタイムメッセージは無視.
			   (Channel Voice Message が複数のメッセージで構成され、 Atomic かどうかわからないため) 
			   System Exclusive Message 以外では state を変更しないため特権的に受ける.
			   0xf* で始まるものは、他のメッセージにはないはず(ASCII か 0-0x7f の範囲なので)
			*/
			if (b == 0xf8) {
				/* Timing Clock */
				return -1;
			}
			else if (b == 0xfe) {
				/* Active Sencing */
				return -1;
			}
			else if (b == 0xf0) {
				/* System Exclusive Message 0xf0...0xf7 */
				state_ = SYSTEM_EXCLUSIVE;
			}
			else if (b == 0xf7) {
				state_ = NONE; // End of System Exclusive Message
				return SYSTEM_EXCLUSIVE;
			}
			return 0;
		}
		/* SEM 以外のメッセージ */
		if (state_ == NONE) {
			/* bulk の状態 */
			uint8_t msg = b & ~0xf;
			senderch_ = b & 0xf;
			/* 値の範囲的に、 bulk で 0xc0-0xb0 の値がくることはないため、字句的に解釈してよさそう */
			if (msg == 0xb0) {
				state_ = CH_VOICE;
			}
			else if (msg == 0xc0) {
				state_ = PROGRAM_CHANGE;
			}
			return unresolve(b);
		}
		else if (state_ == PROGRAM_CHANGE) {
			value_ = b;
			buf_[bytes_] = b;
			resolve();
			return PROGRAM_CHANGE;
		}
		else if (state_ == CH_VOICE) {
			/* channel voice message メッセージの 2 byte 目 */
			if (b == 0x00) {
				state_ = WAIT_BANK_NUMBER;
			}
			else if (b == 0x20) {
				state_ = WAIT_BANK_END;
			}
			else {
				ctrl_ = b;
				state_ = WAIT_CONTROL_VALUE;
			}
			return unresolve(b);
		}
		else if (state_ == WAIT_BANK_NUMBER) {
			bank_ = b;
			buf_[bytes_] = b;
			resolve();
			return WAIT_BANK_NUMBER;
		}
		else if (state_ == WAIT_BANK_END) {
			buf_[bytes_] = b;
			resolve();
			return WAIT_BANK_END;
		}
		else if (state_ == WAIT_CONTROL_VALUE) {
			value_ = b;
			resolve();
			return WAIT_CONTROL_VALUE;
		}
		else {
			// all ignore 
		}
		return 0; // unresolved 
	}

	int unresolve(uint8_t b)
	{
		buf_[bytes_] = b;
		bytes_ ++;
		return 0;
	}
	
	void resolve()
	{
		bytes_ = 0;
		state_ = NONE;
	}
};

static const int CTRL_PUSH = 127;

class statemachine {
	int state_;
	lex_statemachine lex_;

	std::function< void(int, int) > push_;
	std::function< void(int, int, int) > change_prog_;
public:
	statemachine() : state_(NONE) {}

	void set_ctrl_push(std::function< void(int, int) >&& handler)
	{
		push_ = handler;
	}

	void set_change_prog(std::function< void(int, int, int) >&& handler)
	{
		change_prog_ = handler;
	}

	bool put(uint8_t b)
	{
		int was = lex_.put(b);
		if (was <= 0 || was == SYSTEM_EXCLUSIVE)
			return 0;
		//printf("state_:%d event:%d\n", state_, was);
		if (state_ == NONE) {
			if (was == WAIT_CONTROL_VALUE) {
				if (lex_.value_ == 0x7f) {
					/* Edge up */
					state_ = CTRL_PUSH;
				} 
				else {
					/* TODO: Bulk の control value change もありそうだが、 ctrl ごとの Edge/Level の設定が必要.
					   とりあえず全部 Edge の立ち下がりとする. */
				}
			}
			else if (was == WAIT_BANK_NUMBER) {
				state_ = WAIT_BANK_END;
			}
			else {
			}
		}
		else if (state_ == CTRL_PUSH) {
			if (was == WAIT_CONTROL_VALUE) {
				if (lex_.value_ == 0) {
					/* Edge down: ctrl ボタンを離した */
					push_(lex_.senderch_, lex_.ctrl_);
				}
				state_ = NONE;
				return true;
			}
		}
		else if (state_ == WAIT_BANK_END) {
			/* 
			   BANK NO -> BANK END -> CHG PROG
			   
			   CHG PROG は bulk でもユニークなので lex で解析可能だが、
			   bank_no:prog_no で一意になる都合上、必ず直前に BANK SEL を伴う.
			   そのためその状態遷移を期待する.
			*/
			if (was == WAIT_BANK_END)
				state_ = PROGRAM_CHANGE;
			else
				;
		}
		else if (state_ == PROGRAM_CHANGE) {
			if (was == PROGRAM_CHANGE) {
				int bank = lex_.bank_;
				int prog = lex_.value_;
				change_prog_(lex_.senderch_, bank, prog);
				state_ = NONE;
				return true;
			}
		}
		return false;
	}
};

int midi_client::init(device_t* d)
{
	dev_ = d;
	listening_ = true;
	thr_ = std::move(std::thread([this](){
				port::cmd_t cmd = {0, 0, 0};
				statemachine fsm;
				fsm.set_ctrl_push([&](int sender, int ctrl) {
						printf("[CTRL %d pushed] from %d\n", ctrl - 0x50, sender);
						cmd.tag = port::TAP_CTRL;
						cmd.data0 = ctrl - 0x50;
					});
				fsm.set_change_prog([&](int sender, int bank, int prog) {
						cmd.tag = port::CHANGE_PROG;
						cmd.data0 = prog + 1;
						printf("[PROG %d:%d selected] from %d\n", bank, prog + 1, sender);						
					});
				while (listening_) {
					uint8_t ch;
					ch_read(dev_, &ch);
					if (fsm.put(ch)) {
						put(std::move(cmd));
					}
#if defined(__linux__)
					//printf("ch:%02x\n", ch);
#else
					if (ch == '0') {
						cmd.tag = port::CHANGE_PROG;
						cmd.data0 = 0;
					}
					else if (ch == '1') {
						cmd.tag = port::CHANGE_PROG;
						cmd.data0 = 1;
					}
					else if (ch == 'p') {
						cmd.tag = port::START;
					}
					else if (ch == 'b') {
						cmd.tag = port::STOP;
					}
					else {
						continue;
					}
					put(std::move(cmd));
#endif
				}
			}));
	return 0;
}

int midi_client::get(port::cmd_t* cmd)
{
	*cmd = q_.wait();
	return 0;
}

int midi_client::put(port::cmd_t&& cmd)
{
	q_.send(std::move(cmd));
	return 0;
}

device_t* init_midi(const std::string& device)
{
#if defined(__linux__)
	int err;
	snd_rawmidi_t* hdlin;
	snd_rawmidi_t* hdlout;
	printf("raw midi device:%s\n", device.c_str());
	err = snd_rawmidi_open(&hdlin, &hdlout, device.c_str(), 0);
	if (err < 0) {
		printf("Control midi device open error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}
	auto dev = new device_t;
	dev->hdlin = hdlin;
	dev->hdlout = hdlout;
	return dev;
#else
	auto dev = new device_t;
	dev->fdin = 1;
	dev->fdout = 0;
	return dev;
#endif
}

void final_midi(device_t* dev)
{
#if defined(__linux__)
	snd_rawmidi_close(dev->hdlin);
	snd_rawmidi_close(dev->hdlout);
	dev->hdlin = nullptr;
	dev->hdlout = nullptr;
#endif
	delete dev;
}

int ch_read(device_t* dev, uint8_t* ch)
{
#if defined(__linux__)
	snd_rawmidi_read(dev->hdlin, ch, 1); /* blocking */
#else
	while (read(dev->fdin, ch, 1) == EAGAIN) {
		usleep(100000);
	}
#endif
	return 0;
}

int ch_write(device_t* dev, const uint8_t* ch, int cnt)
{
#if defined(__linux__)
	snd_rawmidi_write(dev->hdlout, ch, cnt); /* blocking */
	snd_rawmidi_drain(dev->hdlout);
#else
	write(dev->fdout, ch, cnt);
#endif
	return 0;

}
}
