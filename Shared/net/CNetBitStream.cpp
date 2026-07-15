#include "CNetBitStream.h"
#include <algorithm>
#include <cmath>
#include <cstring>

CNetBitStream::CNetBitStream() : m_uiWriteBitOffset(0), m_uiReadBitOffset(0), m_usVersion(static_cast<unsigned short>(eBitStreamVersion::Latest)) {}

CNetBitStream::CNetBitStream(const void* pData, size_t uiDataSize) : m_uiWriteBitOffset(0), m_uiReadBitOffset(0), m_usVersion(static_cast<unsigned short>(eBitStreamVersion::Latest))
{
    if (pData != nullptr && uiDataSize > 0)
    {
        const unsigned char* pBytes = reinterpret_cast<const unsigned char*>(pData);
        m_Data.assign(pBytes, pBytes + uiDataSize);
        m_uiWriteBitOffset = static_cast<unsigned int>(uiDataSize) * 8;
    }
}

CNetBitStream::CNetBitStream(unsigned short usVersion) : m_uiWriteBitOffset(0), m_uiReadBitOffset(0), m_usVersion(usVersion) {}

CNetBitStream::CNetBitStream(const void* pData, size_t uiDataSize, unsigned short usVersion) : m_uiWriteBitOffset(0), m_uiReadBitOffset(0), m_usVersion(usVersion)
{
    if (pData != nullptr && uiDataSize > 0)
    {
        const unsigned char* pBytes = reinterpret_cast<const unsigned char*>(pData);
        m_Data.assign(pBytes, pBytes + uiDataSize);
        m_uiWriteBitOffset = static_cast<unsigned int>(uiDataSize) * 8;
    }
}

void CNetBitStream::EnsureWriteCapacityBits(unsigned int uiExtraBits)
{
    unsigned int uiRequiredBits = m_uiWriteBitOffset + uiExtraBits;
    unsigned int uiRequiredBytes = (uiRequiredBits + 7) / 8;
    if (uiRequiredBytes > m_Data.size())
        m_Data.resize(uiRequiredBytes, 0);
}

void CNetBitStream::WriteBit(bool input)
{
    EnsureWriteCapacityBits(1);
    unsigned int  uiByte = m_uiWriteBitOffset >> 3;
    unsigned int  uiBitInByte = m_uiWriteBitOffset & 7;
    unsigned char mask = static_cast<unsigned char>(1u << (7 - uiBitInByte));
    if (input)
        m_Data[uiByte] |= mask;
    else
        m_Data[uiByte] &= static_cast<unsigned char>(~mask);
    ++m_uiWriteBitOffset;
}

bool CNetBitStream::ReadBit()
{
    if (m_uiReadBitOffset >= m_uiWriteBitOffset)
        return false;
    unsigned int uiByte = m_uiReadBitOffset >> 3;
    unsigned int uiBitInByte = m_uiReadBitOffset & 7;
    bool         bBit = (m_Data[uiByte] >> (7 - uiBitInByte)) & 1;
    ++m_uiReadBitOffset;
    return bBit;
}

void CNetBitStream::WriteBits(const char* input, unsigned int numbits)
{
    if (numbits == 0)
        return;
    EnsureWriteCapacityBits(numbits);
    const unsigned char* pSrc = reinterpret_cast<const unsigned char*>(input);

    unsigned int uiFullBytes = numbits >> 3;
    unsigned int uiRemainingBits = numbits & 7;

    if ((m_uiWriteBitOffset & 7) == 0 && uiFullBytes > 0)
    {
        unsigned int uiDestByte = m_uiWriteBitOffset >> 3;
        std::memcpy(&m_Data[uiDestByte], pSrc, uiFullBytes);
        m_uiWriteBitOffset += uiFullBytes * 8;
    }
    else
    {
        for (unsigned int i = 0; i < uiFullBytes * 8; ++i)
            WriteBit((pSrc[i >> 3] >> (7 - (i & 7))) & 1);
    }

    for (unsigned int i = 0; i < uiRemainingBits; ++i)
        WriteBit((pSrc[uiFullBytes] >> (uiRemainingBits - 1 - i)) & 1);
}

