#ifndef __7ZIP_IN_STREAM_WRAPPER_H__
#define __7ZIP_IN_STREAM_WRAPPER_H__

class C7ZipInStreamWrapper Z7_final:
    public IInStream,
    public IStreamGetSize,
    public CMyUnknownImp
{
public:
    C7ZipInStreamWrapper(C7ZipInStream * pInStream);
    ~C7ZipInStreamWrapper() {}

public:
    Z7_COM_UNKNOWN_IMP_2(IInStream, IStreamGetSize)
    Z7_IFACE_COM7_IMP(ISequentialInStream)
    Z7_IFACE_COM7_IMP(IInStream)
    Z7_IFACE_COM7_IMP(IStreamGetSize)

private:
    C7ZipInStream * m_pInStream;
};

#endif //__7ZIP_IN_STREAM_WRAPPER_H__
