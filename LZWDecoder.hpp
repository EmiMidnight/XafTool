#pragma once
#include <vector>

class LZWDecoder {
public:
	LZWDecoder(const uint8_t* input, size_t inputSize, uint32_t decompressedSize = 1024, uint32_t startCodeSize = 9)
		: input(input), inputSize(inputSize), inputPos(0),
		codeSize(startCodeSize), initialCodeSize(startCodeSize),
		bitBuffer(0), bitCount(0), dictLimit(1 << startCodeSize)
	{
		// The XAF File entries store the decompressed size, so we can use it to preallocate/reserve the output buffer. Yay.
		output.reserve(decompressedSize);
		dictionary.resize(4096 * 8, 0);
		resetDictionary();
	}

	std::vector<uint8_t> decode() {
		uint32_t prevCode = 0;
		bool first = true;

		while (true) {
			int code = readBits(codeSize);
			if (code < 0) break;

			if (code >= dictSize) {
				temp.clear();
				getSequence(prevCode, temp);
				std::reverse(temp.begin(), temp.end());
				temp.push_back(temp.front());
			}
			else {
				temp.clear();
				getSequence(code, temp);
				std::reverse(temp.begin(), temp.end());
			}

			for (uint8_t c : temp)
				output.push_back(c);

			if (!first) {
				addToDictionary(prevCode, temp.front());
			}
			prevCode = code;
			prevChar = temp.front();
			first = false;

			if (dictSize == dictLimit - 1) {
				if (codeSize < 12) {
					codeSize++;
					dictLimit = 1 << codeSize;
				}
				else {
					resetDictionary();
					codeSize = initialCodeSize;
					dictLimit = 1 << codeSize;
					first = true;
				}
			}
		}
		return output;
	}

private:
	const uint8_t* input;
	size_t inputSize;
	size_t inputPos;

	uint32_t codeSize;
	uint32_t initialCodeSize;
	uint32_t dictLimit;
	uint32_t dictSize = 0;

	uint32_t bitBuffer = 0;
	int bitCount = 0;

	std::vector<uint8_t> dictionary;
	std::vector<uint8_t> temp;
	std::vector<uint8_t> output;

	uint8_t prevChar = 0;

	int readBits(int n) {
		while (bitCount < n) {
			if (inputPos >= inputSize) return -1;
			bitBuffer = (bitBuffer << 8) | input[inputPos++];
			bitCount += 8;
		}
		int shift = bitCount - n;
		int code = (bitBuffer >> shift) & ((1 << n) - 1);
		bitBuffer &= (1 << shift) - 1;
		bitCount = shift;
		return code;
	}

	void resetDictionary() {
		dictSize = 0;
		for (int i = 0; i < 256; ++i) {
			dictionary[i * 8 + 0] = (uint8_t)i;
			*(uint32_t*)(&dictionary[i * 8 + 4]) = 0xFFFFFFFF;
			dictSize++;
		}
	}

	void getSequence(uint32_t code, std::vector<uint8_t>& out) {
		while (true) {
			uint8_t symbol = dictionary[code * 8];
			out.push_back(symbol);
			uint32_t next = *(uint32_t*)(&dictionary[code * 8 + 4]);
			if (next == 0xFFFFFFFF) break;
			code = next / 8;
		}
	}

	void addToDictionary(uint32_t prefixCode, uint8_t newChar) {
		if (dictSize >= 4096) return;
		dictionary[dictSize * 8] = newChar;
		*(uint32_t*)(&dictionary[dictSize * 8 + 4]) = prefixCode * 8;
		dictSize++;
	}
};