bool CNetBitStream::ReadBits(char* output, unsigned int numbits)
{
    if (numbits == 0)
        return true;
    if (m_uiReadBitOffset + numbits > m_uiWriteBitOffset)
        return false;

    unsigned char* pDst = reinterpret_cast<unsigned char*>(output);
    unsigned int   uiFullBytes = numbits >> 3;
    unsigned int   uiRemainingBits = numbits & 7;
    std::memset(pDst, 0, (numbits + 7) / 8);

    if ((m_uiReadBitOffset & 7) == 0 && uiFullBytes > 0)
    {
        unsigned int uiSrcByte = m_uiReadBitOffset >> 3;
        std::memcpy(pDst, &m_Data[uiSrcByte], uiFullBytes);
        m_uiReadBitOffset += uiFullBytes * 8;
    }
    else
    {
        for (unsigned int i = 0; i < uiFullBytes * 8; ++i)
        {
            if (ReadBit())
                pDst[i >> 3] |= static_cast<unsigned char>(1u << (7 - (i & 7)));
        }
    }

    for (unsigned int i = 0; i < uiRemainingBits; ++i)
    {
        if (ReadBit())
            pDst[uiFullBytes] |= static_cast<unsigned char>(1u << (uiRemainingBits - 1 - i));
    }
    return true;
}

void CNetBitStream::WriteRawBytes(const unsigned char* pBytes, int iNumBytes)
{
    WriteBits(reinterpret_cast<const char*>(pBytes), static_cast<unsigned int>(iNumBytes) * 8);
}

bool CNetBitStream::ReadRawBytes(unsigned char* pBytes, int iNumBytes)
{
    return ReadBits(reinterpret_cast<char*>(pBytes), static_cast<unsigned int>(iNumBytes) * 8);
}

void CNetBitStream::WriteUInt(unsigned int value, unsigned int numBits)
{
    for (int i = static_cast<int>(numBits) - 1; i >= 0; --i)
        WriteBit(((value >> i) & 1) != 0);
}

bool CNetBitStream::ReadUInt(unsigned int& value, unsigned int numBits)
{
    value = 0;
    for (unsigned int i = 0; i < numBits; ++i)
    {
        if (m_uiReadBitOffset >= m_uiWriteBitOffset)
            return false;
        value = (value << 1) | (ReadBit() ? 1u : 0u);
    }
    return true;
}

int CNetBitStream::GetReadOffsetAsBits()
{
    return static_cast<int>(m_uiReadBitOffset);
}

void CNetBitStream::SetReadOffsetAsBits(int iOffset)
{
    m_uiReadBitOffset = static_cast<unsigned int>(iOffset);
}

void CNetBitStream::Reset()
{
    m_Data.clear();
    m_uiWriteBitOffset = 0;
    m_uiReadBitOffset = 0;
}

void CNetBitStream::ResetReadPointer()
{
    m_uiReadBitOffset = 0;
}

void CNetBitStream::Write(const unsigned char& input)
{
    WriteRawBytes(&input, sizeof(input));
}
void CNetBitStream::Write(const char& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const unsigned short& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const short& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const unsigned int& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const int& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const float& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const double& input)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(&input), sizeof(input));
}
void CNetBitStream::Write(const char* input, int numberOfBytes)
{
    WriteRawBytes(reinterpret_cast<const unsigned char*>(input), numberOfBytes);
}
void CNetBitStream::Write(const ISyncStructure* syncStruct)
{
    if (syncStruct)
        syncStruct->Write(*this);
}

