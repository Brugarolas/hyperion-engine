#ifndef HYPERION_V2_LIB_BYTE_BUFFER_HPP
#define HYPERION_V2_LIB_BYTE_BUFFER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/Span.hpp>
#include <core/lib/Memory.hpp>
#include <HashCode.hpp>
#include <Constants.hpp>

#include <Types.hpp>

namespace hyperion {

using ByteArray = Array<ubyte>;

using ByteView = Span<ubyte>;
using ConstByteView = Span<const ubyte>;

class ByteBuffer
{
    using InternalArray = Array<ubyte, 1024u>;

public:
    ByteBuffer() = default;

    ByteBuffer(SizeType count)
    {
        SetSize(count);
    }

    ByteBuffer(SizeType count, const void *data)
    {
        SetData(count, data);
    }

    explicit ByteBuffer(const ByteView &view)
    {
        SetData(view.Size(), view.Data());
    }

    explicit ByteBuffer(const ConstByteView &view)
    {
        SetData(view.Size(), view.Data());
    }

    ByteBuffer(const ByteBuffer &other)
        : m_internal(other.m_internal)
    {
    }

    ByteBuffer &operator=(const ByteBuffer &other)
    {
        if (&other == this) {
            return *this;
        }

        m_internal = other.m_internal;

        return *this;
    }

    ByteBuffer(ByteBuffer &&other) noexcept
        : m_internal(std::move(other.m_internal))
    {
    }

    ByteBuffer &operator=(ByteBuffer &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        m_internal = std::move(other.m_internal);

        return *this;
    }

    ~ByteBuffer() = default;
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return true; }

    void Write(SizeType count, SizeType offset, const void *data)
    {
        if (count == 0) {
            return;
        }

        AssertThrow(offset + count <= Size());

        Memory::MemCpy(m_internal.Data() + offset, data, count);
    }

    /**
     * \brief Returns a reference to the ByteBuffer's internal array.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    InternalArray &GetInternalArray()
        { return m_internal; }

    /**
     * \brief Returns a const reference to the ByteBuffer's internal array.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const InternalArray &GetInternalArray() const
        { return const_cast<ByteBuffer *>(this)->GetInternalArray(); }

    /**
     * \brief Returns a copy of the ByteBuffer's data.
     */
    ByteArray ToByteArray() const
    {
        const auto size = GetInternalArray().Size();
        const auto *data = GetInternalArray().Data();

        ByteArray byte_array;
        byte_array.Resize(size);
        Memory::MemCpy(byte_array.Data(), data, size);

        return byte_array;
    }

    /**
     * \brief Returns a ByteView of the ByteBuffer's data.
     */
    ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (size > Size()) {
            size = Size();
        }

        return ByteView(Data() + offset, size);
    }

    /**
     * \brief Returns a ConstByteView of the ByteBuffer's data.
     */
    ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (size > Size()) {
            size = Size();
        }

        return ConstByteView(Data() + offset, size);
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    ubyte *Data()
        { return GetInternalArray().Data(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const ubyte *Data() const
        { return GetInternalArray().Data(); }

    /**
     * \brief Updates the ByteBuffer's data with the given data.
     */
    void SetData(SizeType count, const void *data)
    {
        m_internal.Resize(count);

        if (count == 0) {
            return;
        }

        Memory::MemCpy(m_internal.Data(), data, count);
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Size() const
        { return GetInternalArray().Size(); }
    
    HYP_FORCE_INLINE
    void SetSize(SizeType count)
    {
        if (count == Size()) {
            return;
        }

        m_internal.Resize(count);
    }

    /**
     * \brief Reads a value from the ByteBuffer at the given offset.
     */
    bool Read(SizeType offset, SizeType count, ubyte *out_values) const
    {
        AssertThrow(out_values != nullptr);

        const SizeType size = Size();

        if (offset >= size || offset + count > size) {
            return false;
        }

        const ubyte *data = Data();

        for (SizeType index = offset; index < offset + count; index++) {
            out_values[index - offset] = data[index];
        }

        return true;
    }

    /**
     * \brief Reads a value from the ByteBuffer at the given offset.
     */
    template <class T>
    bool Read(SizeType offset, T *out) const
    {
        static_assert(IsPODType<T>, "Must be POD type");

        AssertThrow(out != nullptr);

        constexpr SizeType count = sizeof(T);
        const SizeType size = Size();

        if (offset >= size || offset + count > size) {
            return false;
        }

        const ubyte *data = Data();

        alignas(T) ubyte bytes[sizeof(T)];

        for (SizeType index = offset; index < offset + count; index++) {
            bytes[index - offset] = data[index];
        }

        Memory::MemCpy(out, bytes, sizeof(T));

        return true;
    }

    /**
     * \brief Returns true if the ByteBuffer has any elements.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Any() const
        { return Size() != 0; }

    /**
     * \brief Returns true if the ByteBuffer has no elements.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Empty() const
        { return Size() == 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ubyte &operator[](SizeType index)
        { return GetInternalArray()[index]; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const ubyte &operator[](SizeType index) const
        { return GetInternalArray()[index]; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const ByteBuffer &other) const
        { return m_internal == other.m_internal; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const ByteBuffer &other) const
        { return m_internal != other.m_internal; }

    /**
     * \brief Returns a copy of the ByteBuffer.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    ByteBuffer Copy() const
        { return ByteBuffer(Size(), Data()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return GetInternalArray().GetHashCode(); }

private:
    InternalArray   m_internal;
};

} // namespace hyperion

#endif