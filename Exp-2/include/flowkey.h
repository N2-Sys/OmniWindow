#ifndef _FLOWKEY_H
#define _FLOWKEY_H

#include "config.h"
#include "cstring"

const char emptyKey[CHARKEY_LEN] = {0};

typedef uint32_t IPKey_t;

typedef struct five_tuple {
	char str[CHARKEY_LEN];
	bool operator < (const five_tuple& a) const {
		for (int i = 0; i < CHARKEY_LEN; ++i) {
			if (str[i] < a.str[i])
				return true;
			else if (str[i] == a.str[i])
				continue;
			else return false;
		}
		return false;
	}
	bool operator == (const five_tuple& a) const {
		for (int i = 0; i < CHARKEY_LEN; ++i) {
			if (str[i] != a.str[i])
				return false;
		}
		return true;
	}
	inline bool isEmpty() {
		return (strcmp(str, emptyKey) == 0);
	}
	five_tuple() {
		memset(str, 0, sizeof(str));
	}
	five_tuple(char *s) {
		memcpy(str, s, CHARKEY_LEN);
	}
	five_tuple(const char *s) {
		memcpy(str, s, CHARKEY_LEN);
	}
	five_tuple & operator ^=(const five_tuple &c) {
		for (int i = 0; i < CHARKEY_LEN; ++i) {
			str[i] ^= c.str[i];
		} 
		return *this;
	}
} Key_t;

typedef struct data_t {
	Key_t key;
	uint32_t length;
	double timestamp;
	data_t(char *s) {
		key = Key_t(s);
		length = *(uint32_t *)(s + CHARKEY_LEN);
		timestamp = *(double *)(s + CHARKEY_LEN + 4);
	}
	data_t(const char *s) {
		key = Key_t(s);
		length = *(uint32_t *)(s + CHARKEY_LEN);
		timestamp = *(double *)(s + CHARKEY_LEN);
	}
	data_t() {
		timestamp = 0;
		length = 0;
	}
} data_t;

#endif


