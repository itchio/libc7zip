
#include "internal.h"
#include <libc7zip.h>

class CbInStream : public C7ZipInStream {

public:
	in_stream_def *m_def;

	CbInStream(in_stream_def *def) : m_def(def) {
	}

	virtual wstring GetExt() const {
		// TODO: use m_def->ext
		return L"7z";
	}

	virtual int Read(void *data, unsigned int size, unsigned int *processedSize) {
		int64_t processedSize64;
		int ret = m_def->read_cb(data, (int64_t)size, &processedSize64);
		*processedSize = (unsigned int)processedSize64;
		return ret;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) {
		int64_t newPos64;
		int ret = m_def->seek_cb(offset, int32_t(seekOrigin), &newPos64);
		if (newPosition) {
			*newPosition = (uint64_t)newPos64;
		}
		return ret;
	}

	virtual int GetSize(unsigned __int64 * size) {
		*size = (uint64_t)m_def->size;
		return 0;
	}

	virtual ~CbInStream() {
	}
};

class TestInStream : public C7ZipInStream
{
private:
	FILE * m_pFile;
	std::string m_strFileName;
	wstring m_strFileExt;
	int m_nFileSize;
public:
	TestInStream(std::string fileName) :
		m_strFileName(fileName),
		m_strFileExt(L"7z")
	{
		
		wprintf(L"fileName.c_str(): %s\n", fileName.c_str());
		m_pFile = fopen(fileName.c_str(), "rb");
		if (m_pFile) {
			fseek(m_pFile, 0, SEEK_END);
			m_nFileSize = ftell(m_pFile);
			fseek(m_pFile, 0, SEEK_SET);

			int pos = m_strFileName.find_last_of(".");

			if (pos != m_strFileName.npos) {
#ifdef _WIN32
				std::string tmp = m_strFileName.substr(pos + 1);
				int nLen = MultiByteToWideChar(CP_ACP, 0, tmp.c_str(), -1, NULL, 0);
				LPWSTR lpszW = new WCHAR[nLen];
				MultiByteToWideChar(CP_ACP, 0, 
									tmp.c_str(), -1, lpszW, nLen);
				m_strFileExt = lpszW;
				// free the string
				delete[] lpszW;
#else
				m_strFileExt = L"7z";
#endif
			}
			wprintf(L"Ext:%ls\n", m_strFileExt.c_str());
		}
		else {
			wprintf(L"fileName.c_str(): %s cant open\n", fileName.c_str());
		}
	}

	virtual ~TestInStream()
	{
		fclose(m_pFile);
	}

public:
	virtual wstring GetExt() const
	{
		wprintf(L"GetExt:%ls\n", m_strFileExt.c_str());
		return m_strFileExt;
	}

	virtual int Read(void *data, unsigned int size, unsigned int *processedSize)
	{
		if (!m_pFile)
			return 1;

		int count = fread(data, 1, size, m_pFile);
		wprintf(L"Read:%d %d\n", size, count);

		if (count >= 0) {
			if (processedSize != NULL)
				*processedSize = count;

			return 0;
		}

		return 1;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition)
	{
		if (!m_pFile)
			return 1;

		int result = fseek(m_pFile, (long)offset, seekOrigin);

		if (!result) {
			if (newPosition)
				*newPosition = ftell(m_pFile);

			return 0;
		}

		return result;
	}

	virtual int GetSize(unsigned __int64 * size)
	{
		if (size)
			*size = m_nFileSize;
		return 0;
	}
};

class TestOutStream : public C7ZipOutStream
{
private:
	FILE * m_pFile;
	std::string m_strFileName;
	wstring m_strFileExt;
	int m_nFileSize;
public:
	TestOutStream(std::string fileName) :
	  m_strFileName(fileName),
	  m_strFileExt(L"7z")
	{
		m_pFile = fopen(fileName.c_str(), "wb");
		m_nFileSize = 0;

		int pos = m_strFileName.find_last_of(".");

		if (pos != m_strFileName.npos)
		{
#ifdef _WIN32
			std::string tmp = m_strFileName.substr(pos + 1);
			int nLen = MultiByteToWideChar(CP_ACP, 0, tmp.c_str(), -1, NULL, 0);
			LPWSTR lpszW = new WCHAR[nLen];
			MultiByteToWideChar(CP_ACP, 0, 
			   tmp.c_str(), -1, lpszW, nLen);
			m_strFileExt = lpszW;
			// free the string
			delete[] lpszW;
#else
			m_strFileExt = L"7z";
#endif
		}
		wprintf(L"Ext:%ls\n", m_strFileExt.c_str());
	}