bool CNetBitStream::Read(unsigned char& output)
{
    return ReadRawBytes(&output, sizeof(output));
}
bool CNetBitStream::Read(char& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(unsigned short& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(short& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(unsigned int& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(int& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(float& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(double& output)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(&output), sizeof(output));
}
bool CNetBitStream::Read(char* output, int numberOfBytes)
{
    return ReadRawBytes(reinterpret_cast<unsigned char*>(output), numberOfBytes);
}
bool CNetBitStream::Read(ISyncStructure* syncStruct)
{
    return syncStruct ? syncStruct->Read(*this) : false;
}

template <typename U>
void CNetBitStream::WriteCompressedUnsigned(U value)
{
    static_assert(std::is_unsigned<U>::value, "WriteCompressedUnsigned requires an unsigned type");
    constexpr int kBytes = sizeof(U);
    unsigned char bytes[kBytes];
    std::memcpy(bytes, &value, kBytes);

    for (int i = kBytes - 1; i > 0; --i)
    {
        bool bNotRedundant = (bytes[i] != 0);
        WriteBit(bNotRedundant);
        if (bNotRedundant)
        {
            for (int j = i; j >= 0; --j)
                Write(bytes[j]);
            return;
        }
    }
    Write(bytes[0]);
}

template <typename U>
bool CNetBitStream::ReadCompressedUnsigned(U& value)
{
    static_assert(std::is_unsigned<U>::value, "ReadCompressedUnsigned requires an unsigned type");
    constexpr int kBytes = sizeof(U);
    unsigned char bytes[kBytes] = {};

    for (int i = kBytes - 1; i > 0; --i)
    {
        bool bNotRedundant = false;
        if (!ReadBit(bNotRedundant))
            return false;
        if (bNotRedundant)
        {
            for (int j = i; j >= 0; --j)
            {
                if (!Read(bytes[j]))
                    return false;
            }
            std::memcpy(&value, bytes, kBytes);
            return true;
        }
    }
    if (!Read(bytes[0]))
        return false;
    std::memcpy(&value, bytes, kBytes);
    return true;
}

template <typename S, typename U>
void CNetBitStream::WriteCompressedSigned(S value)
{
    static_assert(sizeof(S) == sizeof(U), "Signed/unsigned compression pair size mismatch");
    U uValue = static_cast<U>(value);
    U zigzag = static_cast<U>((uValue << 1) ^ static_cast<U>(value >> (sizeof(S) * 8 - 1)));
    WriteCompressedUnsigned<U>(zigzag);
}

template <typename S, typename U>
bool CNetBitStream::ReadCompressedSigned(S& value)
{
    static_assert(sizeof(S) == sizeof(U), "Signed/unsigned compression pair size mismatch");
    U zigzag = 0;
    if (!ReadCompressedUnsigned<U>(zigzag))
        return false;
    value = static_cast<S>((zigzag >> 1) ^ static_cast<U>(-static_cast<S>(zigzag & 1)));
    return true;
}

void CNetBitStream::WriteCompressed(const unsigned char& input)
{
    WriteCompressedUnsigned<unsigned char>(input);
}
void CNetBitStream::WriteCompressed(const char& input)
{
    WriteCompressedUnsigned<unsigned char>(static_cast<unsigned char>(input));
}
void CNetBitStream::WriteCompressed(const unsigned short& input)
{
    WriteCompressedUnsigned<unsigned short>(input);
}
void CNetBitStream::WriteCompressed(const short& input)
{
    WriteCompressedSigned<short, unsigned short>(input);
}
void CNetBitStream::WriteCompressed(const unsigned int& input)
{
    WriteCompressedUnsigned<unsigned int>(input);
}
void CNetBitStream::WriteCompressed(const int& input)
{
    WriteCompressedSigned<int, unsigned int>(input);
}
void CNetBitStream::WriteCompressed(const float& input)
{
    Write(input);
}
void CNetBitStream::WriteCompressed(const double& input)
{
    Write(input);
}

bool CNetBitStream::ReadCompressed(unsigned char& output)
{
    unsigned char temp = 0;
    if (!ReadCompressedUnsigned<unsigned char>(temp))
        return false;
    output = temp;
    return true;
}
bool CNetBitStream::ReadCompressed(char& output)
{
    unsigned char temp = 0;
    if (!ReadCompressedUnsigned<unsigned char>(temp))
        return false;
    output = static_cast<char>(temp);
    return true;
}
bool CNetBitStream::ReadCompressed(unsigned short& output)
{
    return ReadCompressedUnsigned<unsigned short>(output);
}
bool CNetBitStream::ReadCompressed(short& output)
{
    return ReadCompressedSigned<short, unsigned short>(output);
}
bool CNetBitStream::ReadCompressed(unsigned int& output)
{
    return ReadCompressedUnsigned<unsigned int>(output);
}
bool CNetBitStream::ReadCompressed(int& output)
{
    return ReadCompressedSigned<int, unsigned int>(output);
}
bool CNetBitStream::ReadCompressed(float& output)
{
    return Read(output);
}
bool CNetBitStream::ReadCompressed(double& output)
{
    return Read(output);
}

void CNetBitStream::WriteNormVector(float x, float y, float z)
{
    constexpr float kScale = 32767.0f;
    Write(static_cast<short>(std::lround(std::clamp(x, -1.0f, 1.0f) * kScale)));
    Write(static_cast<short>(std::lround(std::clamp(y, -1.0f, 1.0f) * kScale)));
    Write(static_cast<short>(std::lround(std::clamp(z, -1.0f, 1.0f) * kScale)));
}

bool CNetBitStream::ReadNormVector(float& x, float& y, float& z)
{
    short sx = 0, sy = 0, sz = 0;
    if (!Read(sx) || !Read(sy) || !Read(sz))
        return false;
    x = static_cast<float>(sx) / 32767.0f;
    y = static_cast<float>(sy) / 32767.0f;
    z = static_cast<float>(sz) / 32767.0f;
    return true;
}

void CNetBitStream::WriteVector(float x, float y, float z)
{
    float magnitude = std::sqrt(x * x + y * y + z * z);
    Write(magnitude);
    if (magnitude > 0.0f)
        WriteNormVector(x / magnitude, y / magnitude, z / magnitude);
    else
        WriteNormVector(0.0f, 0.0f, 0.0f);
}

bool CNetBitStream::ReadVector(float& x, float& y, float& z)
{
    float magnitude = 0.0f;
    if (!Read(magnitude))
        return false;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    if (!ReadNormVector(nx, ny, nz))
        return false;
    x = nx * magnitude;
    y = ny * magnitude;
    z = nz * magnitude;
    return true;
}

void CNetBitStream::WriteNormQuat(float w, float x, float y, float z)
{
    float        components[4] = {w, x, y, z};
    unsigned int largestIndex = 0;
    float        largestAbs = std::fabs(components[0]);
    for (unsigned int i = 1; i < 4; ++i)
    {
        if (std::fabs(components[i]) > largestAbs)
        {
            largestAbs = std::fabs(components[i]);
            largestIndex = i;
        }
    }

    float sign = (components[largestIndex] < 0.0f) ? -1.0f : 1.0f;

    WriteUInt(largestIndex, 2);

    constexpr float kScale = 32767.0f;
    for (unsigned int i = 0; i < 4; ++i)
    {
        if (i == largestIndex)
            continue;
        float value = std::clamp(components[i] * sign, -1.0f, 1.0f);
        Write(static_cast<short>(std::lround(value * kScale)));
    }
}

bool CNetBitStream::ReadNormQuat(float& w, float& x, float& y, float& z)
{
    unsigned int largestIndex = 0;
    if (!ReadUInt(largestIndex, 2) || largestIndex > 3)
        return false;

    float components[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float sumSquares = 0.0f;
    for (unsigned int i = 0; i < 4; ++i)
    {
        if (i == largestIndex)
            continue;
        short packed = 0;
        if (!Read(packed))
            return false;
        components[i] = static_cast<float>(packed) / 32767.0f;
        sumSquares += components[i] * components[i];
    }
    components[largestIndex] = std::sqrt(std::max(0.0f, 1.0f - sumSquares));

    w = components[0];
    x = components[1];
    y = components[2];
    z = components[3];
    return true;
}

void CNetBitStream::WriteOrthMatrix(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22)
{
    float trace = m00 + m11 + m22;
    float qw, qx, qy, qz;
    if (trace > 0.0f)
    {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        qw = 0.25f * s;
        qx = (m21 - m12) / s;
        qy = (m02 - m20) / s;
        qz = (m10 - m01) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        qw = (m21 - m12) / s;
        qx = 0.25f * s;
        qy = (m01 + m10) / s;
        qz = (m02 + m20) / s;
    }
    else if (m11 > m22)
    {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        qw = (m02 - m20) / s;
        qx = (m01 + m10) / s;
        qy = 0.25f * s;
        qz = (m12 + m21) / s;
    }
    else
    {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        qw = (m10 - m01) / s;
        qx = (m02 + m20) / s;
        qy = (m12 + m21) / s;
        qz = 0.25f * s;
    }

    if (qw < 0.0f)
    {
        qx = -qx;
        qy = -qy;
        qz = -qz;
    }

    constexpr float kScale = 32767.0f;
    Write(static_cast<short>(std::lround(std::clamp(qx, -1.0f, 1.0f) * kScale)));
    Write(static_cast<short>(std::lround(std::clamp(qy, -1.0f, 1.0f) * kScale)));
    Write(static_cast<short>(std::lround(std::clamp(qz, -1.0f, 1.0f) * kScale)));
}

bool CNetBitStream::ReadOrthMatrix(float& m00, float& m01, float& m02, float& m10, float& m11, float& m12, float& m20, float& m21, float& m22)
{
    short sx = 0, sy = 0, sz = 0;
    if (!Read(sx) || !Read(sy) || !Read(sz))
        return false;

    float qx = static_cast<float>(sx) / 32767.0f;
    float qy = static_cast<float>(sy) / 32767.0f;
    float qz = static_cast<float>(sz) / 32767.0f;
    float qw = std::sqrt(std::max(0.0f, 1.0f - qx * qx - qy * qy - qz * qz));

    m00 = 1.0f - 2.0f * (qy * qy + qz * qz);
    m01 = 2.0f * (qx * qy - qz * qw);
    m02 = 2.0f * (qx * qz + qy * qw);

    m10 = 2.0f * (qx * qy + qz * qw);
    m11 = 1.0f - 2.0f * (qx * qx + qz * qz);
    m12 = 2.0f * (qy * qz - qx * qw);

    m20 = 2.0f * (qx * qz - qy * qw);
    m21 = 2.0f * (qy * qz + qx * qw);
    m22 = 1.0f - 2.0f * (qx * qx + qy * qy);
    return true;
}

int CNetBitStream::GetNumberOfBitsUsed() const
{
    return static_cast<int>(m_uiWriteBitOffset);
}

int CNetBitStream::GetNumberOfBytesUsed() const
{
    return static_cast<int>((m_uiWriteBitOffset + 7) / 8);
}

int CNetBitStream::GetNumberOfUnreadBits() const
{
    return (m_uiReadBitOffset < m_uiWriteBitOffset) ? static_cast<int>(m_uiWriteBitOffset - m_uiReadBitOffset) : 0;
}

void CNetBitStream::AlignWriteToByteBoundary() const
{
    m_uiWriteBitOffset = (m_uiWriteBitOffset + 7) & ~static_cast<unsigned int>(7);
}

void CNetBitStream::AlignReadToByteBoundary() const
{
    m_uiReadBitOffset = (m_uiReadBitOffset + 7) & ~static_cast<unsigned int>(7);
}

unsigned char* CNetBitStream::GetData() const
{
    return const_cast<unsigned char*>(m_Data.data());
}

unsigned short CNetBitStream::Version() const
{
    return m_usVersion;
}
