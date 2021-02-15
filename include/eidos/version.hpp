//
// Created by MiyaMoto on 2021/02/13.
//

#pragma once

#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/utsname.h>
#endif

namespace eidos::version {

inline constexpr std::size_t kMajor = 1;
inline constexpr std::size_t kMinor = 0;
inline constexpr std::size_t kRevision = 0;

inline constexpr char kBuildDate[] = __DATE__;
inline constexpr char kBuildTime[] = __TIME__;

inline std::string Version() {
  std::stringstream ss;
  ss << kMajor << "." << kMinor << "." << kRevision << std::flush;
  return ss.str();
}

inline void VersionInfo(std::ostream& os) {
  os << "version: " << Version() << "\n"                       //
     << "built: " << kBuildDate << " " << kBuildTime << "\n";  //

  // build system
#if defined(__clang__)
  os << "build: clang " << __clang_version__ << "\n";
#elif defined(__GNUG__)
  os << "build: gcc " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\n";
#elif defined(_MSC_VER)
  os << "build: MSVC " << _MSC_VER << "\n";
#else
  os << "build: unknown\n";
#endif

  // operating system
#if defined(__unix__) || defined(__APPLE__)
  ::utsname un = {};
  ::uname(&un);
  os << "os: " << un.sysname << " " << un.release << " " << un.machine;
#elif defined(_MSV_VER)
  os << "os: Windows";
#else
  os << "os: Unknown";
#endif
}

}  // namespace eidos::version