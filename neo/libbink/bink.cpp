/*
 * LibBink
 * Copyright (C) 2026 Justin Marshall
 *
 * This file is part of LibBink by Justin Marshall(justinmarshall20@gmail.com).
 *
 * LibBink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * LibBink is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LibBink; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "bink.h"
#include "binkdata.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace
{
enum Source
{
	SOURCE_BLOCK_TYPES,
	SOURCE_SUB_BLOCK_TYPES,
	SOURCE_COLORS,
	SOURCE_PATTERN,
	SOURCE_X_OFF,
	SOURCE_Y_OFF,
	SOURCE_INTRA_DC,
	SOURCE_INTER_DC,
	SOURCE_RUN,
	SOURCE_COUNT
};

enum BlockType
{
	BLOCK_SKIP,
	BLOCK_SCALED,
	BLOCK_MOTION,
	BLOCK_RUN,
	BLOCK_RESIDUE,
	BLOCK_INTRA,
	BLOCK_FILL,
	BLOCK_INTER,
	BLOCK_PATTERN,
	BLOCK_RAW
};

struct BitReader
{
	unsigned char const *Data;
	size_t Size;
	size_t Bit;
	bool Failed;

	BitReader(unsigned char const *data, size_t size)
		: Data(data), Size(size), Bit(0), Failed(false)
	{
	}

	unsigned int Read(unsigned int count)
	{
		if (count > 32 || Bit > Size * 8 || count > Size * 8 - Bit) {
			Failed = true;
			return 0;
		}

		unsigned int value = 0;
		for (unsigned int index = 0; index < count; ++index) {
			value |= ((Data[Bit >> 3] >> (Bit & 7)) & 1U) << index;
			++Bit;
		}
		return value;
	}

	unsigned int Peek(unsigned int count)
	{
		size_t old_bit = Bit;
		bool old_failed = Failed;
		unsigned int value = Read(count);
		Bit = old_bit;
		Failed = old_failed;
		return value;
	}

	void Skip(unsigned int count)
	{
		Read(count);
	}

	void Align32()
	{
		unsigned int remainder = static_cast<unsigned int>(Bit & 31);
		if (remainder) {
			Skip(32 - remainder);
		}
	}
};

struct Tree
{
	unsigned char Codebook;
	unsigned char Symbols[16];
};

struct Bundle
{
	unsigned int LengthBits;
	Tree Huffman;
	unsigned char *Data;
	unsigned char *DataEnd;
	unsigned char *Decoded;
	unsigned char *Current;
};

struct Plane
{
	unsigned int Width;
	unsigned int Height;
	unsigned int Stride;
	std::vector<unsigned char> Pixels;
};

struct Frame
{
	Plane Planes[3];
};

struct AudioState
{
	uint32_t MaxDecodedBytes;
	unsigned int SampleRate;
	unsigned int Channels;
	unsigned int Flags;
	unsigned int FrameLength;
	unsigned int OverlapLength;
	unsigned int BandCount;
	unsigned int Bands[26];
	float QuantTable[96];
	bool First;
	std::vector<float> Previous;
	std::vector<float> Coefficients;
	std::vector<float> BlockOutput;
	std::vector<std::complex<float> > Transform;
	std::vector<float> PCM;

	AudioState()
		: MaxDecodedBytes(0),
		  SampleRate(0),
		  Channels(0),
		  Flags(0),
		  FrameLength(0),
		  OverlapLength(0),
		  BandCount(0),
		  First(true)
	{
		std::memset(Bands, 0, sizeof(Bands));
		std::memset(QuantTable, 0, sizeof(QuantTable));
	}
};

uint16_t ReadLE16(unsigned char const *data)
{
	return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t ReadLE32(unsigned char const *data)
{
	return static_cast<uint32_t>(data[0]) |
		(static_cast<uint32_t>(data[1]) << 8) |
		(static_cast<uint32_t>(data[2]) << 16) |
		(static_cast<uint32_t>(data[3]) << 24);
}

unsigned int IntegerLog2(unsigned int value)
{
	unsigned int result = 0;
	while (value >>= 1) {
		++result;
	}
	return result;
}

unsigned char ClampByte(int value)
{
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return static_cast<unsigned char>(value);
}

bool ReadExact(std::ifstream &stream, void *destination, size_t size)
{
	stream.read(static_cast<char *>(destination), static_cast<std::streamsize>(size));
	return stream.good() || static_cast<size_t>(stream.gcount()) == size;
}

bool Seek(std::ifstream &stream, uint32_t offset)
{
	stream.clear();
	stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	return stream.good();
}
}

struct cw_bink_decoder
{
	std::ifstream File;
	std::string Error;
	uint32_t FileSize;
	uint32_t Width;
	uint32_t Height;
	uint32_t FrameCount;
	uint32_t FpsNumerator;
	uint32_t FpsDenominator;
	uint32_t AudioTracks;
	std::vector<uint32_t> FrameOffsets;
	std::vector<unsigned char> FrameKeys;
	std::vector<unsigned char> Packet;
	std::vector<unsigned char> BundleStorage;
	Bundle Bundles[SOURCE_COUNT];
	Tree ColorHigh[16];
	int LastColor;
	Frame Video[2];
	int CurrentVideo;
	bool HasPrevious;
	uint32_t CurrentFrame;
	std::vector<unsigned char> RGB;
	AudioState Audio;

	cw_bink_decoder()
		: FileSize(0),
		  Width(0),
		  Height(0),
		  FrameCount(0),
		  FpsNumerator(0),
		  FpsDenominator(0),
		  AudioTracks(0),
		  LastColor(0),
		  CurrentVideo(0),
		  HasPrevious(false),
		  CurrentFrame(0)
	{
		std::memset(Bundles, 0, sizeof(Bundles));
		std::memset(ColorHigh, 0, sizeof(ColorHigh));
	}
};

namespace
{
void SetError(cw_bink_decoder *decoder, char const *message)
{
	if (decoder) {
		decoder->Error = message;
	}
}

int DecodeHuffman(BitReader &bits, Tree const &tree)
{
	unsigned int codebook = tree.Codebook;
	for (unsigned int length = 1; length <= 7; ++length) {
		unsigned int code = bits.Peek(length);
		for (unsigned int symbol = 0; symbol < 16; ++symbol) {
			if (bink_tree_lens[codebook][symbol] == length &&
				bink_tree_bits[codebook][symbol] == code) {
				bits.Skip(length);
				return tree.Symbols[symbol];
			}
		}
	}
	bits.Failed = true;
	return 0;
}

void Merge(BitReader &bits, unsigned char *destination, unsigned char *source, int size)
{
	unsigned char *second = source + size;
	int second_size = size;
	while (size && second_size) {
		if (!bits.Read(1)) {
			*destination++ = *source++;
			--size;
		} else {
			*destination++ = *second++;
			--second_size;
		}
	}
	while (size-- > 0) {
		*destination++ = *source++;
	}
	while (second_size-- > 0) {
		*destination++ = *second++;
	}
}

bool ReadTree(BitReader &bits, Tree &tree)
{
	if (bits.Size * 8 - bits.Bit < 4) {
		return false;
	}
	tree.Codebook = static_cast<unsigned char>(bits.Read(4));
	if (!tree.Codebook) {
		for (int index = 0; index < 16; ++index) {
			tree.Symbols[index] = static_cast<unsigned char>(index);
		}
		return !bits.Failed;
	}

	unsigned char first[16] = {0};
	unsigned char second[16] = {0};
	if (bits.Read(1)) {
		int last = static_cast<int>(bits.Read(3));
		for (int index = 0; index <= last; ++index) {
			tree.Symbols[index] = static_cast<unsigned char>(bits.Read(4));
			first[tree.Symbols[index]] = 1;
		}
		for (int value = 0; value < 16 && last < 15; ++value) {
			if (!first[value]) {
				tree.Symbols[++last] = static_cast<unsigned char>(value);
			}
		}
	} else {
		int levels = static_cast<int>(bits.Read(2));
		for (int index = 0; index < 16; ++index) {
			first[index] = static_cast<unsigned char>(index);
		}
		unsigned char *input = first;
		unsigned char *output = second;
		for (int level = 0; level <= levels; ++level) {
			int size = 1 << level;
			for (int offset = 0; offset < 16; offset += size << 1) {
				Merge(bits, output + offset, input + offset, size);
			}
			std::swap(input, output);
		}
		std::memcpy(tree.Symbols, input, 16);
	}
	return !bits.Failed;
}

bool ReadBundleHeader(BitReader &bits, cw_bink_decoder *decoder, int source)
{
	if (source == SOURCE_COLORS) {
		for (int tree = 0; tree < 16; ++tree) {
			if (!ReadTree(bits, decoder->ColorHigh[tree])) {
				return false;
			}
		}
		decoder->LastColor = 0;
	}
	if (source != SOURCE_INTRA_DC && source != SOURCE_INTER_DC) {
		if (!ReadTree(bits, decoder->Bundles[source].Huffman)) {
			return false;
		}
	}
	decoder->Bundles[source].Decoded = decoder->Bundles[source].Data;
	decoder->Bundles[source].Current = decoder->Bundles[source].Data;
	return true;
}

bool BeginBundleRead(BitReader &bits, Bundle &bundle, unsigned int &count)
{
	if (!bundle.Decoded || bundle.Decoded > bundle.Current) {
		count = 0;
		return false;
	}
	count = bits.Read(bundle.LengthBits);
	if (!count) {
		bundle.Decoded = NULL;
	}
	return !bits.Failed;
}

bool CheckBundleSpace(Bundle const &bundle, unsigned char const *end)
{
	return end >= bundle.Data && end <= bundle.DataEnd;
}

bool ReadRuns(BitReader &bits, Bundle &bundle)
{
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	unsigned char *end = bundle.Decoded + count;
	if (!CheckBundleSpace(bundle, end)) {
		return false;
	}
	if (bits.Read(1)) {
		std::memset(bundle.Decoded, static_cast<int>(bits.Read(4)), count);
		bundle.Decoded = end;
	} else {
		while (bundle.Decoded < end) {
			*bundle.Decoded++ = static_cast<unsigned char>(DecodeHuffman(bits, bundle.Huffman));
		}
	}
	return !bits.Failed;
}

bool ReadMotionValues(BitReader &bits, Bundle &bundle)
{
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	unsigned char *end = bundle.Decoded + count;
	if (!CheckBundleSpace(bundle, end)) {
		return false;
	}
	if (bits.Read(1)) {
		int value = static_cast<int>(bits.Read(4));
		if (value && bits.Read(1)) {
			value = -value;
		}
		std::memset(bundle.Decoded, value, count);
		bundle.Decoded = end;
	} else {
		while (bundle.Decoded < end) {
			int value = DecodeHuffman(bits, bundle.Huffman);
			if (value && bits.Read(1)) {
				value = -value;
			}
			*bundle.Decoded++ = static_cast<unsigned char>(value);
		}
	}
	return !bits.Failed;
}

bool ReadBlockTypes(BitReader &bits, Bundle &bundle)
{
	static unsigned char const run_lengths[4] = {4, 8, 12, 32};
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	unsigned char *end = bundle.Decoded + count;
	if (!CheckBundleSpace(bundle, end)) {
		return false;
	}
	if (bits.Read(1)) {
		std::memset(bundle.Decoded, static_cast<int>(bits.Read(4)), count);
		bundle.Decoded = end;
	} else {
		int last = 0;
		while (bundle.Decoded < end) {
			int value = DecodeHuffman(bits, bundle.Huffman);
			if (value < 12) {
				last = value;
				*bundle.Decoded++ = static_cast<unsigned char>(value);
			} else {
				unsigned int run = run_lengths[value - 12];
				if (static_cast<size_t>(end - bundle.Decoded) < run) {
					return false;
				}
				std::memset(bundle.Decoded, last, run);
				bundle.Decoded += run;
			}
		}
	}
	return !bits.Failed;
}

bool ReadPatterns(BitReader &bits, Bundle &bundle)
{
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	unsigned char *end = bundle.Decoded + count;
	if (!CheckBundleSpace(bundle, end)) {
		return false;
	}
	while (bundle.Decoded < end) {
		int value = DecodeHuffman(bits, bundle.Huffman);
		value |= DecodeHuffman(bits, bundle.Huffman) << 4;
		*bundle.Decoded++ = static_cast<unsigned char>(value);
	}
	return !bits.Failed;
}

bool ReadColors(BitReader &bits, cw_bink_decoder *decoder, Bundle &bundle)
{
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	unsigned char *end = bundle.Decoded + count;
	if (!CheckBundleSpace(bundle, end)) {
		return false;
	}
	bool constant = bits.Read(1) != 0;
	do {
		decoder->LastColor =
			DecodeHuffman(bits, decoder->ColorHigh[decoder->LastColor]);
		int value = DecodeHuffman(bits, bundle.Huffman);
		value |= decoder->LastColor << 4;
		if (constant) {
			std::memset(bundle.Decoded, value, count);
			bundle.Decoded = end;
		} else {
			*bundle.Decoded++ = static_cast<unsigned char>(value);
		}
	} while (bundle.Decoded < end);
	return !bits.Failed;
}

bool ReadDCs(BitReader &bits, Bundle &bundle, bool signed_values)
{
	unsigned int count = 0;
	if (!BeginBundleRead(bits, bundle, count) || !count) {
		return !bits.Failed;
	}
	int16_t *destination = reinterpret_cast<int16_t *>(bundle.Decoded);
	int16_t *end = reinterpret_cast<int16_t *>(bundle.DataEnd);
	int value = static_cast<int>(bits.Read(11 - (signed_values ? 1 : 0)));
	if (value && signed_values && bits.Read(1)) {
		value = -value;
	}
	if (destination >= end) {
		return false;
	}
	*destination++ = static_cast<int16_t>(value);
	--count;

	for (unsigned int offset = 0; offset < count; offset += 8) {
		unsigned int group = (std::min)(8U, count - offset);
		unsigned int size = bits.Read(4);
		if (destination + group > end) {
			return false;
		}
		for (unsigned int index = 0; index < group; ++index) {
			if (size) {
				int delta = static_cast<int>(bits.Read(size));
				if (delta && bits.Read(1)) {
					delta = -delta;
				}
				value += delta;
			}
			if (value < -32768 || value > 32767) {
				return false;
			}
			*destination++ = static_cast<int16_t>(value);
		}
	}
	bundle.Decoded = reinterpret_cast<unsigned char *>(destination);
	return !bits.Failed;
}

int GetValue(cw_bink_decoder *decoder, int source)
{
	Bundle &bundle = decoder->Bundles[source];
	if (!bundle.Current || bundle.Current >= bundle.DataEnd) {
		return 0;
	}
	if (source < SOURCE_X_OFF || source == SOURCE_RUN) {
		return *bundle.Current++;
	}
	if (source == SOURCE_X_OFF || source == SOURCE_Y_OFF) {
		return static_cast<int8_t>(*bundle.Current++);
	}
	if (bundle.Current + 2 > bundle.DataEnd) {
		return 0;
	}
	int value = static_cast<int16_t>(ReadLE16(bundle.Current));
	bundle.Current += 2;
	return value;
}

int ReadDCTCoefficients(
	BitReader &bits,
	int32_t block[64],
	int &coefficient_count,
	int coefficient_indices[64])
{
	int coefficient_list[128];
	int mode_list[128];
	int list_start = 64;
	int list_end = 64;
	coefficient_count = 0;

	coefficient_list[list_end] = 4;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 24;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 44;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 1;
	mode_list[list_end++] = 3;
	coefficient_list[list_end] = 2;
	mode_list[list_end++] = 3;
	coefficient_list[list_end] = 3;
	mode_list[list_end++] = 3;

	for (int magnitude_bits = static_cast<int>(bits.Read(4)) - 1;
		 magnitude_bits >= 0;
		 --magnitude_bits) {
		int position = list_start;
		while (position < list_end) {
			if (!(mode_list[position] | coefficient_list[position]) || !bits.Read(1)) {
				++position;
				continue;
			}

			int coefficient = coefficient_list[position];
			int mode = mode_list[position];
			if (mode == 0 || mode == 2) {
				if (mode == 0) {
					coefficient_list[position] = coefficient + 4;
					mode_list[position] = 1;
				} else {
					coefficient_list[position] = 0;
					mode_list[position++] = 0;
				}

				for (int group = 0; group < 4; ++group, ++coefficient) {
					if (bits.Read(1)) {
						coefficient_list[--list_start] = coefficient;
						mode_list[list_start] = 3;
					} else {
						int value;
						if (!magnitude_bits) {
							value = bits.Read(1) ? -1 : 1;
						} else {
							value = static_cast<int>(bits.Read(magnitude_bits)) |
								(1 << magnitude_bits);
							if (bits.Read(1)) {
								value = -value;
							}
						}
						block[bink_scan[coefficient]] = value;
						coefficient_indices[coefficient_count++] = coefficient;
					}
				}
			} else if (mode == 1) {
				mode_list[position] = 2;
				for (int group = 0; group < 3; ++group) {
					coefficient += 4;
					coefficient_list[list_end] = coefficient;
					mode_list[list_end++] = 2;
				}
			} else {
				int value;
				if (!magnitude_bits) {
					value = bits.Read(1) ? -1 : 1;
				} else {
					value = static_cast<int>(bits.Read(magnitude_bits)) |
						(1 << magnitude_bits);
					if (bits.Read(1)) {
						value = -value;
					}
				}
				block[bink_scan[coefficient]] = value;
				coefficient_indices[coefficient_count++] = coefficient;
				coefficient_list[position] = 0;
				mode_list[position++] = 0;
			}
		}
	}

	return bits.Failed ? -1 : static_cast<int>(bits.Read(4));
}

void Unquantize(
	int32_t block[64],
	int32_t const quantizer[64],
	int coefficient_count,
	int const coefficient_indices[64])
{
	block[0] = static_cast<int32_t>(
		(static_cast<int64_t>(block[0]) * quantizer[0]) >> 11);
	for (int index = 0; index < coefficient_count; ++index) {
		int scan_index = coefficient_indices[index];
		int destination = bink_scan[scan_index];
		block[destination] = static_cast<int32_t>(
			(static_cast<int64_t>(block[destination]) * quantizer[scan_index]) >> 11);
	}
}

bool ReadResidue(BitReader &bits, int16_t block[64], int masks_remaining)
{
	int coefficient_list[128];
	int mode_list[128];
	int nonzero[64];
	int list_start = 64;
	int list_end = 64;
	int nonzero_count = 0;

	coefficient_list[list_end] = 4;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 24;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 44;
	mode_list[list_end++] = 0;
	coefficient_list[list_end] = 0;
	mode_list[list_end++] = 2;

	for (int mask = 1 << bits.Read(3); mask; mask >>= 1) {
		for (int index = 0; index < nonzero_count; ++index) {
			if (bits.Read(1)) {
				int position = nonzero[index];
				block[position] += block[position] < 0 ? -mask : mask;
				if (--masks_remaining < 0) {
					return !bits.Failed;
				}
			}
		}

		int position = list_start;
		while (position < list_end) {
			if (!(coefficient_list[position] | mode_list[position]) || !bits.Read(1)) {
				++position;
				continue;
			}

			int coefficient = coefficient_list[position];
			int mode = mode_list[position];
			if (mode == 0 || mode == 2) {
				if (mode == 0) {
					coefficient_list[position] = coefficient + 4;
					mode_list[position] = 1;
				} else {
					coefficient_list[position] = 0;
					mode_list[position++] = 0;
				}
				for (int group = 0; group < 4; ++group, ++coefficient) {
					if (bits.Read(1)) {
						coefficient_list[--list_start] = coefficient;
						mode_list[list_start] = 3;
					} else {
						int destination = bink_scan[coefficient];
						nonzero[nonzero_count++] = destination;
						block[destination] = bits.Read(1) ? -mask : mask;
						if (--masks_remaining < 0) {
							return !bits.Failed;
						}
					}
				}
			} else if (mode == 1) {
				mode_list[position] = 2;
				for (int group = 0; group < 3; ++group) {
					coefficient += 4;
					coefficient_list[list_end] = coefficient;
					mode_list[list_end++] = 2;
				}
			} else {
				int destination = bink_scan[coefficient];
				nonzero[nonzero_count++] = destination;
				block[destination] = bits.Read(1) ? -mask : mask;
				coefficient_list[position] = 0;
				mode_list[position++] = 0;
				if (--masks_remaining < 0) {
					return !bits.Failed;
				}
			}
		}
	}
	return !bits.Failed;
}

int MultiplyDCT(int left, int right)
{
	uint32_t product =
		static_cast<uint32_t>(left) * static_cast<uint32_t>(right);
	return static_cast<int32_t>(product) >> 11;
}

void TransformDCT(int const source[8], int destination[8], bool row)
{
	static int const a1_constant = 2896;
	static int const a2_constant = 2217;
	static int const a3_constant = 3784;
	static int const a4_constant = -5352;

	int a0 = source[0] + source[4];
	int a1 = source[0] - source[4];
	int a2 = source[2] + source[6];
	int a3 = MultiplyDCT(a1_constant, source[2] - source[6]);
	int a4 = source[5] + source[3];
	int a5 = source[5] - source[3];
	int a6 = source[1] + source[7];
	int a7 = source[1] - source[7];
	int b0 = a4 + a6;
	int b1 = MultiplyDCT(a3_constant, a5 + a7);
	int b2 = MultiplyDCT(a4_constant, a5) - b0 + b1;
	int b3 = MultiplyDCT(a1_constant, a6 - a4) - b2;
	int b4 = MultiplyDCT(a2_constant, a7) + b3 - b1;

	int values[8] = {
		a0 + a2 + b0,
		a1 + a3 - a2 + b2,
		a1 - a3 + a2 + b3,
		a0 - a2 - b4,
		a0 - a2 + b4,
		a1 - a3 + a2 - b3,
		a1 + a3 - a2 - b2,
		a0 + a2 - b0,
	};
	for (int index = 0; index < 8; ++index) {
		destination[index] = row ? ((values[index] + 0x7F) >> 8) : values[index];
	}
}

void InverseDCT(int32_t block[64])
{
	int temporary[64];
	for (int column = 0; column < 8; ++column) {
		int source[8];
		int destination[8];
		for (int row = 0; row < 8; ++row) {
			source[row] = block[row * 8 + column];
		}
		if ((source[1] | source[2] | source[3] | source[4] |
			 source[5] | source[6] | source[7]) == 0) {
			for (int row = 0; row < 8; ++row) {
				temporary[row * 8 + column] = source[0];
			}
		} else {
			TransformDCT(source, destination, false);
			for (int row = 0; row < 8; ++row) {
				temporary[row * 8 + column] = destination[row];
			}
		}
	}

	for (int row = 0; row < 8; ++row) {
		int destination[8];
		TransformDCT(temporary + row * 8, destination, true);
		for (int column = 0; column < 8; ++column) {
			block[row * 8 + column] = destination[column];
		}
	}
}

void IDCTPut(unsigned char *destination, unsigned int stride, int32_t block[64])
{
	InverseDCT(block);
	for (int row = 0; row < 8; ++row) {
		for (int column = 0; column < 8; ++column) {
			destination[row * stride + column] =
				static_cast<unsigned char>(block[row * 8 + column]);
		}
	}
}

void IDCTAdd(unsigned char *destination, unsigned int stride, int32_t block[64])
{
	InverseDCT(block);
	for (int row = 0; row < 8; ++row) {
		for (int column = 0; column < 8; ++column) {
			destination[row * stride + column] = static_cast<unsigned char>(
				destination[row * stride + column] + block[row * 8 + column]);
		}
	}
}

void CopyBlock(
	unsigned char *destination,
	unsigned char const *source,
	unsigned int destination_stride,
	unsigned int source_stride)
{
	for (int row = 0; row < 8; ++row) {
		std::memcpy(
			destination + row * destination_stride,
			source + row * source_stride,
			8);
	}
}

void ScaleBlock(
	unsigned char const source[64],
	unsigned char *destination,
	unsigned int stride)
{
	for (int row = 0; row < 8; ++row) {
		for (int column = 0; column < 8; ++column) {
			unsigned char value = source[row * 8 + column];
			destination[(row * 2) * stride + column * 2] = value;
			destination[(row * 2) * stride + column * 2 + 1] = value;
			destination[(row * 2 + 1) * stride + column * 2] = value;
			destination[(row * 2 + 1) * stride + column * 2 + 1] = value;
		}
	}
}

bool MotionBlock(
	cw_bink_decoder *decoder,
	Plane &destination_plane,
	Plane const &previous_plane,
	unsigned int block_x,
	unsigned int block_y,
	unsigned char *destination)
{
	int source_x = static_cast<int>(block_x * 8) + GetValue(decoder, SOURCE_X_OFF);
	int source_y = static_cast<int>(block_y * 8) + GetValue(decoder, SOURCE_Y_OFF);
	if (source_x < 0 || source_y < 0 ||
		source_x + 8 > static_cast<int>(previous_plane.Width) ||
		source_y + 8 > static_cast<int>(previous_plane.Height)) {
		return false;
	}
	CopyBlock(
		destination,
		&previous_plane.Pixels[source_y * previous_plane.Stride + source_x],
		destination_plane.Stride,
		previous_plane.Stride);
	return true;
}

bool DecodePlane(
	cw_bink_decoder *decoder,
	BitReader &bits,
	int plane_index,
	bool chroma)
{
	Frame &current_frame = decoder->Video[decoder->CurrentVideo];
	Frame const &previous_frame =
		decoder->Video[decoder->CurrentVideo ^ (decoder->HasPrevious ? 1 : 0)];
	Plane &plane = current_frame.Planes[plane_index];
	Plane const &previous = previous_frame.Planes[plane_index];

	unsigned int block_width = chroma
		? (decoder->Width + 15) >> 4
		: (decoder->Width + 7) >> 3;
	unsigned int block_height = chroma
		? (decoder->Height + 15) >> 4
		: (decoder->Height + 7) >> 3;
	unsigned int logical_width = chroma ? decoder->Width >> 1 : decoder->Width;
	unsigned int length_width = (std::max)(logical_width, 8U);
	unsigned int aligned_width = (length_width + 7) & ~7U;

	decoder->Bundles[SOURCE_BLOCK_TYPES].LengthBits =
		IntegerLog2((aligned_width >> 3) + 511) + 1;
	decoder->Bundles[SOURCE_SUB_BLOCK_TYPES].LengthBits =
		IntegerLog2((aligned_width >> 4) + 511) + 1;
	decoder->Bundles[SOURCE_COLORS].LengthBits =
		IntegerLog2(block_width * 64 + 511) + 1;
	decoder->Bundles[SOURCE_INTRA_DC].LengthBits =
	decoder->Bundles[SOURCE_INTER_DC].LengthBits =
	decoder->Bundles[SOURCE_X_OFF].LengthBits =
	decoder->Bundles[SOURCE_Y_OFF].LengthBits =
		IntegerLog2((aligned_width >> 3) + 511) + 1;
	decoder->Bundles[SOURCE_PATTERN].LengthBits =
		IntegerLog2((block_width << 3) + 511) + 1;
	decoder->Bundles[SOURCE_RUN].LengthBits =
		IntegerLog2(block_width * 48 + 511) + 1;

	for (int source = 0; source < SOURCE_COUNT; ++source) {
		if (!ReadBundleHeader(bits, decoder, source)) {
			return false;
		}
	}

	int coordinates[64];
	for (int index = 0; index < 64; ++index) {
		coordinates[index] = (index & 7) + (index >> 3) * plane.Stride;
	}

	for (unsigned int block_y = 0; block_y < block_height; ++block_y) {
		if (!ReadBlockTypes(bits, decoder->Bundles[SOURCE_BLOCK_TYPES]) ||
			!ReadBlockTypes(bits, decoder->Bundles[SOURCE_SUB_BLOCK_TYPES]) ||
			!ReadColors(bits, decoder, decoder->Bundles[SOURCE_COLORS]) ||
			!ReadPatterns(bits, decoder->Bundles[SOURCE_PATTERN]) ||
			!ReadMotionValues(bits, decoder->Bundles[SOURCE_X_OFF]) ||
			!ReadMotionValues(bits, decoder->Bundles[SOURCE_Y_OFF]) ||
			!ReadDCs(bits, decoder->Bundles[SOURCE_INTRA_DC], false) ||
			!ReadDCs(bits, decoder->Bundles[SOURCE_INTER_DC], true) ||
			!ReadRuns(bits, decoder->Bundles[SOURCE_RUN])) {
			return false;
		}

		for (unsigned int block_x = 0; block_x < block_width; ++block_x) {
			unsigned char *destination =
				&plane.Pixels[block_y * 8 * plane.Stride + block_x * 8];
			unsigned char const *previous_block =
				&previous.Pixels[block_y * 8 * previous.Stride + block_x * 8];
			int type = GetValue(decoder, SOURCE_BLOCK_TYPES);

			if (((block_y & 1) || (block_x & 1)) && type == BLOCK_SCALED) {
				++block_x;
				continue;
			}

			if (type == BLOCK_SKIP) {
				CopyBlock(destination, previous_block, plane.Stride, previous.Stride);
			} else if (type == BLOCK_SCALED) {
				unsigned char unscaled[64] = {0};
				int subtype = GetValue(decoder, SOURCE_SUB_BLOCK_TYPES);
				if (subtype == BLOCK_RUN) {
					unsigned char const *scan = bink_patterns[bits.Read(4)];
					int written = 0;
					do {
						int run = GetValue(decoder, SOURCE_RUN) + 1;
						written += run;
						if (written > 64) {
							return false;
						}
						if (bits.Read(1)) {
							unsigned char value =
								static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
							for (int index = 0; index < run; ++index) {
								unscaled[*scan++] = value;
							}
						} else {
							for (int index = 0; index < run; ++index) {
								unscaled[*scan++] = static_cast<unsigned char>(
									GetValue(decoder, SOURCE_COLORS));
							}
						}
					} while (written < 63);
					if (written == 63) {
						unscaled[*scan] =
							static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
					}
				} else if (subtype == BLOCK_INTRA) {
					int32_t coefficients[64] = {0};
					int indices[64];
					int count = 0;
					coefficients[0] = GetValue(decoder, SOURCE_INTRA_DC);
					int quantizer = ReadDCTCoefficients(bits, coefficients, count, indices);
					if (quantizer < 0 || quantizer > 15) {
						return false;
					}
					Unquantize(coefficients, bink_intra_quant[quantizer], count, indices);
					IDCTPut(unscaled, 8, coefficients);
				} else if (subtype == BLOCK_FILL) {
					unsigned char value =
						static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
					for (int row = 0; row < 16; ++row) {
						std::memset(destination + row * plane.Stride, value, 16);
					}
				} else if (subtype == BLOCK_PATTERN) {
					unsigned char colors[2] = {
						static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS)),
						static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS)),
					};
					for (int row = 0; row < 8; ++row) {
						int pattern = GetValue(decoder, SOURCE_PATTERN);
						for (int column = 0; column < 8; ++column, pattern >>= 1) {
							unscaled[row * 8 + column] = colors[pattern & 1];
						}
					}
				} else if (subtype == BLOCK_RAW) {
					for (int index = 0; index < 64; ++index) {
						unscaled[index] = static_cast<unsigned char>(
							GetValue(decoder, SOURCE_COLORS));
					}
				} else {
					return false;
				}
				if (subtype != BLOCK_FILL) {
					ScaleBlock(unscaled, destination, plane.Stride);
				}
				++block_x;
			} else if (type == BLOCK_MOTION) {
				if (!MotionBlock(
						decoder, plane, previous, block_x, block_y, destination)) {
					return false;
				}
			} else if (type == BLOCK_RUN) {
				unsigned char const *scan = bink_patterns[bits.Read(4)];
				int written = 0;
				do {
					int run = GetValue(decoder, SOURCE_RUN) + 1;
					written += run;
					if (written > 64) {
						return false;
					}
					if (bits.Read(1)) {
						unsigned char value =
							static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
						for (int index = 0; index < run; ++index) {
							destination[coordinates[*scan++]] = value;
						}
					} else {
						for (int index = 0; index < run; ++index) {
							destination[coordinates[*scan++]] =
								static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
						}
					}
				} while (written < 63);
				if (written == 63) {
					destination[coordinates[*scan]] =
						static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
				}
			} else if (type == BLOCK_RESIDUE) {
				if (!MotionBlock(
						decoder, plane, previous, block_x, block_y, destination)) {
					return false;
				}
				int16_t residue[64] = {0};
				if (!ReadResidue(bits, residue, static_cast<int>(bits.Read(7)))) {
					return false;
				}
				for (int row = 0; row < 8; ++row) {
					for (int column = 0; column < 8; ++column) {
						destination[row * plane.Stride + column] =
							static_cast<unsigned char>(
								destination[row * plane.Stride + column] +
								residue[row * 8 + column]);
					}
				}
			} else if (type == BLOCK_INTRA) {
				int32_t coefficients[64] = {0};
				int indices[64];
				int count = 0;
				coefficients[0] = GetValue(decoder, SOURCE_INTRA_DC);
				int quantizer = ReadDCTCoefficients(bits, coefficients, count, indices);
				if (quantizer < 0 || quantizer > 15) {
					return false;
				}
				Unquantize(coefficients, bink_intra_quant[quantizer], count, indices);
				IDCTPut(destination, plane.Stride, coefficients);
			} else if (type == BLOCK_FILL) {
				unsigned char value =
					static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS));
				for (int row = 0; row < 8; ++row) {
					std::memset(destination + row * plane.Stride, value, 8);
				}
			} else if (type == BLOCK_INTER) {
				if (!MotionBlock(
						decoder, plane, previous, block_x, block_y, destination)) {
					return false;
				}
				int32_t coefficients[64] = {0};
				int indices[64];
				int count = 0;
				coefficients[0] = GetValue(decoder, SOURCE_INTER_DC);
				int quantizer = ReadDCTCoefficients(bits, coefficients, count, indices);
				if (quantizer < 0 || quantizer > 15) {
					return false;
				}
				Unquantize(coefficients, bink_inter_quant[quantizer], count, indices);
				IDCTAdd(destination, plane.Stride, coefficients);
			} else if (type == BLOCK_PATTERN) {
				unsigned char colors[2] = {
					static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS)),
					static_cast<unsigned char>(GetValue(decoder, SOURCE_COLORS)),
				};
				for (int row = 0; row < 8; ++row) {
					int pattern = GetValue(decoder, SOURCE_PATTERN);
					for (int column = 0; column < 8; ++column, pattern >>= 1) {
						destination[row * plane.Stride + column] = colors[pattern & 1];
					}
				}
			} else if (type == BLOCK_RAW) {
				Bundle &colors = decoder->Bundles[SOURCE_COLORS];
				if (!colors.Current || colors.Current + 64 > colors.DataEnd) {
					return false;
				}
				for (int row = 0; row < 8; ++row) {
					std::memcpy(
						destination + row * plane.Stride,
						colors.Current + row * 8,
						8);
				}
				colors.Current += 64;
			} else {
				return false;
			}
		}
	}

	bits.Align32();
	return !bits.Failed;
}

void ConvertToRGB(cw_bink_decoder *decoder)
{
	Frame const &frame = decoder->Video[decoder->CurrentVideo];
	Plane const &luma = frame.Planes[0];
	Plane const &chroma_u = frame.Planes[1];
	Plane const &chroma_v = frame.Planes[2];
	decoder->RGB.resize(static_cast<size_t>(decoder->Width) * decoder->Height * 3);

	for (uint32_t y = 0; y < decoder->Height; ++y) {
		for (uint32_t x = 0; x < decoder->Width; ++x) {
			int value_y = luma.Pixels[y * luma.Stride + x];
			int value_u = chroma_u.Pixels[(y >> 1) * chroma_u.Stride + (x >> 1)];
			int value_v = chroma_v.Pixels[(y >> 1) * chroma_v.Stride + (x >> 1)];
			int c = (std::max)(0, value_y - 16);
			int d = value_u - 128;
			int e = value_v - 128;
			size_t destination = (static_cast<size_t>(y) * decoder->Width + x) * 3;
			decoder->RGB[destination] = ClampByte((298 * c + 409 * e + 128) >> 8);
			decoder->RGB[destination + 1] =
				ClampByte((298 * c - 100 * d - 208 * e + 128) >> 8);
			decoder->RGB[destination + 2] =
				ClampByte((298 * c + 516 * d + 128) >> 8);
		}
	}
}

void InverseFFT(std::vector<std::complex<float> > &values)
{
	size_t count = values.size();
	for (size_t index = 1, reversed = 0; index < count; ++index) {
		size_t bit = count >> 1;
		for (; reversed & bit; bit >>= 1) {
			reversed ^= bit;
		}
		reversed ^= bit;
		if (index < reversed) {
			std::swap(values[index], values[reversed]);
		}
	}

	static double const PI = 3.1415926535897932384626433832795;
	for (size_t length = 2; length <= count; length <<= 1) {
		float angle = static_cast<float>(2.0 * PI / length);
		std::complex<float> step(std::cos(angle), std::sin(angle));
		for (size_t base = 0; base < count; base += length) {
			std::complex<float> factor(1.0f, 0.0f);
			size_t half = length >> 1;
			for (size_t offset = 0; offset < half; ++offset) {
				std::complex<float> even = values[base + offset];
				std::complex<float> odd =
					values[base + offset + half] * factor;
				values[base + offset] = even + odd;
				values[base + offset + half] = even - odd;
				factor *= step;
			}
		}
	}

	float scale = 1.0f / static_cast<float>(count);
	for (size_t index = 0; index < count; ++index) {
		values[index] *= scale;
	}
}

void InverseDCT(AudioState &audio)
{
	size_t length = audio.FrameLength;
	size_t transform_length = length * 2;
	std::fill(
		audio.Transform.begin(),
		audio.Transform.end(),
		std::complex<float>(0.0f, 0.0f));
	audio.Transform[0] = std::complex<float>(audio.Coefficients[0], 0.0f);

	static double const PI = 3.1415926535897932384626433832795;
	for (size_t index = 1; index < length; ++index) {
		float angle = static_cast<float>(PI * index / transform_length);
		std::complex<float> value =
			0.5f * audio.Coefficients[index] *
			std::complex<float>(std::cos(angle), std::sin(angle));
		audio.Transform[index] = value;
		audio.Transform[transform_length - index] = std::conj(value);
	}

	InverseFFT(audio.Transform);
	for (size_t sample = 0; sample < length; ++sample) {
		/*
		 * The normalized 2N inverse FFT contains the unscaled DCT-III
		 * divided by 2N. Bink uses 2/N times that DCT, hence factor 4.
		 */
		audio.BlockOutput[sample] = audio.Transform[sample].real() * 4.0f;
	}
}

