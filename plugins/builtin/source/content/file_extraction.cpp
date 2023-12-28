#include <hex/helpers/fs.hpp>

#include <wolv/io/file.hpp>
#include <romfs/romfs.hpp>

namespace hex::plugin::builtin {

    void extractBundledFiles() {
        constexpr static std::array<std::pair<std::string_view, bool>, 2> Paths = {
            {
                { "auto_extract", false },
                { "always_auto_extract", true }
            }
        };

        for (const auto [extractFolder, alwaysExtract] : Paths) {
            for (const auto &romfsPath : romfs::list(extractFolder)) {
                for (const auto &imhexPath : fs::getDataPaths()) {
                    const auto path = imhexPath / std::fs::relative(romfsPath, extractFolder);
                    if (!alwaysExtract && wolv::io::fs::exists(path))
                        continue;

                    wolv::io::File file(path, wolv::io::File::Mode::Create);
                    if (!file.isValid())
                        continue;

                    auto data = romfs::get(romfsPath).span<u8>();
                    file.writeBuffer(data.data(), data.size());

                    if (file.getSize() == data.size())
                        break;
                }

            }
        }
    }

}