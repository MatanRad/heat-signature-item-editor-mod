#pragma once

#include <cstdint>
#include <string>

#include "ModInterface.h"

class GameMakerPropertyAccess
{
public:
    static constexpr int NoArrayIndex = static_cast<int>(0x80000000u);

    explicit GameMakerPropertyAccess(const HS_ModApi* api) : m_api(api) {}

    /// Reads REAL, BOOL, INT32, or INT64 values as a double.
    bool ReadNumber(
        int handle,
        const char* name,
        double& value,
        std::string& error,
        int arrayIndex = NoArrayIndex) const;

    /// Reads an engine string into caller-owned storage.
    bool ReadString(
        int handle,
        const char* name,
        std::string& value,
        std::string& error,
        int arrayIndex = NoArrayIndex) const;

    /// Writes a GameMaker REAL value.
    bool WriteReal(
        int handle,
        const char* name,
        double value,
        std::string& error,
        int arrayIndex = NoArrayIndex) const;

    /// Writes a GameMaker INT64 value without changing its runtime kind.
    bool WriteInt64(
        int handle,
        const char* name,
        int64_t value,
        std::string& error,
        int arrayIndex = NoArrayIndex) const;

    /// Creates and assigns an engine-owned GameMaker string.
    bool WriteString(
        int handle,
        const char* name,
        const std::string& value,
        std::string& error,
        int arrayIndex = NoArrayIndex) const;

private:
    bool ReadValue(
        int handle,
        const char* name,
        RValue& value,
        std::string& error,
        int arrayIndex) const;

    bool WriteValue(
        int handle,
        const char* name,
        RValue& value,
        std::string& error,
        int arrayIndex) const;

    const HS_ModApi* m_api;
};