float ReadBinkFloat(BitReader &bits)
{
	int power = static_cast<int>(bits.Read(5));
	float value = std::ldexp(
		static_cast<float>(bits.Read(23)),
		power - 23);
	if (bits.Read(1)) {
		value = -value;
	}
	return value;
}

bool DecodeAudioBlock(BitReader &bits, AudioState &audio)
{
	static unsigned char const RUN_LENGTH[16] = {
		2, 3, 4, 5, 6, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 32, 64
	};

	bits.Skip(2);
	for (unsigned int channel = 0; channel < audio.Channels; ++channel) {
		float quant[25];
		audio.Coefficients[0] =
			ReadBinkFloat(bits) * audio.QuantTable[0];
		audio.Coefficients[1] =
			ReadBinkFloat(bits) * audio.QuantTable[0];

		/*
		 * The first two values use the transform root but no band
		 * quantizer. QuantTable[0] is exactly that root.
		 */
		for (unsigned int band = 0; band < audio.BandCount; ++band) {
			unsigned int value = (std::min)(bits.Read(8), 95U);
			quant[band] = audio.QuantTable[value];
		}

		unsigned int band = 0;
		float q = quant[0];
		unsigned int coefficient = 2;
		while (coefficient < audio.FrameLength) {
			unsigned int end;
			if (bits.Read(1)) {
				end = coefficient + RUN_LENGTH[bits.Read(4)] * 8;
			} else {
				end = coefficient + 8;
			}
			end = (std::min)(end, audio.FrameLength);
			unsigned int width = bits.Read(4);
			if (!width) {
				std::fill(
					audio.Coefficients.begin() + coefficient,
					audio.Coefficients.begin() + end,
					0.0f);
				coefficient = end;
				while (band < audio.BandCount &&
					audio.Bands[band] < coefficient) {
					q = quant[band++];
				}
			} else {
				while (coefficient < end) {
					if (band < audio.BandCount &&
						audio.Bands[band] == coefficient) {
						q = quant[band++];
					}
					unsigned int magnitude = bits.Read(width);
					if (magnitude) {
						audio.Coefficients[coefficient] =
							(bits.Read(1) ? -q : q) * magnitude;
					} else {
						audio.Coefficients[coefficient] = 0.0f;
					}
					++coefficient;
				}
			}
		}
		if (bits.Failed) {
			return false;
		}

		InverseDCT(audio);
		unsigned int overlap = audio.OverlapLength;
		if (!audio.First) {
			for (unsigned int sample = 0; sample < overlap; ++sample) {
				audio.BlockOutput[sample] =
					(audio.Previous[sample] * (overlap - sample) +
						audio.BlockOutput[sample] * sample) /
					overlap;
			}
		}
		std::copy(
			audio.BlockOutput.end() - overlap,
			audio.BlockOutput.end(),
			audio.Previous.begin());
		audio.PCM.insert(
			audio.PCM.end(),
			audio.BlockOutput.begin(),
			audio.BlockOutput.end() - overlap);
		audio.First = false;
	}
	return true;
}

