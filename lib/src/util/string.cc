#include <facter/util/string.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <limits>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace boost::algorithm;

namespace facter { namespace util {

    string to_hex(uint8_t const* bytes, size_t length, bool uppercase)
    {
        ostringstream ss;
        if (bytes) {
            ss << hex << (uppercase ? std::uppercase : std::nouppercase) << setfill('0');
            for (size_t i = 0; i < length; ++i) {
                ss << setw(2) << static_cast<int>(bytes[i]);
            }
        }
        return ss.str();
    }

    void each_line(string const& s, function<bool(string&)> callback)
    {
        string line;
        istringstream in(s);
        while (getline(in, line)) {
            if (!callback(line)) {
                break;
            }
        }
    }

    string si_string(uint64_t size)
    {
        static char prefixes[] = { 'K', 'M', 'G', 'T', 'P', 'E' };

        if (size < 1024) {
            return to_string(size) + " bytes";
        }
        unsigned int exp = floor(log2(size) / log2(1024.0));
        double converted = round(100.0 * (size / pow(1024.0, exp))) / 100.0;

        // Check to see if rounding up gets us to 1024; if so, move to the next unit
        if (fabs(converted - 1024.0) < numeric_limits<double>::epsilon()) {
            converted = 1.00;
            ++exp;
        }

        // If we exceed the SI prefix (we shouldn't, but just in case), just return the bytes
        if (exp - 1 >= sizeof(prefixes)) {
            return to_string(size) + " bytes";
        }

        ostringstream ss;
        ss  << fixed << setprecision(2) << converted << " " << prefixes[exp - 1] << "iB";
        return ss.str();
    }

    string percentage(uint64_t used, uint64_t total)
    {
        if (total == 0 || used >= total) {
            return "100%";
        }
        if (used == 0) {
            return "0%";
        }
        double converted = round(10000.0 * (used / static_cast<double>(total))) / 100.0;
        // Check to see if we would round up to 100%; if so, keep it at 99.99%
        if (fabs(converted - 100.0) < numeric_limits<double>::epsilon()) {
            converted = 99.99;
        }
        ostringstream ss;
        ss << fixed << setprecision(2) << converted << "%";
        return ss.str();
    }

}}  // namespace facter::util
