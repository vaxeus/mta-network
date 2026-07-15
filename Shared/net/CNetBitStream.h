#pragma once

#include "../sdk/net/bitstream.h"
#include <vector>
#include <type_traits>

class CNetBitStream final : public NetBitStreamInterface {
public:
    CNetBitStream();
    CNetBitStream(const void* pData, size_t uiDataSize);
    explicit CNetBitStream(unsigned short usVersion);
    CNetBitStream(const void* pData, size_t uiDataSize, unsigned short usVersion);

    using NetBitStreamInterface::Read;
    using NetBitStreamInterface::Write;
    using NetBitStreamInterface::WriteBits;
    using NetBitStreamInterface::ReadBits;
    using NetBitStreamInterface::ReadBit;

    int  GetReadOffsetAsBits() override;
    void SetReadOffsetAsBits(int iOffset) override;

    void Reset() override;
    void ResetReadPointer() override;

    void Write(const unsigned char& input) override;
    void Write(const char& input) override;
    void Write(const unsigned short& input) override;
    void Write(const short& input) override;
    void Write(const unsigned int& input) override;
    void Write(const int& input) override;
    void Write(const float& input) override;
    void Write(const double& input) override;
    void Write(const char* input, int numberOfBytes) override;
    void Write(const ISyncStructure* syncStruct) override;

    void WriteCompressed(const unsigned char& input) override;
    void WriteCompressed(const char& input) override;
    void WriteCompressed(const unsigned short& input) override;
    void WriteCompressed(const short& input) override;
    void WriteCompressed(const unsigned int& input) override;
    void WriteCompressed(const int& input) override;

    void WriteBits(const char* input, unsigned int numbits) override;
    void WriteBit(bool input) override;

    void WriteNormVector(float x, float y, float z) override;
    void WriteVector(float x, float y, float z) override;
    void WriteNormQuat(float w, float x, float y, float z) override;
    void WriteOrthMatrix(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22) override;

    bool Read(unsigned char& output) override;
    bool Read(char& output) override;
    bool Read(unsigned short& output) override;
    bool Read(short& output) override;
    bool Read(unsigned int& output) override;
    bool Read(int& output) override;
    bool Read(float& output) override;
    bool Read(double& output) override;
    bool Read(char* output, int numberOfBytes) override;
    bool Read(ISyncStructure* syncStruct) override;

    bool ReadCompressed(unsigned char& output) override;
    bool ReadCompressed(char& output) override;
    bool ReadCompressed(unsigned short& output) override;
    bool ReadCompressed(short& output) override;
    bool ReadCompressed(unsigned int& output) override;
    bool ReadCompressed(int& output) override;

    bool ReadBits(char* output, unsigned int numbits) override;
    bool ReadBit() override;

    bool ReadNormVector(float& x, float& y, float& z) override;
    bool ReadVector(float& x, float& y, float& z) override;
    bool ReadNormQuat(float& w, float& x, float& y, float& z) override;
    bool ReadOrthMatrix(float& m00, float& m01, float& m02, float& m10, float& m11, float& m12, float& m20, float& m21, float& m22) override;

    int GetNumberOfBitsUsed() const override;
    int GetNumberOfBytesUsed() const override;
    int GetNumberOfUnreadBits() const override;

    void AlignWriteToByteBoundary() const override;
    void AlignReadToByteBoundary() const override;

    unsigned char* GetData() const override;

    unsigned short Version() const override;

protected:
    ~CNetBitStream() override = default;

private:
    void WriteCompressed(const float& input) override;
    void WriteCompressed(const double& input) override;
    bool ReadCompressed(float& output) override;
    bool ReadCompressed(double& output) override;

    void EnsureWriteCapacityBits(unsigned int uiExtraBits);
    void WriteRawBytes(const unsigned char* pBytes, int iNumBytes);
    bool ReadRawBytes(unsigned char* pBytes, int iNumBytes);

    void WriteUInt(unsigned int value, unsigned int numBits);
    bool ReadUInt(unsigned int& value, unsigned int numBits);

    template <typename U>
    void WriteCompressedUnsigned(U value);
    template <typename U>
    bool ReadCompressedUnsigned(U& value);
    template <typename S, typename U>
    void WriteCompressedSigned(S value);
    template <typename S, typename U>
    bool ReadCompressedSigned(S& value);

    std::vector<unsigned char> m_Data;
    mutable unsigned int       m_uiWriteBitOffset;
    mutable unsigned int       m_uiReadBitOffset;
    unsigned short             m_usVersion;
};