bool DecodeAudioPacket(
	cw_bink_decoder *decoder,
	unsigned char const *packet,
	size_t packet_size)
{
	AudioState &audio = decoder->Audio;
	audio.PCM.clear();
	if (!packet_size) {
		return true;
	}
	if (packet_size < 4) {
		SetError(decoder, "truncated BIK audio payload");
		return false;
	}

	uint32_t reported_bytes = ReadLE32(packet);
	if (reported_bytes > audio.MaxDecodedBytes ||
		reported_bytes % (2 * audio.Channels)) {
		SetError(decoder, "invalid BIK decoded audio size");
		return false;
	}
	size_t target_frames = reported_bytes / (2 * audio.Channels);
	BitReader bits(packet, packet_size);
	bits.Skip(32);
	while (audio.PCM.size() / audio.Channels < target_frames) {
		size_t old_bit = bits.Bit;
		if (!DecodeAudioBlock(bits, audio)) {
			SetError(decoder, "invalid Bink DCT audio data");
			return false;
		}
		bits.Align32();
		if (bits.Failed || bits.Bit <= old_bit) {
			SetError(decoder, "truncated Bink DCT audio block");
			return false;
		}
	}
	audio.PCM.resize(target_frames * audio.Channels);
	return true;
}

bool DecodeFrame(cw_bink_decoder *decoder, uint32_t frame_index)
{
	if (frame_index >= decoder->FrameCount) {
		SetError(decoder, "frame index is out of range");
		return false;
	}
	uint32_t offset = decoder->FrameOffsets[frame_index];
	uint32_t end = frame_index + 1 < decoder->FrameCount
		? decoder->FrameOffsets[frame_index + 1]
		: decoder->FileSize;
	if (end <= offset || end - offset > std::numeric_limits<uint32_t>::max() / 2) {
		SetError(decoder, "invalid frame index entry");
		return false;
	}

	decoder->Packet.resize(end - offset);
	if (!Seek(decoder->File, offset) ||
		!ReadExact(decoder->File, decoder->Packet.data(), decoder->Packet.size())) {
		SetError(decoder, "failed to read a BIK frame");
		return false;
	}

	if (frame_index == 0 && decoder->AudioTracks) {
		decoder->Audio.First = true;
		std::fill(
			decoder->Audio.Previous.begin(),
			decoder->Audio.Previous.end(),
			0.0f);
	}
	decoder->Audio.PCM.clear();

	size_t video_offset = 0;
	for (uint32_t track = 0; track < decoder->AudioTracks; ++track) {
		if (video_offset + 4 > decoder->Packet.size()) {
			SetError(decoder, "truncated BIK audio packet header");
			return false;
		}
		uint32_t audio_size = ReadLE32(decoder->Packet.data() + video_offset);
		video_offset += 4;
		if (audio_size > decoder->Packet.size() - video_offset) {
			SetError(decoder, "truncated BIK audio packet");
			return false;
		}
		if (track == 0 &&
			!DecodeAudioPacket(
				decoder,
				decoder->Packet.data() + video_offset,
				audio_size)) {
			return false;
		}
		video_offset += audio_size;
	}
	if (video_offset >= decoder->Packet.size()) {
		SetError(decoder, "BIK frame has no video packet");
		return false;
	}

	if (frame_index == 0) {
		decoder->CurrentVideo = 0;
		decoder->HasPrevious = false;
		for (int frame = 0; frame < 2; ++frame) {
			for (int plane = 0; plane < 3; ++plane) {
				std::fill(
					decoder->Video[frame].Planes[plane].Pixels.begin(),
					decoder->Video[frame].Planes[plane].Pixels.end(),
					0);
			}
		}
	} else {
		decoder->CurrentVideo ^= 1;
		decoder->HasPrevious = true;
	}

	BitReader bits(
		decoder->Packet.data() + video_offset,
		decoder->Packet.size() - video_offset);
	bits.Skip(32);
	if (!DecodePlane(decoder, bits, 0, false) ||
		!DecodePlane(decoder, bits, 2, true) ||
		!DecodePlane(decoder, bits, 1, true)) {
		SetError(decoder, "invalid or unsupported BIKi video data");
		return false;
	}

	ConvertToRGB(decoder);
	decoder->CurrentFrame = frame_index;
	decoder->Error.clear();
	return true;
}
}

