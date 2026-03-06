        #include <cmrc/cmrc.hpp>
#include <map>
#include <utility>

namespace cmrc {
namespace insulationData {

namespace res_chars {
// These are the files which are available in this resource library
// Pointers to src/data/insulation_standards/IEC_60664-1.json
extern const char* const f_2f82_src_data_insulation_standards_IEC_60664_1_json_begin;
extern const char* const f_2f82_src_data_insulation_standards_IEC_60664_1_json_end;
// Pointers to src/data/insulation_standards/IEC_60664-4.json
extern const char* const f_0f4e_src_data_insulation_standards_IEC_60664_4_json_begin;
extern const char* const f_0f4e_src_data_insulation_standards_IEC_60664_4_json_end;
// Pointers to src/data/insulation_standards/IEC_60664-5.json
extern const char* const f_7ae3_src_data_insulation_standards_IEC_60664_5_json_begin;
extern const char* const f_7ae3_src_data_insulation_standards_IEC_60664_5_json_end;
// Pointers to src/data/insulation_standards/IEC_62368-1.json
extern const char* const f_5775_src_data_insulation_standards_IEC_62368_1_json_begin;
extern const char* const f_5775_src_data_insulation_standards_IEC_62368_1_json_end;
// Pointers to src/data/insulation_standards/IEC_61558-1.json
extern const char* const f_bfce_src_data_insulation_standards_IEC_61558_1_json_begin;
extern const char* const f_bfce_src_data_insulation_standards_IEC_61558_1_json_end;
// Pointers to src/data/insulation_standards/IEC_61558-2-16.json
extern const char* const f_e059_src_data_insulation_standards_IEC_61558_2_16_json_begin;
extern const char* const f_e059_src_data_insulation_standards_IEC_61558_2_16_json_end;
// Pointers to src/data/insulation_standards/IEC_60335-1.json
extern const char* const f_bcc4_src_data_insulation_standards_IEC_60335_1_json_begin;
extern const char* const f_bcc4_src_data_insulation_standards_IEC_60335_1_json_end;
}

namespace {

const cmrc::detail::index_type&
get_root_index() {
    static cmrc::detail::directory root_directory_;
    static cmrc::detail::file_or_directory root_directory_fod{root_directory_};
    static cmrc::detail::index_type root_index;
    root_index.emplace("", &root_directory_fod);
    struct dir_inl {
        class cmrc::detail::directory& directory;
    };
    dir_inl root_directory_dir{root_directory_};
    (void)root_directory_dir;
    static auto f_25d9_src_dir = root_directory_dir.directory.add_subdir("src");
    root_index.emplace("src", &f_25d9_src_dir.index_entry);
    static auto f_f066_src_data_dir = f_25d9_src_dir.directory.add_subdir("data");
    root_index.emplace("src/data", &f_f066_src_data_dir.index_entry);
    static auto f_15d0_src_data_insulation_standards_dir = f_f066_src_data_dir.directory.add_subdir("insulation_standards");
    root_index.emplace("src/data/insulation_standards", &f_15d0_src_data_insulation_standards_dir.index_entry);
    root_index.emplace(
        "src/data/insulation_standards/IEC_60664-1.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_60664-1.json",
            res_chars::f_2f82_src_data_insulation_standards_IEC_60664_1_json_begin,
            res_chars::f_2f82_src_data_insulation_standards_IEC_60664_1_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_60664-4.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_60664-4.json",
            res_chars::f_0f4e_src_data_insulation_standards_IEC_60664_4_json_begin,
            res_chars::f_0f4e_src_data_insulation_standards_IEC_60664_4_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_60664-5.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_60664-5.json",
            res_chars::f_7ae3_src_data_insulation_standards_IEC_60664_5_json_begin,
            res_chars::f_7ae3_src_data_insulation_standards_IEC_60664_5_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_62368-1.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_62368-1.json",
            res_chars::f_5775_src_data_insulation_standards_IEC_62368_1_json_begin,
            res_chars::f_5775_src_data_insulation_standards_IEC_62368_1_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_61558-1.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_61558-1.json",
            res_chars::f_bfce_src_data_insulation_standards_IEC_61558_1_json_begin,
            res_chars::f_bfce_src_data_insulation_standards_IEC_61558_1_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_61558-2-16.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_61558-2-16.json",
            res_chars::f_e059_src_data_insulation_standards_IEC_61558_2_16_json_begin,
            res_chars::f_e059_src_data_insulation_standards_IEC_61558_2_16_json_end
        )
    );
    root_index.emplace(
        "src/data/insulation_standards/IEC_60335-1.json",
        f_15d0_src_data_insulation_standards_dir.directory.add_file(
            "IEC_60335-1.json",
            res_chars::f_bcc4_src_data_insulation_standards_IEC_60335_1_json_begin,
            res_chars::f_bcc4_src_data_insulation_standards_IEC_60335_1_json_end
        )
    );
    return root_index;
}

}

cmrc::embedded_filesystem get_filesystem() {
    static auto& index = get_root_index();
    return cmrc::embedded_filesystem{index};
}

} // insulationData
} // cmrc
    