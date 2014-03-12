/* vgmtag - a command-line tag editor of VGM/VGZ media files.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#include <afc/stream.h>
#include <afc/Exception.h>
#include <cstdint>
#include <string>
#include <cstddef>

namespace vgm
{
	class VGMFile
	{
	public:
		enum class Format
		{
			vgm, vgz
		};

		VGMFile(const char * const srcFile);
		~VGMFile() {delete[] m_data;}

		void save(const char * const dest, const Format format);

		/*
		 * a) all tag values must be consecutive integers starting from 0
		 * b) title must be the first tag value
		 * c) notes must be the last tag value
		 */
		enum class Tag
		{
			title = 0, titleJP = 1, game = 2, gameJP = 3, system = 4, systemJP = 5,
			author = 6, authorJP = 7, date = 8, converter = 9, notes = 10
		};

		void setTag(const Tag name, const std::u16string &value);
		const std::u16string &getTag(const Tag name) const;

		Format getFormat() const {return m_format;}
	private:
		static const uint32_t VERSION_1_00 = 0x00000100, VERSION_1_01 = 0x00000101, VERSION_1_10 = 0x00000110,
				VERSION_1_50 = 0x00000150, VERSION_1_51 = 0x00000151, VERSION_1_60 = 0x00000160,
				VERSION_1_61 = 0x00000161;

		VGMFile(const VGMFile &) = delete;
		VGMFile &operator=(const VGMFile &) = delete;

		void normalise();

		void readHeader(afc::InputStream &in, size_t &cursor);
		void readGD3Info(afc::InputStream &in, size_t &cursor);
		void readData(afc::InputStream &in, size_t &cursor);

		void writeContent(afc::OutputStream &out) const;

		size_t version() const { return m_header.elements[VGMHeader::IDX_VERSION]; }

		size_t absoluteVgmDataOffset() const
		{
			return VGMHeader::POS_VGM_DATA + (version() < VERSION_1_50 ?
					VGMHeader::DEFAULT_VGM_DATA_OFFSET : m_header.elements[VGMHeader::IDX_VGM_DATA_OFFSET]);
		}

		struct VGMHeader
		{
			/* For versions prior to 1.50, VGM data offset should be 0 and the VGM data must start
			 * at absolute offset 0x40 (relative 0x0c).
			 */
			static const size_t DEFAULT_VGM_DATA_OFFSET = 0x0c;

			// Absolute positions of VGM header constituents in the header.
			static const uint32_t POS_EOF = 0x4, POS_SN_CLOCK = 0xc, POS_YM2413_CLOCK = 0x10, POS_GD3 = 0x14,
					POS_LOOP = 0x1c, POS_YM2112_CLOCK = 0x2c, POS_YM2151_CLOCK = 0x30, POS_VGM_DATA = 0x34;

			/* Despite of the platform endianness these values are stored in files in the little-endian format and
			   are converted into the platform format while parsing the file. */
			static const uint32_t VGM_FILE_ID = 0x206d6756; // 'Vgm ' in ASCII as 4 bytes casted to little-endian int32.

			// These are index values to access some VGM header elements.
			static const uint32_t IDX_ID = 0x00, IDX_EOF_OFFSET = 0x01, IDX_VERSION = 0x02, IDX_GD3_OFFSET = 0x05,
					IDX_RATE = 0x09, IDX_YM2612_CLOCK = 0x0b, IDX_YM2151_CLOCK = 0x0c, IDX_VGM_DATA_OFFSET = 0x0d;

			// The maximal number of elements of the VGM header (for all supported versions).
			static const size_t ELEMENT_COUNT = 0xc0 / 4;

			uint32_t elements[ELEMENT_COUNT];
		};

		struct GD3Info
		{
			/* Despite of the platform endianness these values are stored in files in the little-endian format and
			   are converted into the platform format while parsing the file. */
			static const uint32_t VGM_FILE_GD3_ID = 0x20336447; // 'Gd3 ' as 4 bytes casted to little-endian int32
			static const uint32_t VGM_FILE_GD3_VERSION = 0x00000100;

			static const uint32_t HEADER_SIZE = 0x0c;

			size_t dataSize;
			std::u16string tags[static_cast<size_t>(Tag::notes) + 1];
		};

		VGMHeader m_header;
		GD3Info m_gd3Info;
		unsigned char *m_data;
		size_t m_dataSize;
		Format m_format;
	};
}
