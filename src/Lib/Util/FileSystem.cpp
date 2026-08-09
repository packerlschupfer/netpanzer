/*
Copyright (C) 2003 Matthias Braun <matze@braunis.de>
Copyright (C) 2024 The NetPanzer Team (https://github.com/netpanzer/)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TEST_LIB

#include "FileSystem.hpp"

#include <stdio.h>
#include <string.h>

#include "Exception.hpp"
#include "Log.hpp"
#include <physfs.h>

namespace filesystem {

const char *getErrStr(void) {
  return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

void initialize(const char* argv0, const char* application) {
  if (!PHYSFS_init(argv0))
    throw Exception("failure while initialising physfs: %s",
                    getErrStr());

  const char* tmp_basedir = PHYSFS_getBaseDir();
  char basedir[strlen(tmp_basedir) + sizeof "/data"];
  snprintf(basedir, sizeof basedir, "%s/data", tmp_basedir);
  const char* userdir = PHYSFS_getUserDir();
  const char* dirsep = PHYSFS_getDirSeparator();
  char* writedir = new char[strlen(userdir) + strlen(application) + 2];

  sprintf(writedir, "%s.%s", userdir, application);
  if (!PHYSFS_setWriteDir(writedir)) {
    // try to create the directory...
    char* mkdir = new char[strlen(application) + 2];
    sprintf(mkdir, ".%s", application);
    if (!PHYSFS_setWriteDir(userdir) || !PHYSFS_mkdir(mkdir)) {
      // Build the message while writedir is still alive. It used to be
      // deleted first and then formatted into the exception, reading freed
      // memory on the way out.
      Exception failure("failed creating configuration directory: '%s': %s",
                        writedir, getErrStr());
      delete[] writedir;
      delete[] mkdir;
      throw failure;
    }
    delete[] mkdir;

    if (!PHYSFS_setWriteDir(writedir)) {
      throw Exception("couldn't set configuration directory to '%s': %s",
                      writedir, getErrStr());
    }
  }

  PHYSFS_mount(writedir, NULL, 0);
  PHYSFS_mount(basedir, NULL, 1);

  /* Root out archives, and add them to search path... */
  const char* archiveExt = "zip";
  if (archiveExt != NULL) {
    char** rc = PHYSFS_enumerateFiles("/");
    char** i;
    size_t extlen = strlen(archiveExt);
    char* ext;

    for (i = rc; *i != NULL; i++) {
      size_t l = strlen(*i);
      if ((l > extlen) && ((*i)[l - extlen - 1] == '.')) {
        ext = (*i) + (l - extlen);
        if (strcasecmp(ext, archiveExt) == 0) {
          LOGGER.warning("==== Adding zip: '%s'", *i);
          const char* d = PHYSFS_getRealDir(*i);
          char* str = new char[strlen(d) + strlen(dirsep) + l + 1];
          sprintf(str, "%s%s%s", d, dirsep, *i);
          PHYSFS_mount(str, NULL, 0);
          delete[] str;
        } /* if */
      }   /* if */
    }     /* for */

    PHYSFS_freeList(rc);
  } /* if */

  PHYSFS_removeFromSearchPath(writedir);
  PHYSFS_mount(writedir, NULL, 0);
  delete[] writedir;
}

void shutdown() { PHYSFS_deinit(); }

void addToSearchPath(const char* directory, bool append) {
    if (!PHYSFS_mount(directory, NULL, append ? 1 : 0))
        throw Exception("Couldn't add '%s' to searchpath: %s", directory,
                        getErrStr());
}

void removeFromSearchPath(const char* directory) {
  if (!PHYSFS_removeFromSearchPath(directory))
    throw Exception("Couldn't remove '%s' from searchpath: %s", directory,
                    getErrStr());
}

const char* getRealDir(const char* filename) {
  return PHYSFS_getRealDir(filename);
}

std::string getRealName(const char* filename) {
  const char* dir = PHYSFS_getRealDir(filename);
  if (dir == 0) {
    throw Exception("no such path '%s'", filename);
  }
  std::string realname = dir;
  realname += PHYSFS_getDirSeparator();
  realname += filename;
  return realname;
}

