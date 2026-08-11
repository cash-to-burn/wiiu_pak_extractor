#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <bit>

extern "C"
{
    #include "include/libvgmstream.h"
}

struct WavHeader
{
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunk_size;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};

#define MIN_SIZE 1024
#define MAX_SIZE 1024 * 1024 * 100

void extract_wem(const char *infile, const std::filesystem::path &file_name)
{
    libvgmstream_t *lib = libvgmstream_init();
    if (!lib) return;

    // make it ptr
    libvgmstream_config_t config = {};
    config.ignore_loop = true;
    config.ignore_fade = true;
    config.force_sfmt = LIBVGMSTREAM_SFMT_PCM16;
    libvgmstream_setup(lib, &config);

    libstreamfile_t *stream_file = libstreamfile_open_from_stdio(infile);
    int err = libvgmstream_open_stream(lib, stream_file, 0);
    if (err < 0)
    {
        std::cout << "not a valid file\n";
        return;
    }
    libstreamfile_close(stream_file);

    printf("\nFile -> %s\n", file_name.c_str());
    printf("channels: %d\n", lib->format->channels);
    printf("sample rate: %d\n", lib->format->sample_rate);
    printf("codec: %s\n", lib->format->codec_name);
    printf("samples: %d\n", (int32_t)lib->format->stream_samples);

    std::vector <char> pcm_data;
    
    while (!lib->decoder->done)
    {
        err = libvgmstream_render(lib);
        if (err < 0) break;

        char *buf = (char*)lib->decoder->buf;

        pcm_data.insert(pcm_data.end(), buf, buf + lib->decoder->buf_bytes);
    }

    WavHeader header;
    header.num_channels = (uint16_t)lib->format->channels;
    header.sample_rate = (uint32_t)lib->format->sample_rate;
    header.bits_per_sample = 16;
    header.block_align = (uint16_t)(lib->format->channels * 2);
    header.byte_rate = lib->format->sample_rate * header.block_align;
    header.data_size = (uint32_t)pcm_data.size();
    header.chunk_size = 36 + header.data_size;

    std::ofstream file(file_name.string(), std::ios::binary);
    file.write((char*)&header, sizeof(header));
    file.write(pcm_data.data(), pcm_data.size());
    file.close();
    libvgmstream_free(lib);
}



void extract_streams(const std::string &path, const std::filesystem::path &output_path, bool decode)
{
    std::filesystem::create_directories(output_path);

    if (decode)
        std::filesystem::create_directories(output_path / "wav_files");

    std::ifstream pak_file(path, std::ios::binary | std::ios::ate);
    if (!pak_file.is_open())
    {
        std::cerr << "Cant open -> " << path << std::endl;
        return;
    }

    const size_t total_len = (size_t)pak_file.tellg();
    pak_file.seekg(0, std::ios::beg);
    std::cout << "Reading Path -> " << path << " (" << total_len / (1024 * 1024) << " MB" << ")"<< std::endl;

    std::vector<char> buffer(total_len);

    pak_file.read(buffer.data(), total_len);
    pak_file.close();

    std::string_view data(buffer.data(), total_len);
    size_t count = 0;
    size_t pos = 0;

    while (pos + 12 <= total_len)
    {
        size_t idx_riff = data.find("RIFF", pos);
        size_t idx_rifx = data.find("RIFX", pos);

        if (idx_riff == std::string::npos && idx_rifx == std::string::npos)
        {
            break;
        }

        size_t main_idx = std::min(idx_riff, idx_rifx);
        if (main_idx + 12 <= total_len)
        {
            std::string_view wave_fmt = data.substr(main_idx + 8, 4);
            if (wave_fmt == "WAVE" || wave_fmt == "XWMA")
            {
                bool is_be = false;
                if (data.substr(main_idx, 4) == "RIFX") is_be = true;

                uint32_t chunk = 0;
                std::memcpy(&chunk, data.data() + main_idx + 4, sizeof(uint32_t));
                
                if (is_be)
                {
                    chunk = std::byteswap(chunk);
                }

                size_t file_size = (size_t) chunk + 8;

                if (file_size >= MIN_SIZE && file_size <= MAX_SIZE)
                {
                    if (main_idx + file_size <= total_len)
                    {
                        std::ostringstream file_name;
                        file_name << "stream_" << std::setw(4) << std::setfill('0') << count << ".wem";

                        std::ostringstream file_name_wav;
                        file_name_wav << "stream_" << std::setw(4) << std::setfill('0') << count << ".wav";

                        std::filesystem::path out_and_path = output_path / file_name.str();

                        std::ofstream file(out_and_path, std::ios::binary);
                        if (file.is_open())
                        {
                            file.write(data.data() + main_idx, file_size);
                            file.close();

                            if (decode)
                                extract_wem(out_and_path.string().c_str(), (output_path / "wav_files" / file_name_wav.str()));

                            count++;
                            pos = main_idx + file_size;
                            continue;
                        }
                    }
                }
            }
            // if failes sikp it 
            pos = main_idx + 4;
        }
    }
}

void print_help()
{
    std::cout
        << "\n \n"
        << " ---[PAK extractor And Decoder]---"
        << "\n"
        << "I did use it on Bayonetta 2 (wiiu) extracted/sound/pck/All.pck File"
        << "\n"
        << "And It worked Fine, i didnt test it with other files/games so try it by your self"
        << "\n"
        << "How to use it : "
        << "\n"
        << "    ./main -pak file.pak -output output_file -decode (if you wanna decode the wem files)"
        << "\n \n";
}

int main(int argc, char *argv[])
{   
    if (argc < 3)
    {
        print_help();
        return -1;
    }

    std::string path;
    std::filesystem::path output;
    bool decode = false;

    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]) == "-pak")
        {
            path = argv[i + 1];
        }
        if (std::string(argv[i]) == "-output")
        {
            output = argv[i + 1];
        }
        if (std::string(argv[i]) == "-decode")
        {
            decode = true;
        }
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-help")
        {
            print_help();
        }
    }
    
    extract_streams(path, output, decode);
}