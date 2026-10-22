#ifndef EVENTLY_EXPORT_H
#define EVENTLY_EXPORT_H
#pragma once

#if defined(_WIN32) && defined(EVENTLY_SHARED)
#if defined(EVENTLY_BUILDING_LIBRARY)
#define EVENTLY_API __declspec(dllexport)
#else
#define EVENTLY_API __declspec(dllimport)
#endif
#else
#define EVENTLY_API
#endif

#endif // EVENTLY_EXPORT_H
