// Copyright 2021 SiLeader and Cerussite.
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an “AS IS” BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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