std::string getRealWriteName(const char* filename) {
  const char* dir = PHYSFS_getWriteDir();
  if (dir == 0) {
    throw Exception("no writedir defined");
  }
  std::string realname = dir;
  realname += PHYSFS_getDirSeparator();
  realname += filename;
  return realname;
}

char** enumerateFiles(const char* directory) {
  return PHYSFS_enumerateFiles(directory);
}

void freeList(char** list) { PHYSFS_freeList(list); }

WriteFile* openWrite(const char* filename) {
  PHYSFS_file* file = PHYSFS_openWrite(filename);
  if (!file)
    throw Exception("couldn't open file '%s' for writing: %s", filename,
                    getErrStr());

  return new WriteFile(file);
}

ReadFile* openRead(const char* filename) {
  PHYSFS_file* file = PHYSFS_openRead(filename);
  if (!file) {
    int fn_length = strlen(filename) + 1;
    char fn[fn_length];
    memcpy(fn, filename, fn_length);  // includes \0;
    fn[sizeof fn - 1] = '\0';  // No, it doesn't

    char* folder_sep = strrchr(fn, '/');
    char* fn_start = fn;
    char** filelist = 0;
    if (folder_sep) {
      *folder_sep = 0;
      filelist = enumerateFiles(fn);
      *folder_sep = '/';
      fn_start = folder_sep + 1;
    } else {
      filelist = enumerateFiles(".");
      folder_sep = fn;
    }

    if (filelist) {
      for (char** curfile = filelist; *curfile != 0; curfile++) {
        if (strcasecmp(*curfile, fn_start) == 0) {
          memmove(fn_start, *curfile, (sizeof fn -1) - (folder_sep - fn));
          file = PHYSFS_openRead(fn);
          break;
        }
      }
      freeList(filelist);
    }

    if (!file) {
      throw Exception("couldn't open file '%s' for reading: %s", filename,
                      getErrStr());
    }
  }

  return new ReadFile(file);
}

WriteFile* openAppend(const char* filename) {
  PHYSFS_file* file = PHYSFS_openAppend(filename);
  if (!file)
    throw Exception("couldn't open file '%s' for writing(append): %s", filename,
                    getErrStr());

  return new WriteFile(file);
}

void mkdir(const char* directory) {
  if (!PHYSFS_mkdir(directory))
    throw Exception("couldn't create directory '%s': %s", directory,
                    getErrStr());
}

void remove(const char* filename) {
  if (!PHYSFS_delete(filename))
    throw Exception("couldn't remove file '%s': %s", filename,
                    getErrStr());
}

bool exists(const char* filename) { return PHYSFS_exists(filename); }

bool isDirectory(const char* filename) { return PHYSFS_isDirectory(filename); }

bool isSymbolicLink(const char* filename) {
  return PHYSFS_isSymbolicLink(filename);
}

int64_t getLastModTime(const char* filename) {
  int64_t modtime = PHYSFS_getLastModTime(filename);
  if (modtime < 0)
    throw Exception("couldn't determine modification time of '%s': %s",
                    filename, getErrStr());

  return modtime;
}

//---------------------------------------------------------------------------

File::File(PHYSFS_file* newfile) : file(newfile) {}

File::~File() { PHYSFS_close(file); }

bool File::eof() { return PHYSFS_eof(file); }

int64_t File::tell() { return PHYSFS_tell(file); }

void File::seek(uint64_t position) {
  if (!PHYSFS_seek(file, position))
    throw Exception("Seek operation failed: %s", getErrStr());
}

int64_t File::fileLength() { return PHYSFS_fileLength(file); }

void File::setBuffer(uint64_t bufsize) {
  if (!PHYSFS_setBuffer(file, bufsize))
    throw Exception("couldn't adjust buffer size: %s", getErrStr());
}

void File::flush() {
  PHYSFS_flush(file);  // no exception - what should an app do if flush fails?
}

//---------------------------------------------------------------------------

