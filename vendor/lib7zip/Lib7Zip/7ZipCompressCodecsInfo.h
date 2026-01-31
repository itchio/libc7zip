#ifndef __7ZIP_COMPRESS_CODECS_INFO_H__
#define __7ZIP_COMPRESS_CODECS_INFO_H__

class C7ZipCompressCodecsInfo Z7_final : public ICompressCodecsInfo,
    public CMyUnknownImp,
    public virtual C7ZipObject
{
public:
    C7ZipCompressCodecsInfo(C7ZipLibrary * pLibrary);
    ~C7ZipCompressCodecsInfo();

    Z7_COM_UNKNOWN_IMP_1(ICompressCodecsInfo)
    Z7_IFACE_COM7_IMP(ICompressCodecsInfo)

    void InitData();
private:
    C7ZipLibrary * m_pLibrary;
    C7ZipObjectPtrArray m_CodecInfoArray;
};

#endif //__7ZIP_COMPRESS_CODECS_INFO_H__
