/*
 * Copyright (C) 2017  Andrew Gunnerson <andrewgunnerson@gmail.com>
 *
 * This file is part of DualBootPatcher
 *
 * DualBootPatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DualBootPatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DualBootPatcher.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mbcommon/file_util.h"

#include <algorithm>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "mbcommon/libc/string.h"

#define DEFAULT_BUFFER_SIZE             (8 * 1024 * 1024)

/*!
 * \file mbcommon/file_util.h
 * \brief Useful utility functions for File API
 */

/*!
 * \typedef FileSearchResultCallback
 *
 * \note The file position must not change after a successful return of this
 *       callback. If file operations need to be performed, save the file
 *       position beforehand with File::seek() and restore it afterwards. Note
 *       that the file position is unlikely to match \p offset.
 *
 * \param file File handle
 * \param offset Offset of match
 * \param userdata User callback data
 *
 * \return
 *   * Return #FileStatus::OK if the search can continue
 *   * Return #FileStatus::WARN if the search should stop
 *   * Return \<= #FileStatus::FAILED if the search should fail
 */

namespace mb
{

/*!
 * \brief Read from a File handle.
 *
 * This function differs from File::read() in that it will call File::read()
 * repeatedly until the buffer is filled or EOF is reached. If File::read()
 * returns #FileStatus::RETRY, the read operation will be automatically
 * reattempted. Thus, this function will never return #FileStatus::RETRY.
 *
 * \note \p bytes_read is updated with the number of bytes successfully read
 *       even when this function fails. Take this into account if reattempting
 *       the read operation.
 *
 * \param[in] file File handle
 * \param[out] buf Buffer to read into
 * \param[in] size Buffer size
 * \param[out] bytes_read Output number of bytes that were read. A short read
 *                        indicates end of file. This parameter cannot be NULL.
 *
 * \return
 *   * #FileStatus::OK if some bytes are read or EOF is reached
 *   * #FileStatus::UNSUPPORTED if the handle source does not support reading
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_read_fully(File &file, void *buf, size_t size,
                           size_t *bytes_read)
{
    size_t n;
    FileStatus ret;

    *bytes_read = 0;

    while (*bytes_read < size) {
        ret = file.read(static_cast<char *>(buf) + *bytes_read,
                        size - *bytes_read, &n);
        if (ret == FileStatus::RETRY) {
            continue;
        } else if (ret < FileStatus::OK) {
            return ret;
        } else if (n == 0) {
            break;
        }

        *bytes_read += n;
    }

    return FileStatus::OK;
}

/*!
 * \brief Write to a File handle.
 *
 * This function differs from File::write() in that it will call
 * File::write() repeatedly until the buffer is filled or EOF is reached. If
 * File::write() returns #FileStatus::RETRY, the write operation will be
 * automatically reattempted. Thus, this function will never return
 * #FileStatus::RETRY.
 *
 * \note \p bytes_written is updated with the number of bytes successfully
 *       written even when this function fails. Take this into account if
 *       reattempting the write operation.
 *
 * \param[in] file File handle
 * \param[in] buf Buffer to write from
 * \param[in] size Buffer size
 * \param[out] bytes_written Output number of bytes that were written. This
 *                           parameter cannot be NULL.
 *
 * \return
 *   * #FileStatus::OK if some bytes are written
 *   * #FileStatus::UNSUPPORTED if the handle source does not support writing
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_write_fully(File &file, const void *buf, size_t size,
                            size_t *bytes_written)
{
    size_t n;
    FileStatus ret;

    *bytes_written = 0;

    while (*bytes_written < size) {
        ret = file.write(static_cast<const char *>(buf) + *bytes_written,
                         size - *bytes_written, &n);
        if (ret == FileStatus::RETRY) {
            continue;
        } else if (ret < FileStatus::OK) {
            return ret;
        } else if (n == 0) {
            break;
        }

        *bytes_written += n;
    }

    return FileStatus::OK;
}

/*!
 * \brief Read from a File handle and discard the data.
 *
 * This function will repeatedly call File::read() and discard any data that
 * is read. If File::read() returns #FileStatus::RETRY, the read operation will
 * be automatically reattempted. Thus, this function will never return
 * #FileStatus::RETRY.
 *
 * \note \p bytes_discarded is updated with the number of bytes successfully
 *       read even when this function fails. Take this into account if
 *       reattempting the read operation.
 *
 * \param[in] file File handle
 * \param[in] size Number of bytes to discard
 * \param[out] bytes_discarded Output number of bytes that were read. A short
 *                             read indicates end of file. This parameter cannot
 *                             be NULL.
 *
 * \return
 *   * #FileStatus::OK if some bytes are discarded or EOF is reached
 *   * #FileStatus::UNSUPPORTED if the handle source does not support reading
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_read_discard(File &file, uint64_t size,
                             uint64_t *bytes_discarded)
{
    char buf[10240];
    size_t n;
    FileStatus ret;

    *bytes_discarded = 0;

    while (*bytes_discarded < size) {
        ret = file.read(buf, std::min<uint64_t>(size, sizeof(buf)), &n);
        if (ret == FileStatus::RETRY) {
            continue;
        } else if (ret < FileStatus::OK) {
            return ret;
        } else if (n == 0) {
            break;
        }

        *bytes_discarded += n;
    }

    return FileStatus::OK;
}

/*!
 * \typedef FileSearchResultCallback
 *
 * \brief Search result callback for file_search()
 *
 * \sa file_search()
 *
 * \param file File handle
 * \param userdata User callback data
 * \param offset File offset of search result
 *
 * \return
 *   * #FileStatus::OK to continue search
 *   * #FileStatus::WARN to stop search, but have file_search() report a
 *     successful result
 *   * \<= #FileStatus::FAILED if an error occurs
 */

/*!
 * \brief Search file for binary sequence
 *
 * If \p buf_size is non-zero, a buffer of size \p buf_size will be allocated.
 * If it is less than or equal to \p pattern_size, then the function will fail
 * and set `errno` to `EINVAL`. If \p buf_size is zero, then the larger of 8 MiB
 * and 2 * \p pattern_size will be used. In the rare case that
 * 2 * \p pattern_size would exceed the maximum value of a `size_t`, `SIZE_MAX`
 * will be used.
 *
 * If \p file does not support seeking, then the file position must be set to
 * the beginning of the file before calling this function. Instead of seeking,
 * the function will read and discard any data before \p start.
 *
 * \note We do not do overlapping searches. For example, if a file's contents
 *       is "ababababab" and the search pattern is "abab", the resulting offsets
 *       will be (0 and 4), *not* (0, 2, 4, 6). In other words, the next search
 *       begins at the end of the curent search.
 *
 * \note The file position after this function returns is undefined. Be sure to
 *       seek to a known location before attempting further read or write
 *       operations.
 *
 * \param file File handle
 * \param start Start offset or negative number for beginning of file
 * \param end End offset or negative number for end of file
 * \param bsize Buffer size or 0 to automatically choose a size
 * \param pattern Pattern to search
 * \param pattern_size Size of pattern
 * \param max_matches Maximum number of matches or -1 to find all matches
 * \param result_cb Callback to invoke upon finding a match
 * \param userdata User callback data
 *
 * \return
 *   * #FileStatus::OK if the search completes successfully
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_search(File &file, int64_t start, int64_t end,
                       size_t bsize, const void *pattern,
                       size_t pattern_size, int64_t max_matches,
                       FileSearchResultCallback result_cb,
                       void *userdata)
{
    FileStatus ret = FileStatus::OK;
    char *buf = nullptr;
    size_t buf_size;
    char *ptr;
    size_t ptr_remain;
    char *match;
    size_t match_remain;
    uint64_t offset;
    size_t n;

    // Check boundaries
    if (start >= 0 && end >= 0 && end < start) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "End offset < start offset");
        ret = FileStatus::FAILED;
        goto done;
    }

    // Trivial case
    if (max_matches == 0 || pattern_size == 0) {
        goto done;
    }

    // Compute buffer size
    if (bsize != 0) {
        buf_size = bsize;
    } else {
        buf_size = DEFAULT_BUFFER_SIZE;

        if (pattern_size > SIZE_MAX / 2) {
            buf_size = SIZE_MAX;
        } else {
            buf_size = std::max(buf_size, pattern_size * 2);
        }
    }

    // Ensure buffer is large enough
    if (buf_size < pattern_size) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Buffer size cannot be less than pattern size");
        ret = FileStatus::FAILED;
        goto done;
    }

    buf = static_cast<char *>(malloc(buf_size));
    if (!buf) {
        file.set_error(-errno, "Failed to allocate buffer: %s",
                       strerror(errno));
        ret = FileStatus::FAILED;
        goto done;
    }

    if (start >= 0) {
        offset = start;
    } else {
        offset = 0;
    }

    // Seek to starting point
    ret = file.seek(offset, SEEK_SET, nullptr);
    if (ret == FileStatus::UNSUPPORTED) {
        uint64_t discarded;
        ret = file_read_discard(file, offset, &discarded);
        if (ret < FileStatus::OK) {
            goto done;
        } else if (discarded != offset) {
            file.set_error(FileError::INVALID_ARGUMENT,
                           "Reached EOF before starting offset");
            ret = FileStatus::FATAL;
            goto done;
        }
    } else if (ret < FileStatus::OK) {
        goto done;
    }

    // Initially read to beginning of buffer
    ptr = buf;
    ptr_remain = buf_size;

    while (true) {
        ret = file_read_fully(file, ptr, ptr_remain, &n);
        if (ret < FileStatus::OK) {
            goto done;
        }

        // Number of available bytes in buf
        n += ptr - buf;

        if (n < pattern_size) {
            // Reached EOF
            goto done;
        } else if (end >= 0 && offset >= static_cast<uint64_t>(end)) {
            // Artificial EOF
            goto done;
        }

        // Ensure that offset + n (and consequently, offset + diff) cannot
        // overflow
        if (n > UINT64_MAX - offset) {
            file.set_error(FileError::INTERNAL_ERROR,
                           "Read overflows offset value");
            ret = FileStatus::FAILED;
            goto done;
        }

        // Search from beginning of buffer
        match = buf;
        match_remain = n;

        while ((match = static_cast<char *>(
                mb_memmem(match, match_remain, pattern, pattern_size)))) {
            // Stop if match falls outside of ending boundary
            if (end >= 0 && offset + match - buf + pattern_size
                    > static_cast<uint64_t>(end)) {
                ret = FileStatus::OK;
                goto done;
            }

            // Invoke callback
            ret = result_cb(file, userdata, offset + match - buf);
            if (ret == FileStatus::WARN) {
                // Stop searching early
                ret = FileStatus::OK;
                goto done;
            } else if (ret < FileStatus::OK) {
                goto done;
            }

            if (max_matches > 0) {
                --max_matches;
                if (max_matches == 0) {
                    goto done;
                }
            }

            // We don't do overlapping searches
            if (match_remain >= pattern_size) {
                match += pattern_size;
                match_remain = n - (match - buf);
            } else {
                break;
            }
        }

        // Up to pattern_size - 1 bytes may still match, so move those to the
        // beginning. We will move fewer than pattern_size - 1 bytes if there
        // was a match close to the end.
        size_t to_move = std::min(match_remain, pattern_size - 1);
        memmove(buf, buf + n - to_move, to_move);
        ptr = buf + to_move;
        ptr_remain = buf_size - to_move;
        offset += n - to_move;
    }

done:
    free(buf);
    return ret;
}

/*!
 * \brief Move data in file
 *
 * This function is equivalent to `memmove()`, except it operates on a File
 * handle. The source and destination regions can overlap. In the degenerate
 * case where \p src == \p dest or \p size == 0, no operation will be performed,
 * but the function will return #FileStatus::OK and set \p size_moved
 * accordingly.
 *
 * \note This function is very seek-heavy and may be slow if the handle cannot
 *       seek efficiently. It will perform two seeks per loop interation. Each
 *       iteration moves up to 10240 bytes.
 *
 * \note If \p *size_moved is less than \p size, then the *first* \p *size_moved
 *       bytes have been copied from offset \p src to offset \p dest. This is
 *       true even if \p src \< \p dest, resulting in a backwards copy.
 *
 * \param[in] file File handle
 * \param[in] src Source offset
 * \param[in] dest Destination offset
 * \param[in] size Size of data to move
 * \param[out] size_moved Pointer to store size of data that is moved
 *
 * \return
 *   * #FileStatus::OK if the data is successfully moved
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_move(File &file, uint64_t src, uint64_t dest,
                     uint64_t size, uint64_t *size_moved)
{
    char buf[10240];
    size_t n_read;
    size_t n_written;
    FileStatus ret;

    // Check if we need to do anything
    if (src == dest || size == 0) {
        *size_moved = size;
        return FileStatus::OK;
    }

    if (src > UINT64_MAX - size || dest > UINT64_MAX - size) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Offset + size overflows integer");
        return FileStatus::FAILED;
    }

    *size_moved = 0;

    if (dest < src) {
        // Copy forwards
        while (*size_moved < size) {
            size_t to_read = std::min<uint64_t>(
                    sizeof(buf), size - *size_moved);

            // Seek to source offset
            ret = file.seek(src + *size_moved, SEEK_SET, nullptr);
            if (ret != FileStatus::OK) {
                return ret;
            }

            // Read data from source
            ret = file_read_fully(file, buf, to_read, &n_read);
            if (ret != FileStatus::OK) {
                return ret;
            } else if (n_read == 0) {
                break;
            }

            // Seek to destination offset
            ret = file.seek(dest + *size_moved, SEEK_SET, nullptr);
            if (ret != FileStatus::OK) {
                return ret;
            }

            // Write data to destination
            ret = file_write_fully(file, buf, n_read, &n_written);
            if (ret != FileStatus::OK) {
                return ret;
            }

            *size_moved += n_written;

            if (n_written < n_read) {
                break;
            }
        }
    } else {
        // Copy backwards
        while (*size_moved < size) {
            size_t to_read = std::min<uint64_t>(
                    sizeof(buf), size - *size_moved);

            // Seek to source offset
            ret = file.seek(src + size - *size_moved - to_read,
                            SEEK_SET, nullptr);
            if (ret != FileStatus::OK) {
                return ret;
            }

            // Read data form source
            ret = file_read_fully(file, buf, to_read, &n_read);
            if (ret != FileStatus::OK) {
                return ret;
            } else if (n_read == 0) {
                break;
            }

            // Seek to destination offset
            ret = file.seek(dest + size - *size_moved - n_read,
                            SEEK_SET, nullptr);
            if (ret != FileStatus::OK) {
                return ret;
            }

            // Write data to destination
            ret = file_write_fully(file, buf, n_read, &n_written);
            if (ret != FileStatus::OK) {
                return ret;
            }

            *size_moved += n_written;

            if (n_written < n_read) {
                // Hit EOF. Subtract bytes beyond EOF that we can't copy
                size -= n_read - n_written;
            }
        }
    }

    return FileStatus::OK;
}

}
