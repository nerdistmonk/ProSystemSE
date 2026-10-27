#include "Archive.h"
#include <limits.h>

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

uint archive_GetUncompressedFileSize(std::string filename) {
    if (filename.empty()) {
        return 0;
    }

    unzFile file = unzOpen(filename.c_str());
    if (file == NULL) {
        return 0;
    }

    int result = unzGoToFirstFile(file);
    if (result != UNZ_OK) {
        unzClose(file);
        return 0;
    }

    unz_file_info_s zipInfo = {0};
    char buffer[_MAX_PATH] = {0};
    result = unzGetCurrentFileInfo(file, &zipInfo, buffer, _MAX_PATH, NULL, 0, NULL, 0);
    if (result != UNZ_OK) {
        unzClose(file);
        return 0;
    }

    uint size = zipInfo.uncompressed_size;
    unzClose(file);
    return size;
}

bool archive_Uncompress(std::string filename, byte* data, uint size) {
    if (filename.empty() || data == NULL) {
        return false;
    }

    unzFile file = unzOpen(filename.c_str());
    if (file == NULL) {
        return false;
    }

    int result = unzGoToFirstFile(file);
    if (result != UNZ_OK) {
        unzClose(file);
        return false;
    }

    result = unzOpenCurrentFile(file);
    if (result != UNZ_OK) {
        unzClose(file);
        return false;
    }

    result = unzReadCurrentFile(file, data, size);
    if (result != static_cast<int>(size)) {
        unzCloseCurrentFile(file);
        unzClose(file);
        return false;
    }

    unzCloseCurrentFile(file);
    unzClose(file);
    return true;
}

bool archive_Compress(std::string zipFilename, std::string filename, const byte* data, uint size) {
    if (zipFilename.empty() || filename.empty() || data == NULL) {
        return false;
    }

    zipFile file = zipOpen(zipFilename.c_str(), APPEND_STATUS_CREATE);
    if (file == NULL) {
        return false;
    }

    zip_fileinfo fileInfo = {0};
    fileInfo.dosDate = 1;

    int result = zipOpenNewFileInZip(file, filename.c_str(), &fileInfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_COMPRESSION);
    if (result != ZIP_OK) {
        zipClose(file, "Failed to compress.");
        return false;
    }

    result = zipWriteInFileInZip(file, data, size);
    if (result != ZIP_OK) {
        zipCloseFileInZip(file);
        zipClose(file, "Failed to compress.");
        return false;
    }

    zipCloseFileInZip(file);
    zipClose(file, "Comment");
    return true;
}
