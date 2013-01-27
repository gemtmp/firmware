/*
 * Regulator.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef REGULATOR_H_
#define REGULATOR_H_

template <typename D, int Up, int Down>
class Action {
public:
	static void stop() {
		D::data &= ~((1 << Up) | (1 << Down));
	}
	static void up() {
		D::data &= ~(1 << Down);
		D::data |= (1 << Up);
	}
	static void down() {
		D::data |= (1 << Down);
		D::data &= ~(1 << Up);
	}
};

template <class Action>
class Regul {
public:
	typedef Action action_t;

	Regul(int8_t target)
		: target(target), lastAction("") {}

	void step(int8_t current) {
		int8_t err = current - target;

		if (err == 0) {
			Action::stop();
			lastAction = "stop";
		} else if (err < 0) {
			Action::up();
			lastAction = "up";
		} else {
			Action::down();
			lastAction = "down";
		}
	}
	void setTarget(uint8_t t) {
		target = t;
	}
	int8_t getTarget() const { return target; }
	const char* getLastAction() const { return lastAction; }
private:
	int8_t target;
	const char *lastAction;
};


#endif /* REGULATOR_H_ */
