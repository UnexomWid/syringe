#include <vcclr.h>

#include "pe.hxx"

// clang-format off
template <typename T> class DisposableGuard {
    private:
    gcroot<T^> disposable;

    public:
    DisposableGuard(T^ disposable) {
        this->disposable = disposable;
    }

    ~DisposableGuard() {
        disposable->Close();
    }
};

pe::Type pe::GetType(String^ path) {
    try {
        auto fileStream = gcnew FileStream(path, FileMode::Open, FileAccess::Read);
        DisposableGuard<FileStream> fileStreamGuard(fileStream);

        auto binaryReader = gcnew BinaryReader(fileStream);
        DisposableGuard<BinaryReader> binaryReaderGuard(binaryReader);

        auto magic = binaryReader->ReadUInt16();

        // MZ
        if (magic != 0x5A4D) {
            return Type::Invalid;
        }

        // e_lfanew AKA new exe header offset
        fileStream->Seek(0x3c, SeekOrigin::Begin);

        auto newExeOffset = binaryReader->ReadInt32();
        fileStream->Seek(newExeOffset, SeekOrigin::Begin);

        auto ntMagic = binaryReader->ReadUInt32();

        // PE\0\0
        if (ntMagic != 0x00004550) {
            return Type::Invalid;
        }

        // The size of the NT header is 20 bytes. Jump over it to reach the optional header.
        fileStream->Seek(20, SeekOrigin::Current);

        auto optionalMagic = binaryReader->ReadUInt16();

        switch (optionalMagic) {
        case 0x10b:
            return Type::Pe32;
        case 0x20b:
            return Type::Pe32Plus;
        default:
            return Type::Invalid;
        }
    } catch (System::Exception^) {
        return Type::Invalid;
    }
}
// clang-format on