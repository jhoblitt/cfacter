/**
 * @file
 * Declares the utility functions for parsing and manipulating strings.
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <initializer_list>
#include <cctype>
#include <cstdint>

namespace facter { namespace util {

    /**
     * Converts the given bytes to a hexadecimal string.
     * @param bytes The pointer to the bytes to convert.
     * @param length The number of bytes to convert.
     * @param uppercase True if the hexadecimal string should be uppercase or false if it should be lowercase.
     * @return Returns the hexadecimal string.
     */
    std::string to_hex(uint8_t const* bytes, size_t length, bool uppercase = false);

    /**
     * Reads each line from the given string.
     * @param s The string to read.
     * @param callback The callback function that is passed each line in the string.
     */
    void each_line(std::string const& s, std::function<bool(std::string&)> callback);

   /**
     * Converts a size, in bytes, to a corresponding string using SI-prefixed units.
     * @param size The size in bytes.
     * @return Returns the size in largest SI unit greater than 1 (e.g. 4.05 GiB, 5.20 MiB, etc).
     */
    std::string si_string(uint64_t size);

    /**
     * Converts an amount used to a percentage.
     * @param used The amount used out of the total.
     * @param total The total amount.
     * @return Returns the percentage (e.g. "41.53%"), to two decimal places, as a string.
     */
    std::string percentage(uint64_t used, uint64_t total);

}}  // namespace facter::util
