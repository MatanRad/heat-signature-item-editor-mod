#include "GameMakerPropertyAccess.h"

namespace
{
    constexpr uint32_t kDoubleBackedNumericKind = 13;

    std::string PropertyError(
        const char* operation,
        const char* name)
    {
        return std::string("Could not ") + operation + ' ' + name;
    }
}

bool GameMakerPropertyAccess::ReadValue(
    int handle,
    const char* name,
    RValue& value,
    std::string& error,
    int arrayIndex) const
{
    value = {};
    if (m_api &&
        m_api->GetVarByName(handle, name, arrayIndex, &value) != 0)
    {
        return true;
    }

    error = PropertyError("read", name);
    return false;
}

bool GameMakerPropertyAccess::WriteValue(
    int handle,
    const char* name,
    RValue& value,
    std::string& error,
    int arrayIndex) const
{
    if (m_api &&
        m_api->SetVarByName(handle, name, arrayIndex, &value) != 0)
    {
        return true;
    }

    error = PropertyError("write", name);
    return false;
}

bool GameMakerPropertyAccess::ReadNumber(
    int handle,
    const char* name,
    double& value,
    std::string& error,
    int arrayIndex) const
{
    RValue raw{};
    if (!ReadValue(handle, name, raw, error, arrayIndex))
        return false;

    const RValueKind kind = GetRValueKind(raw);
    if (kind == RValueKind::Real ||
        static_cast<uint32_t>(kind) == kDoubleBackedNumericKind)
    {
        value = raw.real;
        return true;
    }

    switch (kind)
    {
    case RValueKind::Int32:
        value = static_cast<double>(raw.i32);
        return true;
    case RValueKind::Int64:
        value = static_cast<double>(raw.i64);
        return true;
    default:
        error = std::string(name) + " is not numeric";
        return false;
    }
}

bool GameMakerPropertyAccess::ReadString(
    int handle,
    const char* name,
    std::string& value,
    std::string& error,
    int arrayIndex) const
{
    RValue raw{};
    if (!ReadValue(handle, name, raw, error, arrayIndex))
        return false;

    if (GetRValueKind(raw) != RValueKind::String ||
        !raw.str ||
        !raw.str->text)
    {
        error = std::string(name) + " is not a string";
        return false;
    }

    value.assign(raw.str->text, raw.str->length);
    return true;
}

bool GameMakerPropertyAccess::WriteReal(
    int handle,
    const char* name,
    double value,
    std::string& error,
    int arrayIndex) const
{
    RValue raw{};
    raw.real = value;
    raw.type = static_cast<uint32_t>(RValueKind::Real);
    return WriteValue(handle, name, raw, error, arrayIndex);
}

bool GameMakerPropertyAccess::WriteInt64(
    int handle,
    const char* name,
    int64_t value,
    std::string& error,
    int arrayIndex) const
{
    RValue raw{};
    raw.i64 = value;
    raw.type = static_cast<uint32_t>(RValueKind::Int64);
    return WriteValue(handle, name, raw, error, arrayIndex);
}

bool GameMakerPropertyAccess::WriteString(
    int handle,
    const char* name,
    const std::string& value,
    std::string& error,
    int arrayIndex) const
{
    if (!m_api)
    {
        error = PropertyError("write", name);
        return false;
    }

    RValue raw{};
    if (m_api->SetString(&raw, value.c_str()) == 0)
    {
        error = std::string("Could not create string for ") + name;
        return false;
    }
    return WriteValue(handle, name, raw, error, arrayIndex);
}
