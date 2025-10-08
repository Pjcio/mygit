#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <vector>
#include <sstream>
#include <openssl/sha.h>

std::string sha1_hex(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    // Convert the hash to a hexadecimal string
    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}


std::string zlib_decompress(const std::vector<unsigned char>& data) {
    z_stream zs{};                         

    if (inflateInit(&zs) != Z_OK)         
        throw std::runtime_error("inflateInit failed");

    // Zlib vuole Bytef* (unsigned char* non const)
    // Const_cast serve a rimuovere temporaneamente il const
    zs.next_in = const_cast<Bytef*>(data.data()); 

    zs.avail_in = data.size();                    

    char buffer[32768];                   
    // Stringa finale dove accumuliamo i dati decompressi
    std::string out;                  
    int ret; 

    do {
        // Buffer e' char*, zlib vuole Bytef* -> reinterpret_cast
        zs.next_out = reinterpret_cast<Bytef*>(buffer); 

        zs.avail_out = sizeof(buffer);         

        // Chiamata a zlib per decomprimere
        //   - legge da next_in
        //   - scrive in next_out
        //   - aggiorna next_in/avail_in/next_out/avail_out/total_out
        ret = inflate(&zs, 0);              

        if (out.size() < zs.total_out)  
            out.append(buffer, zs.total_out - out.size());
        
    } while (ret == Z_OK);

    inflateEnd(&zs);                 

    if (ret != Z_STREAM_END)           
        throw std::runtime_error("Decompression failed");

    return out;                       
}

std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& data) {
    uLong srcLen = data.size();
    uLong destLen = compressBound(srcLen);

    std::vector<unsigned char> compressed(destLen);

    if (compress(compressed.data(), &destLen, data.data(), srcLen) != Z_OK) {
        throw std::runtime_error("Errore nella compressione zlib");
    }

    compressed.resize(destLen); // ridimensiona al vero size compresso
    return compressed;
}


int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }

    std::string command = argv[1];

    if (command == "init") {
        try {

            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {

                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {

                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
            std::cout << "Initialized git directory\n";

        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if (command == "cat-file") {

        if (argc < 4 || std::string(argv[2]) != "-p") {

            std::cerr << "Usage: cat-file -p <sha>\n";
            return EXIT_FAILURE;
        }
        std::string sha = argv[3];

        std::string dir = ".git/objects/" + sha.substr(0, 2);
        std::string file = dir + "/" + sha.substr(2);

        // opening the file in binary mode because git objects are compressed
        std::ifstream in(file, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Object not found: " << file << '\n';
            return EXIT_FAILURE;
        }
        // 'compressed' will contain all raw (unsigned char) data from the file.
        std::vector<unsigned char> compressed(
            (std::istreambuf_iterator<char>(in)),   //from start
            std::istreambuf_iterator<char>()      //to end
        );
        in.close();

        try {
            std::string object_str = zlib_decompress(compressed);

            // Find the first null byte separator
            auto null_pos = object_str.find('\0');
            if (null_pos == std::string::npos) {
                std::cerr << "Invalid Git object format\n";
                return EXIT_FAILURE;
            }
            // everything avter the null byte is the actual content
            std::string object_content = object_str.substr(null_pos + 1);
            std::cout << object_content << std::flush;

        } catch (const std::exception& e) {
            std::cerr << "Decompression error: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if (command == "hash-object") {

        if (argc < 3) {
            std::cerr << "Usage: hash-object [options] <file>\n";
            return EXIT_FAILURE;
        }

        if (std::string(argv[2]) == "-w") {
            if (argc < 4) {
                std::cerr << "Usage: hash-object -w <file>\n";
                return EXIT_FAILURE;
            }
        }

        std::string filename = argv[argc - 1];
        std::ifstream in(filename, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Object not found: " << filename << '\n';
            return EXIT_FAILURE;
        }

        std::vector<unsigned char> file_data(
            (std::istreambuf_iterator<char>(in)),   //from start
            std::istreambuf_iterator<char>()      //to end
        );
        in.close();

        // Git object format: "<type> <size>\0<content>"
        // Here we only handle "blob" type
        std::string header = "blob " + std::to_string(file_data.size()) + '\0';
        std::string final_data = header + std::string(file_data.begin(), file_data.end());

        // Compute SHA-1 hash
        std::string sha = sha1_hex(final_data);
        std::cout << sha << '\n';

        if (std::string(argv[2]) == "-w") {
            // Compress the final_data using zlib
            std::string path = ".git/objects/" + sha.substr(0, 2);
            std::string filename = sha.substr(2);

            std::filesystem::create_directory(path);

            try {
                // Compress the final_data using zlib
                std::vector<unsigned char> compressed = zlib_compress(
                    std::vector<unsigned char>(final_data.begin(), final_data.end())
                );

                std::ofstream out(path + "/" + filename, std::ios::binary);
                if (!out.is_open()) {
                    std::cerr << "Failed to create object file: " << path + "/" + filename << '\n';
                    return EXIT_FAILURE;
                }
                out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
                out.close();

            } catch (const std::exception& e) {
                std::cerr << "Compression error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }
    } else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
