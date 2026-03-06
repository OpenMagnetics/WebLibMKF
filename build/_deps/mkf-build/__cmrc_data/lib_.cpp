        #include <cmrc/cmrc.hpp>
#include <map>
#include <utility>

namespace cmrc {
namespace data {

namespace res_chars {
// These are the files which are available in this resource library
// Pointers to MAS/data/cores_stock.ndjson
extern const char* const f_6e69_MAS_data_cores_stock_ndjson_begin;
extern const char* const f_6e69_MAS_data_cores_stock_ndjson_end;
// Pointers to MAS/data/cores.ndjson
extern const char* const f_7219_MAS_data_cores_ndjson_begin;
extern const char* const f_7219_MAS_data_cores_ndjson_end;
// Pointers to MAS/data/core_shapes.ndjson
extern const char* const f_3e23_MAS_data_core_shapes_ndjson_begin;
extern const char* const f_3e23_MAS_data_core_shapes_ndjson_end;
// Pointers to MAS/data/core_materials.ndjson
extern const char* const f_f7fe_MAS_data_core_materials_ndjson_begin;
extern const char* const f_f7fe_MAS_data_core_materials_ndjson_end;
// Pointers to MAS/data/wires.ndjson
extern const char* const f_e0ba_MAS_data_wires_ndjson_begin;
extern const char* const f_e0ba_MAS_data_wires_ndjson_end;
// Pointers to MAS/data/wire_materials.ndjson
extern const char* const f_72d1_MAS_data_wire_materials_ndjson_begin;
extern const char* const f_72d1_MAS_data_wire_materials_ndjson_end;
// Pointers to MAS/data/bobbins.ndjson
extern const char* const f_54e0_MAS_data_bobbins_ndjson_begin;
extern const char* const f_54e0_MAS_data_bobbins_ndjson_end;
// Pointers to MAS/data/insulation_materials.ndjson
extern const char* const f_e867_MAS_data_insulation_materials_ndjson_begin;
extern const char* const f_e867_MAS_data_insulation_materials_ndjson_end;
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
    static auto f_da20_MAS_dir = root_directory_dir.directory.add_subdir("MAS");
    root_index.emplace("MAS", &f_da20_MAS_dir.index_entry);
    static auto f_14d5_MAS_data_dir = f_da20_MAS_dir.directory.add_subdir("data");
    root_index.emplace("MAS/data", &f_14d5_MAS_data_dir.index_entry);
    root_index.emplace(
        "MAS/data/cores_stock.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "cores_stock.ndjson",
            res_chars::f_6e69_MAS_data_cores_stock_ndjson_begin,
            res_chars::f_6e69_MAS_data_cores_stock_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/cores.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "cores.ndjson",
            res_chars::f_7219_MAS_data_cores_ndjson_begin,
            res_chars::f_7219_MAS_data_cores_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/core_shapes.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "core_shapes.ndjson",
            res_chars::f_3e23_MAS_data_core_shapes_ndjson_begin,
            res_chars::f_3e23_MAS_data_core_shapes_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/core_materials.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "core_materials.ndjson",
            res_chars::f_f7fe_MAS_data_core_materials_ndjson_begin,
            res_chars::f_f7fe_MAS_data_core_materials_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/wires.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "wires.ndjson",
            res_chars::f_e0ba_MAS_data_wires_ndjson_begin,
            res_chars::f_e0ba_MAS_data_wires_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/wire_materials.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "wire_materials.ndjson",
            res_chars::f_72d1_MAS_data_wire_materials_ndjson_begin,
            res_chars::f_72d1_MAS_data_wire_materials_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/bobbins.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "bobbins.ndjson",
            res_chars::f_54e0_MAS_data_bobbins_ndjson_begin,
            res_chars::f_54e0_MAS_data_bobbins_ndjson_end
        )
    );
    root_index.emplace(
        "MAS/data/insulation_materials.ndjson",
        f_14d5_MAS_data_dir.directory.add_file(
            "insulation_materials.ndjson",
            res_chars::f_e867_MAS_data_insulation_materials_ndjson_begin,
            res_chars::f_e867_MAS_data_insulation_materials_ndjson_end
        )
    );
    return root_index;
}

}

cmrc::embedded_filesystem get_filesystem() {
    static auto& index = get_root_index();
    return cmrc::embedded_filesystem{index};
}

} // data
} // cmrc
    