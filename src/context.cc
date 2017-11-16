
#include "internal.h"
#include <libc7zip.h>

class CbInStream : public C7ZipInStream {

public:
	in_stream_def m_def;

	CbInStream() {
		// muffin
	}

	virtual wstring GetExt() const {
		// TODO: use m_def->ext
		return L"7z";
	}

	virtual int Read(void *data, unsigned int size, unsigned int *processedSize) {
		int64_t processedSize64;
		int ret = m_def.read_cb(m_def.id, data, (int64_t)size, &processedSize64);
		*processedSize = (unsigned int)processedSize64;
		return ret;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) {
		int64_t newPos64;
		int ret = m_def.seek_cb(m_def.id, offset, int32_t(seekOrigin), &newPos64);
		if (newPosition) {
			*newPosition = (uint64_t)newPos64;
		}
		return ret;
	}

	virtual int GetSize(unsigned __int64 * size) {
		*size = (uint64_t)m_def.size;
		return 0;
	}

	virtual ~CbInStream() {
		// muffin
	}
};

class CbOutStream : public C7ZipOutStream {
public:
	out_stream_def m_def;

	CbOutStream() {
		// muffin
	}

	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize) {
		int64_t processedSize64;
		int ret = m_def.write_cb(m_def.id, data, (int64_t)size, &processedSize64);
		*processedSize = (unsigned int)processedSize64;
		return ret;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) {
		int64_t newPos64;
		int ret = m_def.seek_cb(m_def.id, offset, int32_t(seekOrigin), &newPos64);
		if (newPosition) {
			*newPosition = (uint64_t)newPos64;
		}
		return ret;
	}

	virtual int SetSize(unsigned __int64 size) {
		fprintf(stderr, "CbOutStream::SetSize: %lld\n", size);
		fflush(stderr);
		return 0;
	}

	virtual ~CbOutStream() {
		// muffin
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

void lib_free(lib *l) {
	delete l->lib;
	free(l);
}

struct in_stream {
	CbInStream *strm;
};

in_stream *in_stream_new() {
	in_stream *is = (in_stream *)calloc(1, sizeof(in_stream));
	is->strm = new CbInStream();
	return is;
}

in_stream_def *in_stream_get_def(in_stream *is) {
	return &is->strm->m_def;
}

void in_stream_free(in_stream *is) {
	delete is->strm;
	free(is);
}

struct out_stream {
	CbOutStream *strm;
};

out_stream *out_stream_new() {
	out_stream *os = (out_stream *)calloc(1, sizeof(out_stream));
	os->strm = new CbOutStream();
	return os;
};

out_stream_def *out_stream_get_def(out_stream *os) {
	return &os->strm->m_def;
}

void out_stream_free(out_stream *os) {
	delete os->strm;
	free(os);
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

struct item {
	C7ZipArchiveItem *itm;
};

item *archive_get_item(archive *a, int64_t index) {
	C7ZipArchiveItem *itm;
	if (!a->arch->GetItemInfo((unsigned int) index, &itm)) {
		return NULL;
	}

	item *i = (item*)calloc(1, sizeof(item));
	i->itm = itm;
	return i;
}

int archive_extract(archive *a, item *i, out_stream *os) {
	return a->arch->Extract(i->itm, os->strm);
}

void archive_item_free(item *i) {
	// sic. we don't need to free the C7ZipArchiveItem here
	free(i);
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
