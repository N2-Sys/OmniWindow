#ifndef _LOSS_RADAR_H
#define _LOSS_RADAR_H

#include "sketchBase.h"

class CustomizedKey {
public:
	Key_t flowID;
	uint32_t IP_ID;
	char str[CHARKEY_LEN + 4];

	CustomizedKey(Key_t k, uint32_t ip_id):flowID(k), IP_ID(ip_id) {
		memcpy(str, flowID.str, CHARKEY_LEN );
		memcpy(str + CHARKEY_LEN, &IP_ID, 4);
	}

	CustomizedKey() {
		IP_ID = 0;
		memset(str, 0, CHARKEY_LEN + 4);
	}

	bool operator < (const CustomizedKey &c) {
		if (this->flowID < c.flowID)
			return true;
		else if (this->flowID == c.flowID && this->IP_ID < c.IP_ID)
			return true;
		return false;
	}

	bool operator == (const CustomizedKey &c) {
		if (this->flowID == c.flowID && this->IP_ID == c.IP_ID)
			return true;
		return false;
	}

	CustomizedKey & operator ^=(const CustomizedKey &c) {
		this->flowID ^= c.flowID;
		this->IP_ID ^= c.IP_ID;
		memcpy(str, flowID.str, CHARKEY_LEN + 1);
		memcpy(str + CHARKEY_LEN + 1, &IP_ID, 4);
		return *this;
	}
};

template<typename T, uint32_t sz, uint32_t hash_num>
class LossRadar {
public:
	CustomizedKey *xor_sum;
	T *packet_sum;
	uint32_t size = calNextPrime(sz);
	uint64_t h[hash_num], s[hash_num], n[hash_num];

	std::set<Key_t> lost_flows;

	LossRadar() {
		xor_sum = new CustomizedKey[size];
		packet_sum = new T[size];
		memset(packet_sum, 0, size * sizeof(T));

		int index = 0;
		for (int i = 0; i < hash_num; ++i) {
			h[i] = GenHashSeed(index++);
			s[i] = GenHashSeed(index++);
			n[i] = GenHashSeed(index++);
		}
	}

	uint32_t hash(CustomizedKey key, int i) {
		return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN + 4, h[i], s[i], n[i]) % size);
	}

	void insert(CustomizedKey k) {
		std::set<uint32_t> pos_set;
		for (int i = 0; i < hash_num; ++i) {
			pos_set.insert(hash(k, i));
		}
		for (int i = 0; pos_set.size() < hash_num; ++i) {
			pos_set.insert((*pos_set.begin() + i) % size);
		}

		for (auto it = pos_set.begin(); it != pos_set.end(); ++it) {
			xor_sum[*it] ^= k;
			packet_sum[*it]++;
		}
	}

	LossRadar & operator ^= (const LossRadar &lr) {
		for (int i = 0; i < size; ++i) {
			this->xor_sum[i] ^= lr.xor_sum[i];
			this->packet_sum[i] -= lr.packet_sum[i];
		}
		return *this;
	}

	void dump() {
		while (1) {
            bool stop = true;
			for (int i = 0; i < size; ++i) {
				if (packet_sum[i] == 1) {
					std::set<uint32_t> pos_set;
					for (int j = 0; j < hash_num; ++j) {
						pos_set.insert(hash(xor_sum[i], j));
					}
					for (int j = 0; pos_set.size() < hash_num; ++j) {
						pos_set.insert((*pos_set.begin() + i) % size);
					}

					for (auto it = pos_set.begin(); it != pos_set.end(); ++it) {
						if (*it != i) {
							xor_sum[*it] ^= xor_sum[i];
							packet_sum[*it]--;
						}
					}

					lost_flows.insert(xor_sum[i].flowID);
					xor_sum[i] ^= xor_sum[i];
					packet_sum[i]--;
                    stop = false;
                    // std::cout << i << std::endl;

				}
			}
			if (stop)
				break;
		}
	}

	size_t getMemoryUsage() {
		return (CHARKEY_LEN + sizeof(T)) * size;
	}
};

#endif