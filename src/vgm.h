#include "stream.h"
#include <afc/File.h>
#include <afc/Exception.h>
#include <stdint.h>
#include <string>

namespace vgm
{
	class VGMFile
	{
	public:
		~VGMFile() {delete[] m_data;}

		static VGMFile load(const afc::File &src);
		void save(const afc::File &dest, const bool compress = false);

		/*
		 * a) all tag values must be consequtive integers starting from 0
		 * b) title must be the first tag value
		 * c) notes must be the last tag value
		 */
		enum class Tag
		{
			title = 0, titleJP = 1, game = 2, gameJP = 3, system = 4, systemJP = 5,
			author = 6, authorJP = 7, date = 8, converter = 9, notes = 10
		};

		void setTag(const Tag name, const std::wstring &value);
		const std::wstring &getTag(const Tag name) const;
	private:
		VGMFile() : m_data(0), m_dataSize(0) {}

		void normalise();

		void readHeader(InputStream &in);
		void readGD3Info(InputStream &in);
		void readData(InputStream &in);

		void writeHeader(OutputStream &out) const;
		void writeGD3Info(OutputStream &out) const;
		void writeData(OutputStream &out) const;

		struct VGMHeader
		{
			static const uint32_t POS_EOF = 0x4, POS_SN_CLOCK = 0xc, POS_YM2413_CLOCK = 0x10, POS_GD3 = 0x14, POS_LOOP = 0x1c,
						POS_YM2112_CLOCK = 0x2c, POS_YM2151_CLOCK = 0x30, POS_VGM_DATA = 0x34;
			static const uint32_t VGM_FILE_ID = 0x206d6756; // in LE
			static const uint32_t VGM_FILE_VERSION = 0x00000150; // version in LE
			static const uint32_t HEADER_SIZE = 0x40;

			uint32_t id, eofOffset, version, snClock,
					ym2413Clock, gd3Offset, sampleCount, loopOffset,
					loopSampleCount, rate, snFeedback, ym2612Clock,
					ym2151Clock, vgmDataOffset;
		};
		struct GD3Info
		{
			static const uint32_t VGM_FILE_GD3_ID = 0x20336447; // 'Gd3 ' in LE
			static const uint32_t VGM_FILE_GD3_VERSION = 0x00000100; // version in LE
			static const uint32_t HEADER_SIZE = 0x0c;

			size_t dataSize;
			std::wstring tags[static_cast<size_t>(Tag::notes) + 1];
		};

		VGMHeader m_header;
		GD3Info m_gd3Info;
		unsigned char *m_data;
		size_t m_dataSize;
	};
}
