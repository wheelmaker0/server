#ifndef __GZIP_H__
#define __GZIP_H__

#include <sstream>
#include <strstream>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

namespace common{

namespace bio = boost::iostreams;

class Gzip {
public:
    static std::string compress(const std::string &data) {

        std::stringstream compressed;
        std::stringstream origin(data);

        bio::filtering_streambuf<bio::input> out;
        out.push(bio::gzip_compressor(bio::gzip_params(bio::gzip::best_compression)));
        out.push(origin);
        bio::copy(out, compressed);

        return compressed.str();
    }

    static std::string decompress(const std::string &data) {

        std::stringstream compressed(data);
        std::stringstream decompressed;

        bio::filtering_streambuf<bio::input> out;
        out.push(bio::gzip_decompressor());
        out.push(compressed);
        bio::copy(out, decompressed);

        return decompressed.str();
    }

    static std::string decompress(const std::vector<char> &data) {

        std::istrstream compressed(data.data(), data.size());
        std::stringstream decompressed;

        bio::filtering_streambuf<bio::input> out;
        out.push(bio::gzip_decompressor());
        out.push(compressed);
        bio::copy(out, decompressed);

        return decompressed.str();
    } 
};

} //namespace common
#endif // __GZIP_H__