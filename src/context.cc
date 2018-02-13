
#include "internal.h"
#include "utf_convert.h"
#include <string.h>
#include <libc7zip.h>
#include <stdlib.h>

class CbInStream : public C7ZipInStream {

public:
	in_stream_def m_def;
	wstring m_strFileExt;

	CbInStream() {
		// muffin
	}

	void CommitDef() {
		if (m_def.ext) {
			// the caller allocated m_def.ext, and is also in charge of freeing it
			m_strFileExt = FromCString(m_def.ext);
		}
	}

	virtual wstring GetExt() const {
		return m_strFileExt;
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

class CbSequentialOutStream : public C7ZipSequentialOutStream {
public:
	out_stream_def m_def;

	CbSequentialOutStream() {
		// muffin
	}

	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize) {
		int64_t processedSize64;
		int ret = m_def.write_cb(m_def.id, data, (int64_t)size, &processedSize64);
		*processedSize = (unsigned int)processedSize64;
		return ret;
	}

	virtual ~CbSequentialOutStream() {
		// muffin
	}
};

struct in_stream {
	CbInStream *strm;
};

struct out_stream {
	CbSequentialOutStream *strm;
};

class CbExtractCallback : public C7ZipExtractCallback {
public:
	extract_callback_def m_def;

	CbExtractCallback() {
		// muffin
	}

  virtual void SetTotal(unsigned __int64 size) {
		m_def.set_total_cb(m_def.id, (int64_t)size);
	}

  virtual void SetCompleted(const unsigned __int64 *completeValue) {
		if (completeValue) {
			m_def.set_completed_cb(m_def.id, (int64_t)*completeValue);
		}
	}

	virtual C7ZipSequentialOutStream *GetStream(int index) {
		auto os = m_def.get_stream_cb(m_def.id, (__int64)index);
		if (!os) {
			return NULL;
		}
		return os->strm;
	}

	virtual void SetOperationResult(int operationResult) {
		m_def.set_operation_result_cb(m_def.id, operationResult);
	}

	virtual ~CbExtractCallback() {
		// muffin
	}
};

struct lib {
	C7ZipLibrary *_lib;
};

MYEXPORT lib *lib_new() {
	lib *l = (lib *)calloc(1, sizeof(lib));
	l->_lib = new C7ZipLibrary();

	if (!l->_lib->Initialize()) {
		fprintf(stderr, "Initialize fail!\n");
		delete l->_lib;
		free(l);
		return NULL;
	}

	return l;
}

MYEXPORT int32_t lib_get_last_error(lib *l) {
	return (int32_t) l->_lib->GetLastError();
}

MYEXPORT void lib_free(lib *l) {
	delete l->_lib;
	free(l);
}

MYEXPORT in_stream *in_stream_new() {
	in_stream *is = (in_stream *)calloc(1, sizeof(in_stream));
	is->strm = new CbInStream();
	return is;
}

MYEXPORT in_stream_def *in_stream_get_def(in_stream *is) {
	return &is->strm->m_def;
}

MYEXPORT void in_stream_commit_def(in_stream *is) {
	is->strm->CommitDef();
}

MYEXPORT void in_stream_free(in_stream *is) {
	delete is->strm;
	free(is);
}

MYEXPORT out_stream *out_stream_new() {
	out_stream *os = (out_stream *)calloc(1, sizeof(out_stream));
	os->strm = new CbSequentialOutStream();
	return os;
};

MYEXPORT out_stream_def *out_stream_get_def(out_stream *os) {
	return &os->strm->m_def;
}

MYEXPORT void out_stream_free(out_stream *os) {
	delete os->strm;
	free(os);
}

struct archive {
	C7ZipArchive *arch;
};

MYEXPORT archive *archive_open(lib *l, in_stream *s, int32_t by_signature) {
	C7ZipArchive *arch = NULL;
	if (!l->_lib->OpenArchive(s->strm, &arch, by_signature != 0)) {
		return NULL;
	}

	archive *a = (archive*)calloc(1, sizeof(archive));
	a->arch = arch;
	return a;
}

MYEXPORT void archive_close(archive *a) {
	a->arch->Close();
}

MYEXPORT void archive_free(archive *a) {
	delete a->arch;
	free(a);
}

MYEXPORT int64_t archive_get_item_count(archive *a) {
	unsigned int numItems;
	if (!a->arch->GetItemCount(&numItems)) {
		return -1;
	}
	return (int64_t)numItems;
}

MYEXPORT char *archive_get_archive_format(archive *a) {
	std::wstring ret = a->arch->GetArchiveFormat();
	return ToCString(ret);
}

struct item {
	C7ZipArchiveItem *itm;
};

MYEXPORT item *archive_get_item(archive *a, int64_t index) {
	C7ZipArchiveItem *itm;
	if (!a->arch->GetItemInfo((unsigned int) index, &itm)) {
		return NULL;
	}

	item *i = (item*)calloc(1, sizeof(item));
	i->itm = itm;
	return i;
}

MYEXPORT int32_t item_get_archive_index(item *i) {
	return (int32_t)i->itm->GetArchiveIndex();
}

MYEXPORT int archive_extract_item(archive *a, item *i, out_stream *os) {
	return a->arch->Extract(i->itm, os->strm);
}

MYEXPORT void item_free(item *i) {
	// sic. we don't need to free the C7ZipArchiveItem here
	free(i);
}

MYEXPORT char *item_get_string_property(item *i, int32_t property_index, int32_t *success) {
	std::wstring ret = L"";
	auto pi = (lib7zip::PropertyIndexEnum)(property_index);
	auto success_in = i->itm->GetStringProperty(pi, ret);
	if (success) {
		*success = success_in;
	}
	return ToCString(ret);
}

// this is exported so we don't free a string with a
// different memory allocator
MYEXPORT void string_free(char *s) {
	free(s);
}

MYEXPORT uint64_t item_get_uint64_property(item *i, int32_t property_index, int32_t *success) {
	unsigned __int64 ret = 0;
	auto pi = (lib7zip::PropertyIndexEnum)(property_index);
	auto success_in = i->itm->GetUInt64Property(pi, ret);
	if (success) {
		*success = success_in;
	}
	return ret;
}

MYEXPORT int32_t item_get_bool_property(item *i, int32_t property_index, int32_t *success) {
	bool ret = false;
	auto pi = (lib7zip::PropertyIndexEnum)(property_index);
	auto success_in = i->itm->GetBoolProperty(pi, ret);
	if (success) {
		*success = success_in;
	}
	return int32_t(ret);
}

struct extract_callback {
	CbExtractCallback *cb;
};

MYEXPORT extract_callback *extract_callback_new() {
	extract_callback *ec = (extract_callback *)calloc(1, sizeof(extract_callback));
	ec->cb = new CbExtractCallback();
	return ec;
}

MYEXPORT extract_callback_def *extract_callback_get_def(extract_callback *ec) {
	return &ec->cb->m_def;
}

MYEXPORT void extract_callback_free(extract_callback *ec) {
	delete ec->cb;
	free(ec);
}

MYEXPORT int archive_extract_several(archive *a, int64_t *indices, int32_t num_indices, extract_callback *ec) {
	// that's a bit silly but oh well
	auto uIndices = new unsigned int[num_indices];
	for (int32_t i = 0 ; i < num_indices; i++) {
		uIndices[i] = (unsigned int)indices[i];
	}

	auto ret = a->arch->ExtractSeveral(uIndices, (int)num_indices, ec->cb);
	delete[] uIndices;
	return ret;
}
