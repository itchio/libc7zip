#include "lib7zip.h"
#ifdef S_OK
#undef S_OK
#endif

#if !defined(_WIN32) && !defined(_OS2)
#include "CPP/Common/MyWindows.h"
#endif

#include "C/7zVersion.h"
#include "CPP/7zip/Archive/IArchive.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/Common/MyCom.h"
#include "CPP/7zip/ICoder.h"
#include "CPP/7zip/IPassword.h"
#include "Common/ComTry.h"
#include "Windows/PropVariant.h"
using namespace NWindows;

#include "7ZipInStreamWrapper.h"

/*----------------- C7ZipInStreamWrapper ---------------------*/
C7ZipInStreamWrapper::C7ZipInStreamWrapper(C7ZipInStream * pInStream) :
m_pInStream(pInStream)
{
}

Z7_COM7F_IMF(C7ZipInStreamWrapper::Read(void *data, UInt32 size, UInt32 *processedSize))
{
    return m_pInStream->Read(data,size,processedSize);
}

Z7_COM7F_IMF(C7ZipInStreamWrapper::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
{
    // Cast to handle UInt64 (unsigned long) vs unsigned __int64 (unsigned long long)
    return m_pInStream->Seek(offset,seekOrigin,reinterpret_cast<unsigned __int64*>(newPosition));
}

Z7_COM7F_IMF(C7ZipInStreamWrapper::GetSize(UInt64 *size))
{
    // Cast to handle UInt64 (unsigned long) vs unsigned __int64 (unsigned long long)
    return m_pInStream->GetSize(reinterpret_cast<unsigned __int64*>(size));
}