ReadFile::ReadFile(PHYSFS_file* file) : File(file) {}

size_t ReadFile::_read(void* buffer, size_t objsize, size_t objcount) {
  return (size_t)PHYSFS_read(file, buffer, objsize, objcount);
}

void ReadFile::read(void* buffer, size_t objsize, size_t objcount) {
  PHYSFS_sint64 objsread = PHYSFS_read(file, buffer, objsize, objcount);
  if (objsread != (PHYSFS_sint64)objcount)
    throw FileReadException(objsread, objcount, "eof while reading");
}

bool ReadFile::isEOF() { return PHYSFS_eof(file); }

SDL_IOStream* ReadFile::getSDLRWOps() {
  // SDL3 makes SDL_IOStream opaque: instead of filling in a struct's function
  // pointers, the callbacks go in an SDL_IOStreamInterface and SDL_OpenIO
  // builds the stream around them, carrying our "this" as userdata.
  SDL_IOStreamInterface iface;
  SDL_INIT_INTERFACE(&iface);
  iface.size = RWOps_Size;
  iface.seek = RWOps_Seek;
  iface.read = RWOps_Read;
  iface.close = RWOps_Close;

  return SDL_OpenIO(&iface, this);
}

Sint64 ReadFile::RWOps_Size(void* userdata) {
  ReadFile* file = (ReadFile*)userdata;
  try {
    return file->fileLength();
  } catch (...) {
    return -1;
  }
}

size_t ReadFile::RWOps_Read(void* userdata, void* ptr, size_t size,
                            SDL_IOStatus* status) {
  ReadFile* file = (ReadFile*)userdata;
  // SDL3 asks for a byte count rather than a count of objects, and wants
  // short reads reported through status rather than inferred by the caller.
  try {
    file->read(ptr, 1, size);
  } catch (FileReadException& e) {
    *status = SDL_IO_STATUS_EOF;
    return e.getReadCount();
  } catch (...) {
    *status = SDL_IO_STATUS_ERROR;
    return 0;
  }

  return size;
}

Sint64 ReadFile::RWOps_Seek(void* userdata, Sint64 offset,
                            SDL_IOWhence whence) {
  ReadFile* file = (ReadFile*)userdata;
  try {  // catch exceptions
    switch (whence) {
      case SDL_IO_SEEK_SET:
        file->seek(offset);
        break;
      case SDL_IO_SEEK_CUR:
        file->seek(file->tell() + offset);
        break;
      case SDL_IO_SEEK_END:
        file->seek(file->fileLength() + offset);
        break;
    }
  } catch (...) {
    LOG(("Unexpected exception while seeking in file."));
    return -1;
  }

  return file->tell();
}

bool ReadFile::RWOps_Close(void* userdata) {
  ReadFile* file = (ReadFile*)userdata;
  delete file;
  return true;
}

Sint8 ReadFile::read8() {
  Sint8 val;
  if (PHYSFS_read(file, &val, 1, 1) != 1)
    throw Exception("read error: %s", getErrStr());
  return val;
}

