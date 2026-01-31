#ifndef __7ZIP_ARCHIVE_OPEN_CALLBACK_H__
#define  __7ZIP_ARCHIVE_OPEN_CALLBACK_H__

#define E_NEEDPASSWORD ((HRESULT)0x80040001L)

class C7ZipArchiveOpenCallback Z7_final:
public IArchiveOpenCallback,
    public ICryptoGetTextPassword,
	public IArchiveOpenVolumeCallback,
	public IArchiveOpenSetSubArchiveName,
    public CMyUnknownImp
{
 public:
	Z7_COM_UNKNOWN_IMP_3(
					IArchiveOpenVolumeCallback,
					ICryptoGetTextPassword,
					IArchiveOpenSetSubArchiveName
					)

	Z7_IFACE_COM7_IMP(IArchiveOpenCallback)
	Z7_IFACE_COM7_IMP(IArchiveOpenVolumeCallback)
	Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)
	Z7_IFACE_COM7_IMP(IArchiveOpenSetSubArchiveName)

public:
    bool PasswordIsDefined;
    wstring Password;

	wstring _subArchiveName;
	bool _subArchiveMode;
	UInt64 TotalSize;

    C7ZipMultiVolumes * m_pMultiVolumes;
	bool m_bMultiVolume;

 C7ZipArchiveOpenCallback(C7ZipMultiVolumes * pMultiVolumes) : PasswordIsDefined(false),
		_subArchiveMode(false),
		m_pMultiVolumes(pMultiVolumes),
		m_bMultiVolume(pMultiVolumes != NULL) {
	}
};

#endif // __7ZIP_ARCHIVE_OPEN_CALLBACK_H__