extern "C" cw_bink_decoder *cw_bink_open_file(char const *path)
{
	if (!path) {
		return NULL;
	}

	cw_bink_decoder *decoder = new cw_bink_decoder;
	decoder->File.open(path, std::ios::binary);
	if (!decoder->File) {
		SetError(decoder, "failed to open BIK file");
		delete decoder;
		return NULL;
	}

	decoder->File.seekg(0, std::ios::end);
	std::streamoff actual_size = decoder->File.tellg();
	decoder->File.seekg(0, std::ios::beg);
	if (actual_size < 44 ||
		actual_size > static_cast<std::streamoff>(std::numeric_limits<uint32_t>::max())) {
		SetError(decoder, "invalid BIK file size");
		delete decoder;
		return NULL;
	}

	unsigned char header[44];
	if (!ReadExact(decoder->File, header, sizeof(header)) ||
		std::memcmp(header, "BIKi", 4) != 0) {
		SetError(decoder, "only BIKi Bink 1 files are supported");
		delete decoder;
		return NULL;
	}

	decoder->FileSize = ReadLE32(header + 4) + 8;
	decoder->FrameCount = ReadLE32(header + 8);
	uint32_t largest_frame = ReadLE32(header + 12);
	decoder->Width = ReadLE32(header + 20);
	decoder->Height = ReadLE32(header + 24);
	decoder->FpsNumerator = ReadLE32(header + 28);
	decoder->FpsDenominator = ReadLE32(header + 32);
	uint32_t video_flags = ReadLE32(header + 36);
	decoder->AudioTracks = ReadLE32(header + 40);

	if (decoder->FileSize != static_cast<uint32_t>(actual_size) ||
		!decoder->FrameCount || decoder->FrameCount > 1000000 ||
		largest_frame > decoder->FileSize ||
		!decoder->Width || !decoder->Height ||
		decoder->Width > 7680 || decoder->Height > 4800 ||
		!decoder->FpsNumerator || !decoder->FpsDenominator ||
		decoder->AudioTracks > 256) {
		SetError(decoder, "invalid BIK header");
		delete decoder;
		return NULL;
	}
	if (video_flags & 0x00100000U) {
		SetError(decoder, "BIK alpha planes are not supported");
		delete decoder;
		return NULL;
	}

	size_t audio_table_size = static_cast<size_t>(decoder->AudioTracks) * 12;
	size_t index_size = static_cast<size_t>(decoder->FrameCount) * 4;
	if (44 + audio_table_size + index_size > decoder->FileSize) {
		SetError(decoder, "truncated BIK tables");
		delete decoder;
		return NULL;
	}

	if (audio_table_size) {
		std::vector<unsigned char> audio_tables(audio_table_size);
		if (!ReadExact(decoder->File, audio_tables.data(), audio_tables.size())) {
			SetError(decoder, "truncated BIK audio table");
			delete decoder;
			return NULL;
		}
		if (decoder->AudioTracks != 1) {
			SetError(decoder, "only one Bink audio track is supported");
			delete decoder;
			return NULL;
		}

		AudioState &audio = decoder->Audio;
		audio.MaxDecodedBytes = ReadLE32(audio_tables.data());
		audio.SampleRate = ReadLE16(audio_tables.data() + 4);
		audio.Flags = ReadLE16(audio_tables.data() + 6);
		audio.Channels = (audio.Flags & 0x2000U) ? 2 : 1;
		if (!audio.MaxDecodedBytes ||
			!audio.SampleRate ||
			audio.SampleRate > 192000 ||
			!(audio.Flags & 0x1000U) ||
			audio.Channels != 1) {
			SetError(
				decoder,
				"only mono Bink DCT audio is supported");
			delete decoder;
			return NULL;
		}

		if (audio.SampleRate < 22050) {
			audio.FrameLength = 512;
		} else if (audio.SampleRate < 44100) {
			audio.FrameLength = 1024;
		} else {
			audio.FrameLength = 2048;
		}
		audio.OverlapLength = audio.FrameLength / 16;
		float root =
			std::sqrt(static_cast<float>(audio.FrameLength)) / 32768.0f;
		for (unsigned int index = 0; index < 96; ++index) {
			audio.QuantTable[index] =
				std::exp(index * 0.15289164787221953823f) * root;
		}

		static unsigned int const CRITICAL_FREQUENCIES[25] = {
			100, 200, 300, 400, 510, 630, 770, 920, 1080,
			1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700,
			4400, 5300, 6400, 7700, 9500, 12000, 15500, 24500
		};
		unsigned int half_rate = (audio.SampleRate + 1) / 2;
		for (audio.BandCount = 1; audio.BandCount < 25;
			++audio.BandCount) {
			if (half_rate <=
				CRITICAL_FREQUENCIES[audio.BandCount - 1]) {
				break;
			}
		}
		audio.Bands[0] = 2;
		for (unsigned int index = 1; index < audio.BandCount; ++index) {
			audio.Bands[index] =
				(CRITICAL_FREQUENCIES[index - 1] *
					audio.FrameLength / half_rate) & ~1U;
		}
		audio.Bands[audio.BandCount] = audio.FrameLength;
		audio.Previous.resize(audio.OverlapLength);
		audio.Coefficients.resize(audio.FrameLength);
		audio.BlockOutput.resize(audio.FrameLength);
		audio.Transform.resize(audio.FrameLength * 2);
	}

	std::vector<unsigned char> raw_index(index_size);
	if (!ReadExact(decoder->File, raw_index.data(), raw_index.size())) {
		SetError(decoder, "truncated BIK frame index");
		delete decoder;
		return NULL;
	}
	decoder->FrameOffsets.resize(decoder->FrameCount);
	decoder->FrameKeys.resize(decoder->FrameCount);
	for (uint32_t frame = 0; frame < decoder->FrameCount; ++frame) {
		uint32_t encoded_offset = ReadLE32(raw_index.data() + frame * 4);
		decoder->FrameKeys[frame] = static_cast<unsigned char>(encoded_offset & 1);
		decoder->FrameOffsets[frame] = encoded_offset & ~1U;
		if (decoder->FrameOffsets[frame] >= decoder->FileSize ||
			(frame && decoder->FrameOffsets[frame] <= decoder->FrameOffsets[frame - 1])) {
			SetError(decoder, "invalid BIK frame index");
			delete decoder;
			return NULL;
		}
	}

	unsigned int luma_width = ((decoder->Width + 7) >> 3) * 8;
	unsigned int luma_height = ((decoder->Height + 7) >> 3) * 8;
	unsigned int chroma_width = ((decoder->Width + 15) >> 4) * 8;
	unsigned int chroma_height = ((decoder->Height + 15) >> 4) * 8;
	for (int frame = 0; frame < 2; ++frame) {
		for (int plane = 0; plane < 3; ++plane) {
			Plane &destination = decoder->Video[frame].Planes[plane];
			destination.Width = plane ? chroma_width : luma_width;
			destination.Height = plane ? chroma_height : luma_height;
			destination.Stride = destination.Width;
			destination.Pixels.resize(
				static_cast<size_t>(destination.Stride) * destination.Height);
		}
	}

	size_t block_count =
		static_cast<size_t>((decoder->Width + 7) >> 3) *
		((decoder->Height + 7) >> 3);
	if (!block_count ||
		block_count > std::numeric_limits<size_t>::max() / (64 * SOURCE_COUNT)) {
		SetError(decoder, "BIK dimensions overflow decoder buffers");
		delete decoder;
		return NULL;
	}
	size_t bundle_size = block_count * 64;
	decoder->BundleStorage.resize(bundle_size * SOURCE_COUNT);
	for (int source = 0; source < SOURCE_COUNT; ++source) {
		Bundle &bundle = decoder->Bundles[source];
		bundle.Data = decoder->BundleStorage.data() + bundle_size * source;
		bundle.DataEnd = bundle.Data + bundle_size;
		bundle.Decoded = NULL;
		bundle.Current = NULL;
	}

	return decoder;
}

