#ifndef EVENTLY_REFLECTION_ERROR_H
#define EVENTLY_REFLECTION_ERROR_H
#pragma once

#include <stdexcept>
#include <string>

namespace Evently
{
    enum class ReflectionErrorCode
    {
        InvalidArgument,
        ClassNotFound,
        ClassNameConflict,
        FieldNotFound,
        FieldNotWritable,
        FieldTypeMismatch,
        MethodNotFound,
        InvalidInstance,
        RegistryFrozen,
        ArgumentCountMismatch,
        ArgumentTypeMismatch,
        DuplicateRegistration,
        AmbiguousMethod,
        ConstructorNotFound,
        AmbiguousConstructor
    };

    class ReflectionError : public std::runtime_error
    {
    public:
        ReflectionError(ReflectionErrorCode code, const std::string &message)
            : std::runtime_error(message), code_(code) {}

        ReflectionErrorCode code() const noexcept
        {
            return code_;
        }

    private:
        ReflectionErrorCode code_;
    };
}

#endif // EVENTLY_REFLECTION_ERROR_H
