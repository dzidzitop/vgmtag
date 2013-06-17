#include "stream.h"
#include <afc/Exception.h>
#include <cassert>
#include <vector>

using afc::File;
using afc::IOException;
using afc::IllegalStateException;
using afc::MalformedFormatException;
using afc::UnsupportedFormatException;
using namespace std;

namespace
{
	void throwIOException(const char * const message = "")
	{
		throw IOException(message);
	}

	void throwCannotOpenFileIOException(const File &file)
	{
		throwIOException(("unable to open file '" + file.absolutePath() + '\'').c_str());
	}

	void throwIllegalStateException(const char * const message = "")
	{
		throw IllegalStateException(message);
	}

	void throwMalformedFormatException(const char * const message = "")
	{
		throw MalformedFormatException(message);
	}

	void throwUnsupportedFormatException(const char * const message = "")
	{
		throw UnsupportedFormatException(message);
	}

	inline void ensureNotClosed(FILE * const file) {
		if (file == 0) {
			throwIOException("stream is closed");
		}
	}

	inline void ensureNotClosed(gzFile file) {
		if (file == 0) {
			throwIOException("stream is closed");
		}
	}
}

vgm::FileInputStream::FileInputStream(const File &file)
{
	m_file = fopen(file.path().c_str(), "rb");
	if (m_file == 0) {
		throwCannotOpenFileIOException(file);
	}
}

size_t vgm::FileInputStream::read(unsigned char * const data, const size_t n)
{
	ensureNotClosed(m_file);
	const size_t count = fread(data, sizeof(unsigned char), n, m_file);
	if (count != n) {
		if (ferror(m_file)) {
			throwIOException("error encountered while reading from file");
		}
	}
	return count;
}

void vgm::FileInputStream::reset()
{
	ensureNotClosed(m_file);
	if (fseek(m_file, 0, SEEK_SET) != 0) {
		throwIOException("unable to reset stream");
	}
}

size_t vgm::FileInputStream::skip(const size_t n)
{
	ensureNotClosed(m_file);
	const long currPos = ftell(m_file);
	if (fseek(m_file, 0, SEEK_END) != 0) {
		throwIOException("unable to skip data in stream");
	}
	const long endPos = ftell(m_file);
	const size_t tail = endPos - currPos;
	if (n >= tail) {
		return tail;
	}
	if (fseek(m_file, currPos + n, SEEK_SET) != 0) {
		throwIOException("unable to skip data in stream");
	}
	return n;
}

void vgm::FileInputStream::close()
{
	if (m_file == 0) {
		return;
	}
	if (fclose(m_file) != 0) {
		throwIOException("file is not closed");
	}
	m_file = 0;
}

vgm::FileOutputStream::FileOutputStream(const File &file)
{
	m_file = fopen(file.path().c_str(), "wb");
	if (m_file == 0) {
		throwCannotOpenFileIOException(file);
	}
}

void vgm::FileOutputStream::write(const unsigned char * const data, const size_t n)
{
	ensureNotClosed(m_file);
	if (fwrite(data, sizeof(unsigned char), n, m_file) != n) {
		throwIOException("error encountered while writting to file");
	}
}

void vgm::FileOutputStream::close()
{
	if (m_file == 0) {
		return;
	}
	if (fclose(m_file) != 0) {
		throwIOException("file is not closed");
	}
	m_file = 0;
}

vgm::GZipFileInputStream::GZipFileInputStream(const File &file)
{
	m_file = gzopen(file.path().c_str(), "rb");
	if (m_file == 0) {
		throwCannotOpenFileIOException(file);
	}
}

// TODO process closed stream correctly
size_t vgm::GZipFileInputStream::read(unsigned char * const buf, const size_t n)
{
	ensureNotClosed(m_file);
	const size_t count = gzread(m_file, buf, n);
	if (count != n) {
		int errorCode;
		const char * const msg = gzerror(m_file, &errorCode);
		switch (errorCode) {
		case Z_OK:
			break;
		case Z_ERRNO:
			throwIOException(msg);
		default:
			throwMalformedFormatException(msg);
		}
	}
	return count;
}

void vgm::GZipFileInputStream::close()
{
	if (m_file == nullptr) {
		return;
	}
	if (gzclose(m_file) != 0) {
		throwIOException("file is not closed");
	}
	m_file = nullptr;
}

void vgm::GZipFileInputStream::reset()
{
	ensureNotClosed(m_file);
	if (gzseek(m_file, 0, SEEK_SET) != 0) {
		throwIOException("unable to reset stream");
	}
}

size_t vgm::GZipFileInputStream::skip(const size_t n)
{
	vector<unsigned char> buf(n);
	return read(&buf[0], n);
}

vgm::GZipFileOutputStream::GZipFileOutputStream(const File &file)
{
	m_file = gzopen(file.path().c_str(), "wb");
	if (m_file == 0) {
		throwCannotOpenFileIOException(file);
	}
}

void vgm::GZipFileOutputStream::write(const unsigned char * const data, const size_t n)
{
	ensureNotClosed(m_file);
	if (gzwrite(m_file, data, n) == 0) {
		throwIOException("error encountered while writing to file");
	}
}

void vgm::GZipFileOutputStream::close()
{
	if (m_file == 0) {
		return;
	}
	if (gzclose(m_file) != 0) {
		throwIOException("file is not closed");
	}
	m_file = 0;
}