	virtual ~TestOutStream()
	{
		fclose(m_pFile);
	}

public:
	int GetFileSize() const 
	{
		return m_nFileSize;
	}

	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize)
	{
		int count = fwrite(data, 1, size, m_pFile);
		wprintf(L"Write:%d %d\n", size, count);

		if (count >= 0)
		{
			if (processedSize != NULL)
				*processedSize = count;

			m_nFileSize += count;
			return 0;
		}

		return 1;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition)
	{
		int result = fseek(m_pFile, (long)offset, seekOrigin);

		if (!result)
		{
			if (newPosition)
				*newPosition = ftell(m_pFile);

			return 0;
		}

		return result;
	}

	virtual int SetSize(unsigned __int64 size)
	{
		wprintf(L"SetFileSize:%ld\n", size);
		return 0;
	}
};

struct lib {
	C7ZipLibrary *lib;
};

lib *lib_new() {
	lib *l = (lib *)calloc(1, sizeof(lib));
	l->lib = new C7ZipLibrary();

	if (!l->lib->Initialize()) {
		fprintf(stderr, "Initialize fail!\n");
		delete l->lib;
		free(l);
		return NULL;
	}

	return l;
}

struct in_stream {
	CbInStream *strm;
};

in_stream *in_stream_new() {
	in_stream *s = (in_stream *)calloc(1, sizeof(in_stream));
	in_stream_def *def = (in_stream_def *)calloc(1, sizeof(in_stream_def));
	s->strm = new CbInStream(def);
	return s;
}

in_stream_def *in_stream_get_def(in_stream *s) {
	return s->strm->m_def;
}

struct archive {
	C7ZipArchive *arch;
};

archive *archive_open(lib *l, in_stream *s) {
	C7ZipArchive *arch = NULL;
	if (!l->lib->OpenArchive(s->strm, &arch)) {
		fprintf(stderr, "Could not open archive");
		return NULL;
	}

	archive *a = (archive*)calloc(1, sizeof(archive));
	a->arch = arch;
	return a;
}

int64_t archive_get_item_count(archive *a) {
	unsigned int numItems;
	if (!a->arch->GetItemCount(&numItems)) {
		return -1;
	}
	return (int64_t)numItems;
}

// #ifdef _WIN32
// int _tmain(int argc, _TCHAR* argv[])
// #else
// int main(int argc, char * argv[])
// #endif
// {
// 	C7ZipLibrary lib;

// 	if (!lib.Initialize()) {
// 		wprintf(L"initialize fail!\n");
// 		return 1;
// 	}

// 	C7ZipArchive * pArchive = NULL;

// 	TestInStream stream("Test7Zip.7z");
// 	TestOutStream oStream("TestResult.txt");
// 	if (lib.OpenArchive(&stream, &pArchive)) {
// 		unsigned int numItems = 0;

// 		pArchive->GetItemCount(&numItems);

// 		wprintf(L"%d\n", numItems);

// 		for(unsigned int i = 0;i < numItems;i++) {
// 			C7ZipArchiveItem * pArchiveItem = NULL;

// 			if (pArchive->GetItemInfo(i, &pArchiveItem)) {
// 				wprintf(L"%d,%ls,%d\n", pArchiveItem->GetArchiveIndex(),
// 						pArchiveItem->GetFullPath().c_str(),
// 						pArchiveItem->IsDir());

// 				//set archive password or item password
// 				pArchive->SetArchivePassword(L"test");
// 				if (i==0) {
// 					//Or set password for each archive item
// 					//pArchiveItem->SetArchiveItemPassword(L"test");
// 					pArchive->Extract(pArchiveItem, &oStream);
// 				}
// 			} //if
// 		}//for
// 	}
// 	else {
// 		wprintf(L"open archive Test7Zip.7z fail\n");
// 	}

// 	if (pArchive != NULL)
// 		delete pArchive;

// 	return 0;
// }
