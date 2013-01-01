#pragma once

template <typename T>
T stepTowards(T initial, T target, T step) {
	if (initial < target) {
		initial += step;
		if (initial > target)
			return target;
		return initial;
	} else if (initial > target) {
		initial -= step;
		if (initial < target)
			return target;
		return initial;
	} else {
		return initial;
	}
}

template <typename T>
T clamp(T min, T val, T max) {
	if (val < min) return min;
	else if (val > max) return max;
	else return val;
}