Sint16 ReadFile::readSLE16() {
  Sint16 val;
  if (!PHYSFS_readSLE16(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Uint16 ReadFile::readULE16() {
  Uint16 val;
  if (!PHYSFS_readULE16(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Sint16 ReadFile::readSBE16() {
  Sint16 val;
  if (!PHYSFS_readSBE16(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Uint16 ReadFile::readUBE16() {
  Uint16 val;
  if (!PHYSFS_readUBE16(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Sint32 ReadFile::readSLE32() {
  Sint32 val;
  if (!PHYSFS_readSLE32(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Uint32 ReadFile::readULE32() {
  Uint32 val;
  if (!PHYSFS_readULE32(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Sint32 ReadFile::readSBE32() {
  Sint32 val;
  if (!PHYSFS_readSBE32(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

Uint32 ReadFile::readUBE32() {
  Uint32 val;
  if (!PHYSFS_readUBE32(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

int64_t ReadFile::readSLE64() {
  PHYSFS_sint64 val;
  if (!PHYSFS_readSLE64(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

uint64_t ReadFile::readULE64() {
  PHYSFS_uint64 val;
  if (!PHYSFS_readULE64(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

int64_t ReadFile::readSBE64() {
  PHYSFS_sint64 val;
  if (!PHYSFS_readSBE64(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

uint64_t ReadFile::readUBE64() {
  PHYSFS_uint64 val;
  if (!PHYSFS_readUBE64(file, &val))
    throw Exception("read error: %s", getErrStr());
  return val;
}

void ReadFile::readLine(std::string& buffer) {
  if (isEOF()) throw Exception("end of file while reading line");

  char c;
  buffer = "";

  while (!isEOF() && (c = read8()) != '\n') {
    buffer += c;
  }
}

//---------------------------------------------------------------------------

WriteFile::WriteFile(PHYSFS_file* file) : File(file) {}

void WriteFile::write(const void* buffer, size_t objsize, size_t objcount) {
  PHYSFS_sint64 objswritten = PHYSFS_write(file, buffer, objsize, objcount);
  if (objswritten != (PHYSFS_sint64)objcount)
    throw FileReadException(objswritten, objcount,
                            "write not possible (disk full)?");
}

void WriteFile::write8(Sint8 val) {
    if (PHYSFS_writeBytes(file, &val, 1) != 1)
        throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSLE16(Sint16 val) {
  if (!PHYSFS_writeSLE16(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeULE16(Uint16 val) {
  if (!PHYSFS_writeULE16(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSBE16(Sint16 val) {
  if (!PHYSFS_writeSBE16(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeUBE16(Uint16 val) {
  if (!PHYSFS_writeUBE16(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSLE32(Sint32 val) {
  if (!PHYSFS_writeSLE32(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeULE32(Uint32 val) {
  if (!PHYSFS_writeULE32(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSBE32(Sint32 val) {
  if (!PHYSFS_writeSBE32(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeUBE32(Uint32 val) {
  if (!PHYSFS_writeUBE32(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSLE64(int64_t val) {
  if (!PHYSFS_writeSLE64(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeULE64(uint64_t val) {
  if (!PHYSFS_writeULE64(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeSBE64(int64_t val) {
  if (!PHYSFS_writeSBE64(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeUBE64(uint64_t val) {
  if (!PHYSFS_writeUBE64(file, val))
    throw Exception("couldn't write: %s", getErrStr());
}

void WriteFile::writeLine(const std::string& buffer) {
  write(buffer.c_str(), buffer.size(), 1);
  write8('\n');
}

//---------------------------------------------------------------------------

FileReadException::FileReadException(size_t newobjread, size_t newobjrequested,
                                     const std::string& msg) throw()
    : std::range_error(msg),
      objread(newobjread),
      objrequested(newobjrequested) {}

FileReadException::~FileReadException() throw() {}

}  // namespace filesystem

#else

#include "test.hpp"

void testEnumerateFilesOpenMapsDir(void) {
  filesystem::addToSearchPath(Package::getDataDir().c_str(), true);

  const char mapsPath[] = "maps/";

  char **list = filesystem::enumerateFiles(mapsPath);
  int n = 0, nTotal = 0;
  for (char **i = list; *i != NULL; i++) {
    fprintf(stderr, "%d / i: %s\n", nTotal++, *i);
    if (strcmp(*i, "Two clans.npm") == 0)
      n++;
    else if (strcmp(*i, "Two clans.opt") == 0)
      n++;
    else if (strcmp(*i, "Two clans.spn") == 0)
      n++;
    else if (strcmp(*i, "Open War.npm") == 0)
      n++;
  }
  filesystem::freeList(list);
  assert(n == 4);

  // This will need to be changed when more maps are added
  fprintf(stderr, "nTotal: %d\n", nTotal);
  assert(nTotal < 120 && nTotal > 76);

  return;
}

int main(int argc, char *argv[]) {
  (void)argc;

  filesystem::initialize(argv[0], "test_FileSystem");
  Package::assignDataDir();

  testEnumerateFilesOpenMapsDir();
  return 0;
}

#endif