extern "C" void cw_bink_close(cw_bink_decoder *decoder)
{
	delete decoder;
}

extern "C" int cw_bink_info(
	cw_bink_decoder const *decoder,
	unsigned long *width,
	unsigned long *height,
	unsigned long *frame_count,
	unsigned long *fps_numerator,
	unsigned long *fps_denominator)
{
	if (!decoder) {
		return -1;
	}
	if (width) {
		*width = decoder->Width;
	}
	if (height) {
		*height = decoder->Height;
	}
	if (frame_count) {
		*frame_count = decoder->FrameCount;
	}
	if (fps_numerator) {
		*fps_numerator = decoder->FpsNumerator;
	}
	if (fps_denominator) {
		*fps_denominator = decoder->FpsDenominator;
	}
	return 0;
}

extern "C" int cw_bink_first(cw_bink_decoder *decoder)
{
	if (!decoder || !DecodeFrame(decoder, 0)) {
		return CW_BINK_ERROR;
	}
	return decoder->FrameCount == 1 ? CW_BINK_LAST : CW_BINK_MORE;
}

extern "C" int cw_bink_next(cw_bink_decoder *decoder)
{
	if (!decoder) {
		return CW_BINK_ERROR;
	}
	if (decoder->CurrentFrame + 1 >= decoder->FrameCount) {
		return CW_BINK_DONE;
	}
	if (!DecodeFrame(decoder, decoder->CurrentFrame + 1)) {
		return CW_BINK_ERROR;
	}
	return decoder->CurrentFrame + 1 == decoder->FrameCount
		? CW_BINK_LAST
		: CW_BINK_MORE;
}

extern "C" unsigned char const *cw_bink_get_rgb24(cw_bink_decoder const *decoder)
{
	return decoder && !decoder->RGB.empty() ? decoder->RGB.data() : NULL;
}

extern "C" int cw_bink_audio_info(
	cw_bink_decoder const *decoder,
	unsigned long *sample_rate,
	unsigned int *channels)
{
	if (!decoder || !decoder->AudioTracks) {
		return -1;
	}
	if (sample_rate) {
		*sample_rate = decoder->Audio.SampleRate;
	}
	if (channels) {
		*channels = decoder->Audio.Channels;
	}
	return 0;
}

extern "C" float const *cw_bink_get_audio_f32(
	cw_bink_decoder const *decoder,
	unsigned long *sample_frames)
{
	if (sample_frames) {
		*sample_frames = decoder && decoder->Audio.Channels
			? static_cast<unsigned long>(
				decoder->Audio.PCM.size() / decoder->Audio.Channels)
			: 0;
	}
	return decoder && !decoder->Audio.PCM.empty()
		? decoder->Audio.PCM.data()
		: NULL;
}

extern "C" char const *cw_bink_get_error(cw_bink_decoder const *decoder)
{
	return decoder ? decoder->Error.c_str() : "decoder is null";
}
